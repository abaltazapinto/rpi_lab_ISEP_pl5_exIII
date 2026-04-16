#include <linux/module.h> 
#include <linux/cdev.h>
#include <linux/version.h>

#include <linux/fs.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Blinker");

#define RWBUFSIZE 32
#define MODULE_NAME   "Blinker" 
#define DEVFILE_NAME  "blinker"  
#define CLASS_NAME  "PGSCE-DISEM"

static int blinker_major;
static struct hrtimer my_timer;
static unsigned char led_status = 0;
static unsigned long blink_delay_nsec;
static unsigned long blink_delay_sec;
static ktime_t blink_delay;
static int device_open = 0;
static struct device* blinker_device;
static struct class* blinker_class;

//check in /sys/module/blinker/parameters/led_status
module_param(led_status, byte, 0444);
MODULE_PARM_DESC(led_status, "Port status");

module_param(blink_delay_nsec, ulong, 0444);
MODULE_PARM_DESC(blink_delay_nsec , "Number of nanoseconds of delay (to be added to blink_delay_sec)");

module_param(blink_delay_sec, ulong, 0444);
MODULE_PARM_DESC(blink_delay_sec, "Number of seconds of delay (to be added to blink_delay_nsec)");

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
  int len1 = 0;

  static char local_buf[RWBUFSIZE];
  static int len;
  int res;
  
  if((*f_pos)==0) {
    sprintf(local_buf, "%ld\n", blink_delay_nsec*2);    
    len = strnlen(local_buf, RWBUFSIZE-1);
  }

  len1 = len - (*f_pos);
  len1 = len1 > count ? count : len1;
      
  res = copy_to_user(buf, local_buf + (*f_pos), len1);  
  if(res!=0)
    printk(KERN_WARNING "Bytes left to copy\n");
  
  (*f_pos) += len1;

  return len1;
}

static ssize_t blinker_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{

  
  static char local_buf[RWBUFSIZE];
  int res, i;
  char c;
  
  for(i=0; i < count; ++i, ++(*f_pos))
  {
    if((*f_pos) > RWBUFSIZE - 2) //read \n and leave space for \0
      return -1;

    res = copy_from_user(&c, buf + i, 1);
    if(res!=0)
      printk(KERN_WARNING "Bytes left to copy\n");
    
    if(c == '\n')
    {
      local_buf[*f_pos] = 0;
      blink_delay_nsec = simple_strtol(local_buf, NULL, 0)/2; 
      blink_delay = ktime_set(blink_delay_sec, blink_delay_nsec);
      printk(KERN_WARNING "New period: %ld ns\n", blink_delay_nsec*2);    
      return i+1;
    }
    else
      local_buf[*f_pos] = c;
  }

  return count;
}

static struct file_operations blinker_fops = {
  .owner =    THIS_MODULE,
  .read =     blinker_read,
  .write =    blinker_write,
  .open =     blinker_open,
  .release =  blinker_release,
};

static enum hrtimer_restart my_timer_func(struct hrtimer *timer){
  hrtimer_forward_now(timer, blink_delay);
  
  led_status = ~led_status;
  
  printk(KERN_WARNING "blinker: %x", led_status);
  
  return HRTIMER_RESTART;
}


static int __init blinker_init(void)
{
  int result;

  blinker_major = register_chrdev(0, MODULE_NAME, &blinker_fops);
  if (blinker_major < 0) {
    printk(KERN_WARNING "blinker: can't get major number\n");
    return result;
  }
    
  printk(KERN_WARNING "Blinker: major=%d HZ: %d\n", blinker_major, HZ);    
    
  hrtimer_init(&my_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  my_timer.function = my_timer_func;
  blink_delay_nsec = 25000UL;
  blink_delay_sec = 0;
  blink_delay = ktime_set(blink_delay_sec, blink_delay_nsec);
  hrtimer_start(&my_timer, blink_delay, CLOCK_MONOTONIC);  

  blinker_class = class_create(CLASS_NAME);
  if (IS_ERR(blinker_class)){
    hrtimer_cancel(&my_timer);
    unregister_chrdev(blinker_major, MODULE_NAME); 
  }    
  
  blinker_device = device_create(blinker_class, NULL, MKDEV(blinker_major,0), NULL, DEVFILE_NAME);
  if (IS_ERR(blinker_device)){
    class_destroy(blinker_class);
    hrtimer_cancel(&my_timer);
    unregister_chrdev(blinker_major, MODULE_NAME); 
  } 
  
  return 0;
}

static void __exit blinker_exit(void) 
{
  device_destroy(blinker_class, MKDEV(blinker_major,0));
  class_destroy(blinker_class);  
  
  hrtimer_cancel(&my_timer);

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