#include "sys.h"
#include "typedef.h"
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>

/* --- Zeitstempel Fix für Kernel 6.8 --- */
unsigned long long sys_gettimestamp(void) {
  struct timespec64 ts;
  unsigned long long timestamp;

  ktime_get_real_ts64(&ts);
  timestamp = (unsigned long long)ts.tv_sec;
  timestamp *= 1000000000;
  timestamp += ts.tv_nsec;

  return timestamp;
}

/* --- Spinlock & Semaphore Logik --- */
/* PROC-level locks use a mutex instead of spinlock because the blob
 * calls sleeping functions (sys_msleep, I2C ops) while holding them. */
typedef struct {
  sys_spin_lock_t interface;
  sys_spin_lock_level_e level;
  spinlock_t lock;
  struct mutex mlock;
  unsigned long flags;
} sys_spin_lock_cxt_t;

typedef struct {
  struct semaphore sem;
  struct timer_list timer;
} sys_wait_sem_t;

typedef struct {
  atomic_t value;
} sys_atomic_cxt_t;

/* sys_msleep: use mdelay when in atomic context to avoid scheduling crashes */
void sys_msleep(unsigned ms) {
  if (in_atomic() || irqs_disabled())
    mdelay(ms);
  else
    msleep(ms);
}
void sys_mdelay(unsigned ms) { mdelay(ms); }
void sys_udelay(unsigned us) { udelay(us); }

int sys_sprintf(char *buf, const char *fmt, ...) {
  va_list args;
  int i;
  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);
  return i;
}

int sys_sscanf(const char *buf, const char *fmt, ...) {
  va_list args;
  int i;
  va_start(args, fmt);
  i = vsscanf(buf, fmt, args);
  va_end(args);
  return i;
}

void *sys_get_drvdata(device_handle_t device_handle) {
  struct device *dev = device_handle;
  return dev ? dev_get_drvdata(dev) : NULL;
}

static void i2c_timeout(struct timer_list *t) {
  sys_wait_sem_t *wait_sem_cxt = container_of(t, sys_wait_sem_t, timer);
  up(&wait_sem_cxt->sem);
}

void sys_signal_sem(void *handle) {
  sys_wait_sem_t *wait_sem_cxt = (sys_wait_sem_t *)handle;
  if (wait_sem_cxt)
    up(&wait_sem_cxt->sem);
}

void sys_wait_sem(void *handle) {
  sys_wait_sem_t *wait_sem_cxt = (sys_wait_sem_t *)handle;
  if (wait_sem_cxt)
    down(&wait_sem_cxt->sem);
}

void sys_del_timer(void *handle) {
  sys_wait_sem_t *wait_sem_cxt = (sys_wait_sem_t *)handle;
  if (wait_sem_cxt)
    timer_delete_sync(&wait_sem_cxt->timer);
}

void sys_wait_sem_timer(void *handle, unsigned int timeout) {
  sys_wait_sem_t *wait_sem_cxt = (sys_wait_sem_t *)handle;
  if (!wait_sem_cxt)
    return;
  wait_sem_cxt->timer.expires = jiffies + (timeout ? timeout * HZ / 50 : HZ);
  add_timer(&wait_sem_cxt->timer);
  down(&wait_sem_cxt->sem);
}

void *sys_new_wait_sem(void) {
  sys_wait_sem_t *wait_sem_cxt = kzalloc(sizeof(sys_wait_sem_t), GFP_KERNEL);
  if (wait_sem_cxt) {
    sema_init(&wait_sem_cxt->sem, 0);
    timer_setup(&wait_sem_cxt->timer, i2c_timeout, 0);
  }
  return wait_sem_cxt;
}

void sys_release_waitsem(void *handle) {
  if (handle)
    kfree(handle);
}

sys_spin_lock_t *sys_new_spinlock(const char *name,
                                  sys_spin_lock_level_e level) {
  sys_spin_lock_cxt_t *lock_cxt =
      kzalloc(sizeof(sys_spin_lock_cxt_t), GFP_KERNEL);
  if (lock_cxt) {
    lock_cxt->interface.name = name;
    lock_cxt->level = level;
    spin_lock_init(&lock_cxt->lock);
    mutex_init(&lock_cxt->mlock);
    return &lock_cxt->interface;
  }
  return NULL;
}

/* PROC level uses mutex (allows sleeping), BH/IRQ levels use spinlock */
void sys_spin_lock(sys_spin_lock_t *lock) {
  sys_spin_lock_cxt_t *lock_cxt =
      container_of(lock, sys_spin_lock_cxt_t, interface);
  if (lock_cxt) {
    if (lock_cxt->level == SYS_SPINLOCK_LEVEL_PROC)
      mutex_lock(&lock_cxt->mlock);
    else if (lock_cxt->level == SYS_SPINLOCK_LEVEL_BH)
      spin_lock_bh(&lock_cxt->lock);
    else if (lock_cxt->level == SYS_SPINLOCK_LEVEL_IRQ)
      spin_lock_irqsave(&lock_cxt->lock, lock_cxt->flags);
  }
}

void sys_spin_unlock(sys_spin_lock_t *lock) {
  sys_spin_lock_cxt_t *lock_cxt =
      container_of(lock, sys_spin_lock_cxt_t, interface);
  if (lock_cxt) {
    if (lock_cxt->level == SYS_SPINLOCK_LEVEL_PROC)
      mutex_unlock(&lock_cxt->mlock);
    else if (lock_cxt->level == SYS_SPINLOCK_LEVEL_BH)
      spin_unlock_bh(&lock_cxt->lock);
    else if (lock_cxt->level == SYS_SPINLOCK_LEVEL_IRQ)
      spin_unlock_irqrestore(&lock_cxt->lock, lock_cxt->flags);
  }
}

void sys_release_spinlock(sys_spin_lock_t *lock) {
  if (lock)
    kfree(container_of(lock, sys_spin_lock_cxt_t, interface));
}

sys_atomic_t sys_new_atomic(void) {
  return kzalloc(sizeof(sys_atomic_cxt_t), GFP_KERNEL);
}

void sys_release_atomic(sys_atomic_t atomic) {
  if (atomic)
    kfree(atomic);
}

void sys_atomic_set(sys_atomic_t atomic, int value) {
  if (atomic)
    atomic_set(&((sys_atomic_cxt_t *)atomic)->value, value);
}

int sys_atomic_get(sys_atomic_t atomic) {
  return atomic ? atomic_read(&((sys_atomic_cxt_t *)atomic)->value) : 0;
}

/* --- Dateizugriff Fix für Kernel 6.8 (Deaktiviert, da VFS_READ nicht mehr
 * exportiert) --- */
void *sys_fopen(const char *filename, enum sys_fop_flag_e flag) { return NULL; }

void sys_fclose(void *fp) {}

SIZE_T sys_fread(void *fp, void *buf, SIZE_T size, SIZE_T count,
                 SIZE_T offset) {
  return 0;
}
