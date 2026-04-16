#include <linux/module.h> 
#include <linux/cdev.h>
#include <linux/version.h>

#include <linux/fs.h>

// minhas mudancas
#include "gpio.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SOFTWARE PWM on GPIO12 for Raspberry PI 4");

#define RWBUFSIZE 32
#define MODULE_NAME   "dimmer-rpi4"
#define DEVFILE_NAME  "dimmer"
#define CLASS_NAME  "PGSCE-DISEM"



static int blinker_major;
static struct hrtimer my_timer;

// PWM State
static unsigned int duty_cycle_setpoint = 50; // parametro de escolha entre 0 e 100 fui com 50
static unsigned int duty_cycle = 0; // start comeco do periodo pwm
static unsigned int new_period = 1; // 1 comeco e depois 0 vai ser metade do periodo
static ktime_t blink_delay; // proximo intervalo para programar no hrtimer


//debug visibilidade logica
static unsigned char led_status = 0;

static int device_open = 0;
static struct device* blinker_device;
static struct class* blinker_class;

// Duty-cycle parameter required by the exercise 
module_param(duty_cycle_setpoint, uint, 0644);
MODULE_PARM_DESC(duty_cycle_setpoint, "PWM duty cycle in percent (0..100)");

/* Optional debug parameter */
module_param(led_status, byte, 0444);
MODULE_PARM_DESC(led_status, "Current logical output state");

static int blinker_open(struct inode *inode, struct file *filp)
{
  if (device_open)
  {
    printk(KERN_WARNING "Already open\n");
    return -EBUSY;
  }
  device_open++;
  try_module_get(THIS_MODULE);
  return 0;
}

static int blinker_release(struct inode *inode, struct file *filp)
{
  device_open--;
  module_put(THIS_MODULE);
  return 0;
}



static ssize_t blinker_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  char local_buf[RWBUFSIZE];
  int len;

  if (*f_pos > 0)
    return 0;

  len = sprintf(local_buf, "%u\n", duty_cycle_setpoint);

  if (copy_to_user(buf, local_buf, len))
    return -EFAULT;

  *f_pos = len;
  return len;
}

static ssize_t blinker_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  char local_buf[RWBUFSIZE];
  long val;

  if (count >= RWBUFSIZE)
    return -EINVAL;

  if (copy_from_user(local_buf, buf, count))
    return -EFAULT;

  local_buf[count] = '\0';

  if (kstrtol(local_buf, 10, &val) < 0)
    return -EINVAL;

  if (val < 0 || val > 100)
    return -EINVAL;

  duty_cycle_setpoint = val;

  printk(KERN_WARNING "New duty cycle: %ld%%\n", val);

  return count;
}

static struct file_operations blinker_fops = {
  .owner =    THIS_MODULE,
  .read =     blinker_read,
  .write =    blinker_write,
  .open =     blinker_open,
  .release =  blinker_release,
};

static enum hrtimer_restart my_timer_func(struct hrtimer *timer)
{
  ktime_t interval;

  if (new_period == 1)
  {
    duty_cycle = duty_cycle_setpoint;

    if (duty_cycle > 0)
    {
      led_status = 1;
      gpio12_set(1);
      printk(KERN_WARNING "PWM ON\n");

      interval = ktime_set(0, duty_cycle * 10000);

      if (duty_cycle < 100)
        new_period = 0;
    }
    else
    {
      led_status = 0;
      gpio12_set(0);
      printk(KERN_WARNING "PWM OFF (0%%)\n");

      interval = ktime_set(0, 1000000);
    }
  }
  else
  {
    led_status = 0;
    gpio12_set(0);
    printk(KERN_WARNING "PWM OFF phase\n");

    interval = ktime_set(0, (100 - duty_cycle) * 10000);
    new_period = 1;
  }

  hrtimer_forward_now(timer, interval);
  return HRTIMER_RESTART;
}


static int __init blinker_init(void)
{
  int result;

  blinker_major = register_chrdev(0, MODULE_NAME, &blinker_fops);
  if (blinker_major < 0) {
    printk(KERN_WARNING "dimmer-rpi4: can't get major number\n");
    return blinker_major;
  }

  printk(KERN_WARNING "dimmer-rpi4: major=%d\n", blinker_major);

  hrtimer_init(&my_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  my_timer.function = my_timer_func;

  new_period = 1;
  duty_cycle = duty_cycle_setpoint;

  /* 1 ms = 1,000,000 ns => 1 kHz PWM base period */
  blink_delay = ktime_set(0, 1000000);

  if (gpio12_init_output() < 0) {
    unregister_chrdev(blinker_major, MODULE_NAME);
    return -ENOMEM;
  }

  gpio12_set(0);
  hrtimer_start(&my_timer, blink_delay, HRTIMER_MODE_REL);

  blinker_class = class_create(CLASS_NAME);
  if (IS_ERR(blinker_class)) {
    hrtimer_cancel(&my_timer);
    gpio12_set(0);
    gpio12_cleanup();
    unregister_chrdev(blinker_major, MODULE_NAME);
    return PTR_ERR(blinker_class);
  }

  blinker_device = device_create(blinker_class, NULL, MKDEV(blinker_major, 0), NULL, DEVFILE_NAME);
  if (IS_ERR(blinker_device)) {
    class_destroy(blinker_class);
    hrtimer_cancel(&my_timer);
    gpio12_set(0);
    gpio12_cleanup();
    unregister_chrdev(blinker_major, MODULE_NAME);
    return PTR_ERR(blinker_device);
  }

  return 0;
}
static void __exit blinker_exit(void)
{
  device_destroy(blinker_class, MKDEV(blinker_major,0));
  class_destroy(blinker_class);

  hrtimer_cancel(&my_timer);

  gpio12_set(0);
  gpio12_cleanup();

  unregister_chrdev(blinker_major, MODULE_NAME);
}

module_init(blinker_init);  
module_exit(blinker_exit);

/* 
linux/fs.h

int __register_chrdev(unsigned int major, unsigned int baseminor, unsigned int count, const char *name, const struct file_operations *fops)

int register_chrdev(unsigned int major, const char *name, const struct file_operations *fops) {
  return __register_chrdev(major, 0, 256, name, fops);
}
*/
  
/*
https://origin.kernel.org/doc/html/v6.12/driver-api/basics.html#c.hrtimer_init

void hrtimer_init(struct hrtimer *timer, clockid_t clock_id, enum hrtimer_mode mode)

    initialize a timer to the given clock

Parameters

struct hrtimer *timer

    the timer to be initialized
clockid_t clock_id

    the clock to be used
enum hrtimer_mode mode

    The modes which are relevant for initialization: HRTIMER_MODE_ABS, HRTIMER_MODE_REL, HRTIMER_MODE_ABS_SOFT, HRTIMER_MODE_REL_SOFT
    
    

void hrtimer_start(struct hrtimer *timer, ktime_t tim, const enum hrtimer_mode mode)

    (re)start an hrtimer

Parameters

struct hrtimer *timer

    the timer to be added
ktime_t tim

    expiry time
const enum hrtimer_mode mode

    timer mode: absolute (HRTIMER_MODE_ABS) or relative (HRTIMER_MODE_REL), and pinned (HRTIMER_MODE_PINNED); softirq based mode is considered for debug purpose only!


u64 hrtimer_forward_now(struct hrtimer *timer, ktime_t interval)

    forward the timer expiry so it expires after now

Parameters

struct hrtimer *timer

    hrtimer to forward
ktime_t interval

    the interval to forward

Description

It is a variant of hrtimer_forward(). The timer will expire after the current time of the hrtimer clock base. See hrtimer_forward() for details.


*/
