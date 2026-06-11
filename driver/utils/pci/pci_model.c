/*===================================================================
 * Copyright(c) AVerMedia TECHNOLOGIES, Inc. 2017
 * All rights reserved
 * =================================================================
 * pci_model.c
 *
 *  Created on: Apr 15, 2017
 *      Author: 
 *      Version:
 * =================================================================
 */
 
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/preempt.h>

/* Compat: PCI_IRQ_INTX was introduced in kernel 6.8 as a rename of
 * PCI_IRQ_LEGACY. Provide a fallback for older kernels. */
#ifndef PCI_IRQ_INTX
#define PCI_IRQ_INTX PCI_IRQ_LEGACY
#endif

#include "cxt_mgr.h"
#include "mem_model.h"
#include "pci_model.h"
#include "alsa_model.h"
#include "debug.h"

typedef struct
{
    pci_model_flags_e flags;
    unsigned long driver_data;
}pci_model_driver_data_t;

typedef struct 
{
    BASIC_CXT_HANDLE_DECLARE;
    struct pci_driver driver;
    pci_model_probe_func_t probe_func;
    pci_model_suspend_func_t suspend_func;
    pci_model_resume_func_t resume_func;
    pci_model_remove_func_t remove_func;
    struct pci_device_id *id_table;
    pci_model_driver_data_t driver_data;
}pci_model_driver_cxt_t;

typedef struct
{
    u64 phys_addr;
    u64 size;
    u64 __iomem *mmio;
}bar_info_t;

#define MAX_BAR_COUNT 6

typedef struct
{
    BASIC_CXT_HANDLE_DECLARE;
    struct pci_dev *pci_dev;
    pci_model_irq_func_t irq_func;
    void *irq_func_cxt;
    int bar_count;
    bar_info_t bar_info[MAX_BAR_COUNT];
    int msi_enabled;
    /* V-DESC intercept hook (reg 0x10 bit 0x2): called from hard IRQ
     * context BEFORE the vendor blob's irq_func sees the interrupt.
     * Used to manipulate the just-completed frame buffer at the
     * hardware heartbeat (byte-order fix experiments). */
    pci_model_vdesc_hook_t vdesc_hook;
    void *vdesc_hook_cxt;
    /* I2C engine IRQ (bit 0x800 in reg 0x10): latched status + completion
     * so synchronous I2C polling paths can sleep instead of spinning.
     *
     * IMPORTANT: the ISR only ACKs bit 0x800 while i2c_waiters > 0.
     * The vendor blob's synchronous I2C code polls reg 0x10 for that very
     * bit; unconditional ACK in the ISR cleared it before the blob's poll
     * loop could see it → infinite spin → insmod freeze. */
    atomic_t i2c_done_count;
    atomic_t i2c_waiters;
    atomic_t irq_ready;
    u32 last_i2c_status;
    struct completion i2c_done;
}pci_model_cxt_t;

static pci_model_driver_cxt_t *pci_model_drv_cxt=NULL;
static void *pci_model_alloc(void);
static void pci_model_release(void *cxt);

int subsystem_id;
static int user_disable_msi;

static void *pci_model_alloc()
{
    pci_model_cxt_t *pci_cxt=mem_model_alloc_buffer(sizeof(pci_model_cxt_t));

    if (pci_cxt) {
        atomic_set(&pci_cxt->i2c_done_count, 0);
        atomic_set(&pci_cxt->i2c_waiters, 0);
        atomic_set(&pci_cxt->irq_ready, 0);
        init_completion(&pci_cxt->i2c_done);
    }
    return pci_cxt;
}

static void pci_model_release(void *cxt)
{
	pci_model_cxt_t *pci_cxt=cxt;
        mesg_debug("%s\n",__func__);

	
	if(pci_cxt)
	{
            
            mem_model_free_buffer(pci_cxt);
	}
}

/* DIAG (Windows driver audit 2026-06-11): BAR0 reg 0x10 is the central
 * IRQ status/ACK register. Known bits:
 *   0x001 video termination     0x002 V-Desc complete (frame done)
 *   0x010 audio termination     0x020 A-Desc complete (audio)
 *   0x100 audio termination 2   0x200 AM-Desc complete (audio 2)
 *   0x800 I2C engine complete (status latch in reg 0x1a4)
 * The I2C bit is ACKed here (the blob never clears it → IRQ spam /
 * deadlocked synchronous polling). All other bits are left for the
 * registered handler/blob so the video DMA chain stays intact. */
#define CX511H_IRQ_STATUS_REG     0x10
#define CX511H_IRQ_STATUS_MASK    0x1fff
#define CX511H_IRQ_VDESC_DONE     0x002
#define CX511H_IRQ_I2C_DONE       0x800
#define CX511H_VDESC_INDEX_REG    0x300
#define CX511H_I2C_STATUS_REG     0x1a4
#define CX511H_IRQ_LOG_BUDGET     64
#define CX511H_IRQ_I2C_LOG_BUDGET 8
#define CX511H_BYPASS_LOG_BUDGET  32

static void pci_model_irq_diag(u32 irq_status)
{
    /* Separate budgets: continuous I2C-complete spam must not starve the
     * interesting video/audio bits out of the log window. */
    static atomic_t log_budget     = ATOMIC_INIT(CX511H_IRQ_LOG_BUDGET);
    static atomic_t i2c_log_budget = ATOMIC_INIT(CX511H_IRQ_I2C_LOG_BUDGET);
    bool i2c_only = (irq_status & CX511H_IRQ_STATUS_MASK) == CX511H_IRQ_I2C_DONE;
    atomic_t *budget = i2c_only ? &i2c_log_budget : &log_budget;

    if (!atomic_add_unless(budget, -1, 0)) {
        if (!i2c_only)
            printk_ratelimited(KERN_ERR
                "[cx511h-irq] RAW STATUS: 0x%08X (ratelimited; budget spent)\n",
                irq_status);
        return;
    }

    printk(KERN_ERR "[cx511h-irq] RAW STATUS: 0x%08X%s%s%s%s%s%s%s\n",
           irq_status,
           (irq_status & 0x002) ? " [V-DESC complete]"   : "",
           (irq_status & 0x001) ? " [video term]"        : "",
           (irq_status & 0x020) ? " [A-DESC complete]"   : "",
           (irq_status & 0x010) ? " [audio term]"        : "",
           (irq_status & 0x200) ? " [AM-DESC complete]"  : "",
           (irq_status & 0x100) ? " [audio term 2]"      : "",
           (irq_status & 0x800) ? " [I2C complete]"      : "");
}

static irqreturn_t pci_model_irq(int irq, void *dev_id)
{
    int handled=0;
    pci_model_cxt_t *pci_cxt=dev_id;
    u32 __iomem *mmio = (u32 __iomem *)pci_cxt->bar_info[0].mmio;
    bool vdesc_fired = false;

    if (mmio) {
        u32 irq_status = readl(&mmio[CX511H_IRQ_STATUS_REG >> 2]);

        /* 0xffffffff = device gone; 0 in masked bits = not ours (shared INTx). */
        if (irq_status != 0xffffffffu &&
            (irq_status & CX511H_IRQ_STATUS_MASK)) {

            pci_model_irq_diag(irq_status);

            /* === AGGRESSIVE IRQ BYPASS (V-DESC intercept) ===
             * Grab the completed frame BEFORE the vendor blob's
             * irq_func can touch/drop it. Slot index per Windows
             * audit: reg 0x300 & 7. ACK of bit 0x2 stays with the
             * blob path; see the post-irq_func safety ACK below. */
            if (irq_status & CX511H_IRQ_VDESC_DONE) {
                static atomic_t bypass_log_budget =
                    ATOMIC_INIT(CX511H_BYPASS_LOG_BUDGET);
                int slot_index =
                    (int)(readl(&mmio[CX511H_VDESC_INDEX_REG >> 2]) & 7);

                vdesc_fired = true;

                if (atomic_add_unless(&bypass_log_budget, -1, 0))
                    printk(KERN_ERR
                        "[cx511h-bypass] V-DESC INTERCEPTED! Slot Index: %d\n",
                        slot_index);

                if (pci_cxt->vdesc_hook)
                    pci_cxt->vdesc_hook(pci_cxt->vdesc_hook_cxt, slot_index);
            }

            /* ACK bit 0x800 ONLY when someone armed a wait via
             * pci_model_wait_i2c_done(). The blob's synchronous I2C
             * code polls this bit itself; clearing it unconditionally
             * starves that poll loop and freezes insmod/probe. */
            if ((irq_status & CX511H_IRQ_I2C_DONE) &&
                atomic_read(&pci_cxt->i2c_waiters) > 0) {
                /* Latch engine status first (Windows ISR reads 0x1a4
                 * before ACK), then clear the IRQ source. */
                pci_cxt->last_i2c_status =
                    readl(&mmio[CX511H_I2C_STATUS_REG >> 2]);
                writel(CX511H_IRQ_I2C_DONE,
                       &mmio[CX511H_IRQ_STATUS_REG >> 2]);

                atomic_inc(&pci_cxt->i2c_done_count);
                complete(&pci_cxt->i2c_done);
                handled = 1;
            }
        }
    }

    if(pci_cxt->irq_func)
    {
        handled |= pci_cxt->irq_func(pci_cxt->irq_func_cxt);
    }    

    /* Safety ACK "if necessary": the blob/board path normally ACKs
     * bit 0x2 itself (0x10 ← 0x2). If it didn't — bit still set after
     * irq_func — clear it here so the IRQ line doesn't wedge. Same
     * lesson as the I2C freeze: never steal an ACK the blob expects,
     * only mop up what nobody claimed. */
    if (vdesc_fired && mmio) {
        u32 post_status = readl(&mmio[CX511H_IRQ_STATUS_REG >> 2]);

        if (post_status != 0xffffffffu &&
            (post_status & CX511H_IRQ_VDESC_DONE)) {
            writel(CX511H_IRQ_VDESC_DONE,
                   &mmio[CX511H_IRQ_STATUS_REG >> 2]);
            printk_ratelimited(KERN_ERR
                "[cx511h-bypass] V-DESC ACK fallback (blob left 0x2 set)\n");
            handled = 1;
        }
    }
    
    return IRQ_RETVAL(handled);
}

void pci_model_register_vdesc_hook(pci_model_handle_t handle,
                                   pci_model_vdesc_hook_t hook, void *cxt)
{
    pci_model_cxt_t *pci_cxt = handle;

    if (!pci_cxt)
        return;
    /* Context first so the ISR never sees a hook with stale context. */
    pci_cxt->vdesc_hook_cxt = cxt;
    smp_wmb();
    pci_cxt->vdesc_hook = hook;
}

/* === I2C IRQ notification API ===
 * Lets I2C transaction code sleep on the completion instead of
 * synchronously polling the engine (the freeze/deadlock path). */

unsigned pci_model_i2c_done_count(pci_model_handle_t handle)
{
    pci_model_cxt_t *pci_cxt = handle;

    if (!pci_cxt)
        return 0;
    return (unsigned)atomic_read(&pci_cxt->i2c_done_count);
}

U32_T pci_model_last_i2c_status(pci_model_handle_t handle)
{
    pci_model_cxt_t *pci_cxt = handle;

    if (!pci_cxt)
        return 0;
    return pci_cxt->last_i2c_status;
}

/* Hard upper bound — an I2C transaction at 100 kHz takes ~1ms; anything
 * beyond 50ms means the engine is wedged and waiting longer only stalls
 * the caller. Never freeze the kernel thread over a dead I2C engine. */
#define CX511H_I2C_WAIT_MAX_MS 50

/* Poll fallback: directly watch reg 0x10 bit 0x800 without sleeping.
 * Used when IRQs aren't live yet (probe phase) or the caller is in
 * atomic context. Mirrors the blob's own polling, incl. status latch
 * and ACK on success. */
static int pci_model_poll_i2c_done(pci_model_cxt_t *pci_cxt,
                                   unsigned timeout_ms)
{
    u32 __iomem *mmio = (u32 __iomem *)pci_cxt->bar_info[0].mmio;
    unsigned waited;

    if (!mmio)
        return -ENODEV;

    for (waited = 0; waited <= timeout_ms; waited++) {
        u32 irq_status = readl(&mmio[CX511H_IRQ_STATUS_REG >> 2]);

        if (irq_status == 0xffffffffu)
            return -ENODEV;
        if (irq_status & CX511H_IRQ_I2C_DONE) {
            pci_cxt->last_i2c_status =
                readl(&mmio[CX511H_I2C_STATUS_REG >> 2]);
            writel(CX511H_IRQ_I2C_DONE,
                   &mmio[CX511H_IRQ_STATUS_REG >> 2]);
            atomic_inc(&pci_cxt->i2c_done_count);
            return 0;
        }
        mdelay(1);
    }
    return -ETIMEDOUT;
}

int pci_model_wait_i2c_done(pci_model_handle_t handle, unsigned timeout_ms)
{
    pci_model_cxt_t *pci_cxt = handle;
    int ret;

    if (!pci_cxt)
        return -EINVAL;

    if (timeout_ms == 0 || timeout_ms > CX511H_I2C_WAIT_MAX_MS)
        timeout_ms = CX511H_I2C_WAIT_MAX_MS;

    /* Sleeping is forbidden in atomic context and pointless before the
     * IRQ handler is live (probe phase) — poll the register instead. */
    if (!atomic_read(&pci_cxt->irq_ready) ||
        in_interrupt() || irqs_disabled() || !preemptible()) {
        ret = pci_model_poll_i2c_done(pci_cxt, timeout_ms);
        if (ret == -ETIMEDOUT)
            printk_ratelimited(KERN_WARNING
                "[cx511h-i2c] poll-wait timed out after %ums (irq_ready=%d)\n",
                timeout_ms, atomic_read(&pci_cxt->irq_ready));
        return ret;
    }

    /* IRQ path: arm the waiter so the ISR ACKs bit 0x800 for us. */
    if (atomic_inc_return(&pci_cxt->i2c_waiters) == 1)
        reinit_completion(&pci_cxt->i2c_done);

    if (!wait_for_completion_timeout(&pci_cxt->i2c_done,
                                     msecs_to_jiffies(timeout_ms)))
        ret = -ETIMEDOUT;
    else
        ret = 0;

    atomic_dec(&pci_cxt->i2c_waiters);

    if (ret == -ETIMEDOUT) {
        printk_ratelimited(KERN_WARNING
            "[cx511h-i2c] IRQ-wait timed out after %ums — falling back to poll\n",
            timeout_ms);
        /* One last short poll in case the IRQ was lost/raced. */
        ret = pci_model_poll_i2c_done(pci_cxt, 2);
    }

    return ret;
}

static u16 pci_get_subsystem(struct pci_dev *pdev)
{
    u16 sub_id;

    pci_read_config_word(pdev, PCI_SUBSYSTEM_ID	, &sub_id);
    return sub_id;
}
 
static int pci_model_probe(struct pci_dev *pci_dev,const struct pci_device_id *pci_id)
{
    int ret=0;
    struct device *dev=&pci_dev->dev;
    cxt_mgr_handle_t cxt_mgr=cxt_manager_alloc(dev);
    pci_model_cxt_t *pci_cxt=NULL;
    int i;
    pci_model_driver_data_t *driver_data=(pci_model_driver_data_t *)pci_id->driver_data;
    pci_model_flags_e flags=driver_data->flags;
    int sub_system;
    
    enum
    {
        NO_ERROR=0,
        NO_DRV_CXT,
        NO_CXT_MGR,
        ERROR_ALLOC_CXT,
        ERROR_ENABLE_PCI_DEV,
        ERROR_REQUEST_REGIONS,
        ERROR_ENABLE_MSI,
        ERROR_SET_DMA_MASK,
        ERROR_REQUEST_IRQ,
    }err=NO_ERROR;

    printk("pci_model_probe prepare\n");
    do
    {
        if(!pci_model_drv_cxt)
        {
            err=NO_DRV_CXT;
            break;
        }
        if(!cxt_mgr)
        {
            err=NO_CXT_MGR;
            break;
        }
        dev_set_drvdata(dev,cxt_mgr);
        pci_cxt=cxt_manager_add_cxt(cxt_mgr,PCI_CXT_ID,pci_model_alloc,pci_model_release);
        if(!pci_cxt)
        {
            err=ERROR_ALLOC_CXT;
            break;
        }
        pci_cxt->pci_dev=pci_dev;
        {
            int enable_ret = pci_enable_device(pci_dev);
            printk(KERN_ERR "[cx511h-debug] pci_enable_device returned: %d, irq=%u\n",
                   enable_ret, pci_dev->irq);
            if(enable_ret)
            {
                err=ERROR_ENABLE_PCI_DEV;
                break;
            }
        }
        pci_set_master(pci_dev);
        printk(KERN_ERR "[cx511h-debug] pci_set_master done, irq=%u\n", pci_dev->irq);
        if(pci_request_regions(pci_dev,pci_name(pci_dev)))
        {
            err=ERROR_REQUEST_REGIONS;
            break;
            
        }
        if( !(flags & PCI_MODEL_FORCE_DMA_32) &&  pci_find_capability(pci_dev,PCI_CAP_ID_EXP))
        {
            mesg_debug("PCI Express\n");
            
            if(!dma_set_mask(&pci_dev->dev, DMA_BIT_MASK(64)))
            {
                mesg_info("set 64bit DMA mask\n");
            }else 
                if(!dma_set_mask(&pci_dev->dev, DMA_BIT_MASK(32)))
            {
                mesg_info("set 32bit DMA mask\n");
            }else
            {
                
                mesg_err("No suitable DMA available\n");
                err=ERROR_SET_DMA_MASK;
                break;
            }
        }else
        {
            if(!dma_set_mask(&pci_dev->dev, DMA_BIT_MASK(32)))
            {
                mesg_info("set 32bit DMA mask\n");
                
            }else
            {
                mesg_err("No suitable DMA available\n");
                err=ERROR_SET_DMA_MASK;
                break;
            }   
        }
        if(err!=NO_ERROR)
            break;
        
        user_disable_msi = flags & PCI_MODEL_DISABLE_MSI;
        pci_cxt->msi_enabled = false;

        printk(KERN_ERR "[cx511h-debug] flags=0x%x user_disable_msi=%d irq_before_alloc=%u\n",
               flags, user_disable_msi, pci_dev->irq);

        /* Modern IRQ allocation: try MSI first, fall back to legacy INTx.
         * The old code treated pci_enable_msi() failure as fatal, which
         * broke IRQ registration on modern Intel platforms entirely. */
        if (!user_disable_msi) {
            int irq_vectors;
            irq_vectors = pci_alloc_irq_vectors(pci_dev, 1, 1,
                                                 PCI_IRQ_MSI | PCI_IRQ_INTX);
            printk(KERN_ERR "[cx511h-debug] pci_alloc_irq_vectors returned: %d\n",
                   irq_vectors);
            if (irq_vectors >= 1) {
                int irq_num = pci_irq_vector(pci_dev, 0);
                pci_cxt->msi_enabled = pci_dev->msi_enabled;
                printk(KERN_ERR "[cx511h-debug] IRQ allocated: irq=%d msi=%s\n",
                       irq_num,
                       pci_cxt->msi_enabled ? "yes" : "no (legacy)");
                if (request_irq(irq_num, pci_model_irq,
                                pci_cxt->msi_enabled ? 0 : IRQF_SHARED,
                                pci_name(pci_dev), pci_cxt) < 0)
                {
                    printk(KERN_ERR "[cx511h-debug] request_irq FAILED for irq %d\n",
                           irq_num);
                    pci_free_irq_vectors(pci_dev);
                    err = ERROR_REQUEST_IRQ;
                    break;
                }
                printk(KERN_ERR "[cx511h-debug] request_irq SUCCESS for irq %d\n",
                       irq_num);
                atomic_set(&pci_cxt->irq_ready, 1);
            } else {
                printk(KERN_ERR "[cx511h-debug] pci_alloc_irq_vectors FAILED (%d), "
                       "trying direct request_irq on irq %u\n",
                       irq_vectors, pci_dev->irq);
                /* Last resort: try direct request_irq with the raw PCI irq */
                if (request_irq(pci_dev->irq, pci_model_irq, IRQF_SHARED,
                                pci_name(pci_dev), pci_cxt) < 0)
                {
                    printk(KERN_ERR "[cx511h-debug] direct request_irq ALSO FAILED\n");
                    err = ERROR_REQUEST_IRQ;
                    break;
                }
                printk(KERN_ERR "[cx511h-debug] direct request_irq SUCCESS on irq %u\n",
                       pci_dev->irq);
                atomic_set(&pci_cxt->irq_ready, 1);
            }
        } else {
            printk(KERN_ERR "[cx511h-debug] MSI disabled by flags, using legacy irq %u\n",
                   pci_dev->irq);
            /* MSI explicitly disabled by driver flags — use legacy only */
            if (request_irq(pci_dev->irq, pci_model_irq, IRQF_SHARED,
                            pci_name(pci_dev), pci_cxt) < 0)
            {
                err = ERROR_REQUEST_IRQ;
                break;
            }
            atomic_set(&pci_cxt->irq_ready, 1);
        }


        if(err!=NO_ERROR)
            break;
        
        for(i=0;i<MAX_BAR_COUNT && pci_resource_len(pci_dev,i)!=0 ;i++)
        {
            bar_info_t *bar_info=&pci_cxt->bar_info[i];
            //unsigned int phys_addr=pci_resource_start(pci_dev,i);
            //unsigned int size=pci_resource_len(pci_dev,i);
            u64 phys_addr=pci_resource_start(pci_dev,i);
            u64 size=pci_resource_len(pci_dev,i);
            void *mmio=NULL;
            
            if((mmio=ioremap(phys_addr,size)))
            {
                bar_info->phys_addr=phys_addr;
                bar_info->size=size;
                bar_info->mmio=mmio;
                /* FIX: only increment bar_count on successful ioremap */
                pci_cxt->bar_count++;
                mesg_debug("%s ioremap %08x size %x to %p\n",__func__,phys_addr,size,mmio);
            }else
            {
                mesg_err("%s ioremap %08x size %x error\n",__func__,phys_addr,size);
            }
        }
        
        sub_system = pci_get_subsystem(pci_dev);
        
        subsystem_id = sub_system;
        
        printk("%s sub_id=%x\n",__func__,sub_system);
           
        if(pci_model_drv_cxt->probe_func)
        {
            ret=pci_model_drv_cxt->probe_func(dev,driver_data->driver_data);
            printk("board_probe=%d\n",ret);
		}
    
        
    }while(0);
    if(err!=NO_ERROR)
    {
        mesg_err("%s error %d\n",__func__,err);
        printk(KERN_ERR "pci_model_probe fail\n");
        switch(err)
        {      
            case ERROR_REQUEST_IRQ:
                /* FIX: clean up IRQ vectors allocated before the failure */
                pci_free_irq_vectors(pci_dev);
                // fall through
            case ERROR_SET_DMA_MASK:
            case ERROR_ENABLE_MSI:
                /* FIX: undo pci_set_master and pci_enable_device on these paths */
                pci_clear_master(pci_dev);
                pci_disable_device(pci_dev);
            case ERROR_REQUEST_REGIONS:
                pci_clear_master(pci_dev);
                pci_disable_device(pci_dev);
            case ERROR_ENABLE_PCI_DEV:
                cxt_manager_unref_context(pci_cxt);
                pci_cxt=NULL;
            case ERROR_ALLOC_CXT:
            case NO_CXT_MGR:
            case NO_DRV_CXT:
                break;
            default:
                pci_release_regions(pci_dev);
                break;
        }
    }
    
    
    return ret;
}

static void pci_model_remove(struct pci_dev *pci_dev)
{
    struct device *dev=&pci_dev->dev;
    cxt_mgr_handle_t cxt_mgr=get_cxt_manager(dev);
    pci_model_cxt_t *pci_cxt = NULL;
    int i;
    mesg_debug("%s\n",__func__);
    
    // --- FIX --- Call board remove for software cleanup first
    if(pci_model_drv_cxt && pci_model_drv_cxt->remove_func)
        pci_model_drv_cxt->remove_func(dev);
    
    if(cxt_mgr)
    {
        // --- FIX --- Get PCI context BEFORE releasing manager
        pci_cxt = cxt_manager_get_context(cxt_mgr, PCI_CXT_ID, 0);
        if (pci_cxt)
        {
            // --- FIX --- Free IRQ while context is still valid
            atomic_set(&pci_cxt->irq_ready, 0);
            if (pci_cxt->msi_enabled)
            {
                free_irq(pci_irq_vector(pci_dev, 0), pci_cxt);
                pci_cxt->msi_enabled = false;
            }
            else
            {
                free_irq(pci_dev->irq, pci_cxt);
            }
            // --- FIX --- Always free IRQ vectors if they were allocated
            pci_free_irq_vectors(pci_dev);
            
            // --- FIX --- Unmap BARs while context is still valid
            for(i = 0; i < pci_cxt->bar_count; i++)
            {
                if (pci_cxt->bar_info[i].mmio)
                    iounmap(pci_cxt->bar_info[i].mmio);
            }
        }
        
        // --- FIX --- Now release context manager (frees pci_cxt memory)
        cxt_manager_release(cxt_mgr);
    }
    
    // --- FIX --- PCI cleanup after all context is cleaned up
    pci_release_regions(pci_dev);
    pci_clear_master(pci_dev);
    pci_disable_device(pci_dev);
        
}

static int pci_model_suspend (struct pci_dev *pci_dev, pm_message_t state)
{
//    int ret=0;
    struct device *dev=&pci_dev->dev;
    cxt_mgr_handle_t cxt_mgr=get_cxt_manager(dev);
    pci_model_cxt_t *pci_cxt = cxt_manager_get_context(cxt_mgr,PCI_CXT_ID,0);

    mesg_debug("%s\n",__func__);

    if (cxt_mgr)
    {
        alsa_model_suspend(cxt_manager_get_context(cxt_mgr,ALSA_CXT_ID,0));

        pci_model_drv_cxt->suspend_func(dev);

        atomic_set(&pci_cxt->irq_ready, 0);
        free_irq(pci_irq_vector(pci_dev, 0), pci_cxt);
        pci_free_irq_vectors(pci_dev);
        pci_cxt->msi_enabled = false;
    }

    pci_disable_device(pci_dev);
    pci_save_state(pci_dev);
    pci_set_power_state(pci_dev, pci_choose_state(pci_dev, state));

    return 0;
}

static int pci_model_resume(struct pci_dev *pci_dev)
{
    int ret = 0;
    struct device *dev=&pci_dev->dev;
    cxt_mgr_handle_t cxt_mgr=get_cxt_manager(dev);
    pci_model_cxt_t *pci_cxt = cxt_manager_get_context(cxt_mgr,PCI_CXT_ID,0);

    mesg_debug("%s\n",__func__);

    pci_set_power_state(pci_dev, PCI_D0);
    pci_restore_state(pci_dev);

    ret = pci_enable_device(pci_dev);
    if (ret)
    {
        mesg_debug("pci model resume failed");
        return ret;
    }

    pci_cxt->msi_enabled = false;
    {
        int irq_vectors;
        irq_vectors = pci_alloc_irq_vectors(pci_dev, 1, 1,
                                             PCI_IRQ_MSI | PCI_IRQ_INTX);
        if (irq_vectors >= 1) {
            pci_cxt->msi_enabled = pci_dev->msi_enabled;
            if (request_irq(pci_irq_vector(pci_dev, 0), pci_model_irq,
                            pci_cxt->msi_enabled ? 0 : IRQF_SHARED,
                            pci_name(pci_dev), pci_cxt) < 0)
            {
                pci_free_irq_vectors(pci_dev);
                /* FIX: disable and clear PCI device before returning on failure */
                pci_disable_device(pci_dev);
                pci_clear_master(pci_dev);
                return -1;
            }
            atomic_set(&pci_cxt->irq_ready, 1);
        } else {
            /* FIX: disable and clear PCI device before returning on failure */
            pci_disable_device(pci_dev);
            pci_clear_master(pci_dev);
            return -1;
        }
    }

    pci_set_master(pci_dev);

    pci_model_drv_cxt->resume_func(dev);

    alsa_model_resume(cxt_manager_get_context(cxt_mgr,ALSA_CXT_ID,0));

    return 0;
}

static __attribute__ ((unused)) void pci_model_shutdown(struct pci_dev *pci_dev)
{
    
}

pci_model_handle_t pci_model_get_handle(cxt_mgr_handle_t cxt_mgr)   
{

	pci_model_cxt_t *pci_cxt=NULL;

	if(cxt_mgr)
	{
		pci_cxt=cxt_manager_get_context(cxt_mgr,PCI_CXT_ID,0);
		if(pci_cxt)
			return pci_cxt;
	}
	return NULL;
}

u32 pci_model_mmio_read(pci_model_handle_t handle,int index,unsigned offset)
{
    pci_model_cxt_t *pci_cxt=handle;
    u32 __iomem *mmio;
    u32 ret=0;
    
    do
    {
        if(!pci_cxt)
            break;
        if(offset & 0x3)
        {
            mesg_err("%s offset %x no 32 bit align\n",__func__,offset);
            break;
        }
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u32 __iomem *)pci_cxt->bar_info[index].mmio;
        ret=readl(&mmio[offset>>2]);
    }while(0);
    
    return ret;
}

u16 pci_model_mmio_readw(pci_model_handle_t handle,int index,unsigned offset)
{
    pci_model_cxt_t *pci_cxt=handle;
    u16 __iomem *mmio;
    u16 ret=0;
    
    do
    {
        if(!pci_cxt)
            break;
        if(offset & 0x1)
        {
            mesg_err("%s offset %x no 16 bit align\n",__func__,offset);
            break;
        }
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u16 __iomem *)pci_cxt->bar_info[index].mmio;
        ret=readw(&mmio[offset>>1]);
    }while(0);
    
    return ret;
}

u8 pci_model_mmio_readb(pci_model_handle_t handle,int index,unsigned offset)
{
    pci_model_cxt_t *pci_cxt=handle;
    u8 __iomem *mmio;
    u8 ret=0;
    
    do
    {
        if(!pci_cxt)
            break;
      
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u8 __iomem *)pci_cxt->bar_info[index].mmio;
        ret=readb(&mmio[offset]);
    }while(0);
    
    return ret;
}

void pci_model_mmio_write(pci_model_handle_t handle,int index,unsigned offset,u32 value)
{
    pci_model_cxt_t *pci_cxt=handle;
    u32 __iomem *mmio;

    
    do
    {
        if(!pci_cxt)
            break;
        if(offset & 0x3)
        {
            mesg_err("%s offset %x no 32 bit align\n",__func__,offset);
            break;
        }
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u32 __iomem *)pci_cxt->bar_info[index].mmio;
        
        writel(value,&mmio[offset>>2]);
    }while(0);
}

void pci_model_mmio_writew(pci_model_handle_t handle,int index,unsigned offset,u16 value)
{
    pci_model_cxt_t *pci_cxt=handle;
    u16 __iomem *mmio;

    
    do
    {
        if(!pci_cxt)
            break;
        if(offset & 0x1)
        {
            mesg_err("%s offset %x no 16 bit align\n",__func__,offset);
            break;
        }
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u16 __iomem *)pci_cxt->bar_info[index].mmio;
        writew(value,&mmio[offset>>1]);
    }while(0);
}


void pci_model_mmio_writeb(pci_model_handle_t handle,int index,unsigned offset,u8 value)
{
    pci_model_cxt_t *pci_cxt=handle;
    u8 __iomem *mmio;

    
    do
    {
        if(!pci_cxt)
            break;
        if(index <0 || (index>pci_cxt->bar_count))
        {
            mesg_err("%s access wrong index %d\n",__func__,index);
            break;
        }
        mmio=(u8 __iomem *)pci_cxt->bar_info[index].mmio;
        writeb(value,&mmio[offset]);
    }while(0);
}





void pci_model_register_isr(pci_model_handle_t handle,pci_model_irq_func_t irq_func,void *data)
{
    pci_model_cxt_t *pci_cxt=handle;
    
    if(pci_cxt)
    {
        if(pci_cxt->irq_func)
        {
            mesg_err("Another ISR already registered\n");
            return;
        }
        pci_cxt->irq_func= irq_func;
        pci_cxt->irq_func_cxt=data;
    }
}
 

int pci_model_driver_init(pci_model_driver_setup_t *pcidrv_setup)
{
	int err; 
	printk("pci_model_driver_init\n");
    if(pci_model_drv_cxt==NULL)
    {
        pci_model_drv_cxt=mem_model_alloc_buffer(sizeof(pci_model_driver_cxt_t));
        if(pci_model_drv_cxt)
        {
            struct pci_driver *driver=&pci_model_drv_cxt->driver;
            
            driver->name=pcidrv_setup->name;
            driver->probe=pci_model_probe;
            driver->remove=pci_model_remove;
            driver->suspend=pci_model_suspend;
            driver->resume=pci_model_resume;
//            driver->shutdown=pci_model_shutdown;

            pci_model_drv_cxt->probe_func=pcidrv_setup->prob_func;
            pci_model_drv_cxt->remove_func=pcidrv_setup->remove_func;
            pci_model_drv_cxt->suspend_func=pcidrv_setup->suspend_func;
            pci_model_drv_cxt->resume_func=pcidrv_setup->resume_func;
            
            
            if(pcidrv_setup->id_table)
            {
                int count=0,i;
                pci_model_id_t *id_setup;
                for(count=0,id_setup=(pci_model_id_t *)pcidrv_setup->id_table;id_setup[count].device!=0 || id_setup[count].vendor!=0;count++)
                    ;
                if(count)
                {
                    pci_model_drv_cxt->id_table=mem_model_alloc_buffer(sizeof(struct pci_device_id)*(count+1));
                    if(pci_model_drv_cxt->id_table)
                    {
                        struct pci_device_id *id=pci_model_drv_cxt->id_table;
                        id_setup=(pci_model_id_t *)pcidrv_setup->id_table;
                        for(i=0;i<count;i++)
                        {
                            if(id_setup->device)
                            {   
                                id->device=id_setup->device;
                                printk("id->device=%02x\n",id->device);
							}
                            else
                                id->device=PCI_ANY_ID;
                            
                            if(id_setup->vendor)
                                id->vendor=id_setup->vendor;
                            else
                                id->vendor=PCI_ANY_ID;
                            
                            if(id_setup->sub_vendor)
                                id->subvendor=id_setup->sub_vendor;
                            else
                                id->subvendor=PCI_ANY_ID;
                            
                            if(id_setup->sub_device)
                                id->subdevice=id_setup->sub_device;
                            else
                                id->subdevice=PCI_ANY_ID;
                            pci_model_drv_cxt->driver_data.flags=pcidrv_setup->flags;
                            pci_model_drv_cxt->driver_data.driver_data=id_setup->driver_data;
                            id->driver_data=(uintptr_t )&pci_model_drv_cxt->driver_data;
                            id++;
                            id_setup++;
                        }
                        id->vendor=0;
                        id->device=0;
                    } 
                }
                if(pci_model_drv_cxt->id_table)
                    driver->id_table=pci_model_drv_cxt->id_table;
            } 
        
            printk(KERN_ERR "DEBUG: pci_register_driver mit vendor=%04x device=%04x subvendor=%04x subdevice=%04x\n",
                   pci_model_drv_cxt->id_table[0].vendor,
                   pci_model_drv_cxt->id_table[0].device,
                   pci_model_drv_cxt->id_table[0].subvendor,
                   pci_model_drv_cxt->id_table[0].subdevice);
            err = pci_register_driver(&pci_model_drv_cxt->driver);
            if (err ==0)
            {
				printk(">>>pci_register_driver ok\n"); 
				printk(KERN_ERR "DEBUG: TEST NACH PCI REG\n");
			}
            else
            {
				printk(">>>pci_register_driver fail\n");
			}
			//pcidrv_setup->subsystem_id = subsystem_id;
			
        }
    }
    return 0;
}

void pci_model_driver_exit()
{
    mesg_debug("%s\n",__func__);
    if(pci_model_drv_cxt)
    {    
        pci_unregister_driver(&pci_model_drv_cxt->driver);
        if(pci_model_drv_cxt->id_table)
            mem_model_free_buffer(pci_model_drv_cxt->id_table);
        mem_model_free_buffer(pci_model_drv_cxt);
        pci_model_drv_cxt=NULL;
    } 
    mesg_debug("%s done\n",__func__);
}
