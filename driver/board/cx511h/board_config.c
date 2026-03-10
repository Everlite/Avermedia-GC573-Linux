/*===================================================================
 * Copyright(c) AVerMedia TECHNOLOGIES, Inc. 2017
 * All rights reserved
 * =================================================================
 * board_config.c
 *
 *  Created on: Apr 15, 2017
 *      Author: 
 *      Version:
 * =================================================================
 */
 
//#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include "board.h"
#include "cxt_mgr.h"
#include "pci_model.h"
#include "i2c_model.h"
#include "mem_model.h"
#include "gpio_model.h"
#include "task_model.h"
#include "trace.h"
#include "debug.h"
#include "aver_xilinx.h"
#include "board_i2c.h"
#include "board_gpio.h"
#include "board_alsa.h"
#include "ite6805.h"
#include "board_v4l2.h"

/* Force linker to keep the ITE6805 I2C driver registration from AverMediaLib_64.
 * The i2c_model_data linker section mechanism is broken, so we reference the
 * struct directly and use its .attach callback as a fallback. */
extern i2c_model_driver_t ITE6805_ID_i2c_model_data;

/* Local copy of ITE6805 config for direct attach fallback.
 * Must match the config in board_i2c.c */
static ite6805_cfg_t ite6805_fallback_cfg = {
    .out_ctrl = {
        .out_fmt = ITE6805_OUT_FORMAT_SDR_ITU656_24_MODE0,
        .out_colorspace = ITE6805_OUT_YUV709,
    },
};
#include "pic_bmp.h"

static char *no_signal_pic = NULL;
module_param(no_signal_pic, charp, 0444);
MODULE_PARM_DESC(no_signal_pic, "Loading this bitmap file and display it when the input is no signal");

//static char *out_of_range_pic = NULL;
//module_param(out_of_range_pic, charp, 0444);
//MODULE_PARM_DESC(out_of_range_pic, "Loading this bitmap file and display it when the content is out of range");

static char *copy_protetion_pic = NULL;
module_param(copy_protetion_pic, charp, 0444);
MODULE_PARM_DESC(copy_protetion_pic, "Loading this bitmap file and display it when the content was protected");

/*
 * LED GPIO pin configuration (module parameters)
 * The GC573 has 9 FPGA GPIO pins (0-8). Pin 0 = Reset, Pin 2 = HPD.
 * Default candidate pins for RGB LED: 3 (Red), 4 (Green), 5 (Blue)
 * Set to -1 to disable a color channel.
 * Adjust via: insmod cx511h.ko led_pin_r=3 led_pin_g=4 led_pin_b=5
 */
static int led_pin_r = 3;
module_param(led_pin_r, int, 0644);
MODULE_PARM_DESC(led_pin_r, "GPIO pin number for red LED (-1 to disable, default: 3)");

static int led_pin_g = 4;
module_param(led_pin_g, int, 0644);
MODULE_PARM_DESC(led_pin_g, "GPIO pin number for green LED (-1 to disable, default: 4)");

static int led_pin_b = 5;
module_param(led_pin_b, int, 0644);
MODULE_PARM_DESC(led_pin_b, "GPIO pin number for blue LED (-1 to disable, default: 5)");

int board_init(void);
void board_exit(void);

int board_get_led_pin_r(void) { return led_pin_r; }
int board_get_led_pin_g(void) { return led_pin_g; }
int board_get_led_pin_b(void) { return led_pin_b; }

const char *BOARD_NAME="CL511H";
//pci_model_driver_setup_t pci_setup;
extern int subsystem_id;

pci_model_id_t id_table[]={
    {
      .vendor=0x1461,
      .device=0x0054, //according to hw DEVICE ID config
      .sub_vendor = 0x1461,
      .sub_device = 0x5730,
      .driver_data=CL511H,        
    },
    {
      0
    }
    
};

static aver_xilinx_cfg_t aver_xilinx_cfg=
{
    .gpio_mask = Bit(1)|Bit(3)|Bit(4)|Bit(5)|Bit(6)|Bit(7)|Bit(8),  // Enable candidate LED GPIO pins as output
    .gpio_value = 0,    // All GPIO outputs initially low
    .i2c_bus_speed={
        [AVER_XILINX_I2C_BUS_0]=400*1000,
   
    },
    .audio_buffer_size=11520*4,//10584 12*1024,
    
};

int board_probe(struct device *dev,unsigned long driver_info)
{
    cxt_mgr_handle_t cxt_mgr=get_cxt_manager(dev);
    handle_t pci_handle=NULL;
    handle_t aver_xilinx_handle=NULL;
    handle_t trace_handle=NULL;
    handle_t i2c_mgr=NULL;
    handle_t gpio_mgr=NULL;
    handle_t ite6805_handle_1=NULL; 
    handle_t task_handle=NULL;
    handle_t mem_handle=NULL;

    int i;
    int ret;
    enum
    {        
	    NO_ERROR=0,
        NO_PCI_HANDLE,
        ERROR_TRACE_HANDLE,
        ERROR_I2C_MGR,
        ERROR_GPIO_MGR,
        ERROR_TASK_HANDLE,
        ERROR_MEM_HANDLE,
        ERROR_AVER_XILINX,
        ERROR_BOARD_I2C_INIT,
        ERROR_BOARD_GPIO_INIT,
    }err=NO_ERROR;
    
        
    mesg("board_probe start \n");
    debug_msg("%s\n",__func__);
    do
    {
        pci_handle=pci_model_get_handle(cxt_mgr);
        if(!pci_handle)
        {
			mesg("pci_handle err\n");
            err=NO_PCI_HANDLE;
            break;
        } 
	    i2c_mgr=i2c_model_init(cxt_mgr);
	    if(!i2c_mgr)
	    {
		    mesg("i2c_model_init err\n");
            err=ERROR_I2C_MGR;
            break;
	    }
        gpio_mgr=gpio_model_init(cxt_mgr);
        if(!gpio_mgr)
        {
			mesg("gpio_model_init err\n");
            err=ERROR_GPIO_MGR;
            break;
        }
        mem_handle=mem_model_init(cxt_mgr);
        if(!mem_handle)
        {
			mesg("mem_model_init err\n");
            err=ERROR_MEM_HANDLE;
            break;
        }
        task_handle=task_model_init(cxt_mgr);
        if(!task_handle)
        {
			mesg("task_model_init err\n");
            err=ERROR_TASK_HANDLE;
            break;
        }
        aver_xilinx_handle=aver_xilinx_init(cxt_mgr,pci_handle,&aver_xilinx_cfg);
        if(!aver_xilinx_handle)
        {
			mesg("aver_xilinx_init err\n");
            err=ERROR_AVER_XILINX;
            break;
        }
        aver_xilinx_init_registers(aver_xilinx_handle, &aver_xilinx_cfg);
        //aver_xilinx_add_trace(aver_xilinx_handle,trace_handle);
        for(i=0;i<BOARD_I2C_BUS_NUM;i++)
        {
            i2c_model_bus_cfg_t i2c_bus_cfg;
            const char *i2c_bus_name;
            
            i2c_bus_name=board_get_i2c_bus_name(i);
            switch(i)
            {
                case I2C_BUS_COM:
                    aver_xilinx_get_i2c_bus_cfg(aver_xilinx_handle,AVER_XILINX_I2C_BUS_0,&i2c_bus_cfg);
                    i2c_model_new_bus(i2c_mgr,i2c_bus_name,&i2c_bus_cfg);
                    break;
                
            }
            
        }
        ret=board_gpio_init(cxt_mgr);
        if(ret!=0)
        {
			mesg("board_gpio_init\n");
            err=ERROR_BOARD_GPIO_INIT;
            break;
        }
        printk(KERN_ERR "DEBUG: board_i2c_init starten...\n");
        ret=board_i2c_init(cxt_mgr,driver_info);
        printk(KERN_ERR "DEBUG: board_i2c_init ret=%d\n", ret);
        if(ret!=0)
	    {
		    mesg("board_i2c_init\n");
            err=ERROR_BOARD_I2C_INIT;
            break;
	    }   
       
        /* Give hardware time to settle after I2C init */
        msleep(100);
     
        printk(KERN_ERR "DEBUG: suche ITE6805 ueber i2c_model_get_driver_handle...\n");
        ite6805_handle_1=i2c_model_get_driver_handle(i2c_mgr,ITE6805_DRVNAME);
        printk(KERN_ERR "DEBUG: ite6805_handle_1=%p (normaler Weg)\n", ite6805_handle_1);
        if(!ite6805_handle_1)
        {
            /* Linker section i2c_model_data is broken - call attach directly
             * from the blob's exported registration struct */
            i2c_model_bus_handle_t i2c_bus;
            printk(KERN_ERR "DEBUG: Fallback: ITE6805_ID_i2c_model_data.drv_name=%s .attach=%p\n",
                   ITE6805_ID_i2c_model_data.drv_name ? ITE6805_ID_i2c_model_data.drv_name : "(null)",
                   ITE6805_ID_i2c_model_data.attach);
            i2c_bus = i2c_model_get_bus(i2c_mgr, board_get_i2c_bus_name(0));
            printk(KERN_ERR "DEBUG: i2c_bus=%p\n", i2c_bus);
            if (i2c_bus && ITE6805_ID_i2c_model_data.attach)
            {
                ite6805_handle_1 = ITE6805_ID_i2c_model_data.attach(i2c_bus, 0x58, &ite6805_fallback_cfg);
                printk(KERN_ERR "DEBUG: direkt attach => ite6805_handle_1=%p\n", ite6805_handle_1);
                if (ite6805_handle_1) {
                    i2c_model_force_register_driver_handle(i2c_mgr, ITE6805_DRVNAME, ite6805_handle_1);
                }
            }
        }
	    if(!ite6805_handle_1)
	    {
		    printk(KERN_ERR "ERROR: ITE6805 konnte nicht initialisiert werden!\n");
		    err=ERROR_BOARD_I2C_INIT;
		    break;
	    }
	    ite6805_add_trace(ite6805_handle_1,trace_handle); 
	    
	    mesg("%s subsystem_id=%x\n",__func__,subsystem_id);

        pic_bmp_init(cxt_mgr, no_signal_pic, NULL, copy_protetion_pic);
        printk(KERN_ERR "DEBUG: board_alsa_init starten...\n");
        board_alsa_init(cxt_mgr); 
        printk(KERN_ERR "DEBUG: board_v4l2_init starten...\n");
        board_v4l2_init(cxt_mgr,subsystem_id);  
        //aver_xilinx_sha204_init(aver_xilinx_handle);
        
    }while(0);
    if(err!=NO_ERROR)
    {
        debug_msg("%s err\n",__func__,err);
        mesg(">>>board_probe fail \n");
        switch(err)
        {
            case ERROR_AVER_XILINX:
                cxt_manager_unref_context(i2c_mgr);
            case ERROR_I2C_MGR:
                cxt_manager_unref_context(gpio_mgr);
            case ERROR_GPIO_MGR:    
                 cxt_manager_unref_context(trace_handle);
            case ERROR_TRACE_HANDLE:
                
            case NO_PCI_HANDLE:
                break;
            default:
                break;
        }
        return err;
    }
    
    
    return 0;
}

void board_suspend(struct device *dev)
{
    cxt_mgr_handle_t cxt_mgr;

    if (!dev)
    {
        debug_msg("Error: pointer is NULL");
        return;
    }

    cxt_mgr = get_cxt_manager(dev);
    if (!cxt_mgr)
    {
        debug_msg("Error: pointer is NULL");
        return;
    }

    board_v4l2_suspend(cxt_mgr);
}

void board_resume(struct device *dev)
{
    cxt_mgr_handle_t cxt_mgr;
    handle_t aver_xilinx_handle;

    if (!dev)
    {
        debug_msg("Error: pointer is NULL");
        return;
    }

    cxt_mgr = get_cxt_manager(dev);
    if (!cxt_mgr)
    {
        debug_msg("Error: pointer is NULL");
        return;
    }

    aver_xilinx_handle=cxt_manager_get_context(cxt_mgr,AVER_XILINX_CXT_ID,0);
    if (!aver_xilinx_handle)
    {
        debug_msg("Error: pointer is NULL");
        return;
    }

    aver_xilinx_init_registers(aver_xilinx_handle, &aver_xilinx_cfg);

    board_v4l2_resume(cxt_mgr);
}

void board_remove(struct device *dev)
{
    debug_msg("%s\n",__func__);
}


int board_init()
{
    pci_model_driver_setup_t pci_setup;
    
    pci_setup.name=BOARD_NAME;
    pci_setup.id_table=id_table;
    pci_setup.prob_func=board_probe;
    pci_setup.suspend_func=board_suspend;
    pci_setup.resume_func=board_resume;
    pci_setup.remove_func=board_remove;
    pci_setup.flags= PCI_MODEL_FORCE_DMA_32;
    pci_model_driver_init(&pci_setup);

	return 0;
}

void board_exit()
{
    pci_model_driver_exit();
}
