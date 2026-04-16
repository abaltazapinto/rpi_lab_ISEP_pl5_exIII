#include <linux/module.h> 
#include <linux/cdev.h>
#include <linux/version.h>
#include "gpio.h"
#include <linux/fs.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Blinker");

#define RWBUFSIZE 11
#define MODULE_NAME   "Blinker"
#define DEVFILE_NAME  "blinker"
#define CLASS_NAME  "PGSCE-DISEM"

static int blinker_major;
static struct timer_list my_timer;
static unsigned char led_status = 0;
static unsigned int blink_delay = HZ;
static int device_open = 0;
static struct device* blinker_device;
static struct class* blinker_class;

//check in /sys/module/blinker/parameters/led_status
module_param(led_status, byte, 0444);
MODULE_PARM_DESC(led_status, "Port status");

module_param(blink_delay, uint, 0444);
MODULE_PARM_DESC(blink_delay, "Half period in jiffies");
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
  static unsigned int period_ms;

  int res;
  
  if((*f_pos)==0) {
    period_ms = blink_delay*1000/HZ*2; 
    sprintf(local_buf, "%d\n", period_ms);    
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
  int period_msec;
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
      period_msec = simple_strtol(local_buf, NULL, 0); 
      blink_delay = period_msec*HZ/2000;
      printk(KERN_WARNING "New period: %d ms\n", period_msec);    
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

static void my_timer_func(struct timer_list *unused){
  led_status = ~led_status;

  gpio12_set(led_status ? 1 : 0);

  printk(KERN_WARNING "blinker: %x", led_status);
  
  my_timer.expires += blink_delay;
  add_timer(&my_timer);
}


static int __init blinker_init(void)
{
  int result;
;
printk(KERN_WARNING
"ANDRE_TEST_BLINKER_INIT\n");

	
  /* 
  linux/fs.h
  
  int __register_chrdev(unsigned int major, unsigned int baseminor, unsigned int count, const char *name, const struct file_operations *fops)
  
  int register_chrdev(unsigned int major, const char *name, const struct file_operations *fops) {
    return __register_chrdev(major, 0, 256, name, fops);
  }
  */
  blinker_major = register_chrdev(0, MODULE_NAME, &blinker_fops);
  if (blinker_major < 0) {
    printk(KERN_WARNING "blinker: can't get major number\n");
    return result;
  }
    
  printk(KERN_WARNING "Blinker: major=%d HZ: %d\n", blinker_major, HZ);    
    


  blinker_class = class_create(CLASS_NAME);
  if (IS_ERR(blinker_class)){
    timer_delete(&my_timer);
    unregister_chrdev(blinker_major, MODULE_NAME); 
  }    
  
  blinker_device = device_create(blinker_class, NULL, MKDEV(blinker_major,0), NULL, DEVFILE_NAME);
  if (IS_ERR(blinker_device)){
    class_destroy(blinker_class);
    timer_delete(&my_timer);
    unregister_chrdev(blinker_major, MODULE_NAME);
    return PTR_ERR(blinker_device);
  }  

  if (result) {
    device_destroy(blinker_class, MKDEV(blinker_major,0));
    class_destroy(blinker_class);
    timer_delete(&my_timer);
    unregister_chrdev(blinker_major, MODULE_NAME);
    return result;
  }
  result = gpio12_init_output();
	if (result)
		return result;
//	gpio12_set(1);

timer_setup(&my_timer, my_timer_func, 0);
  my_timer.expires = jiffies + blink_delay;
  add_timer(&my_timer);

  return 0;
}

static void __exit blinker_exit(void) 
{
  device_destroy(blinker_class, MKDEV(blinker_major,0));
  class_destroy(blinker_class);  
  
  timer_delete(&my_timer);

  unregister_chrdev(blinker_major, MODULE_NAME);
	gpio12_cleanup(); 
}

module_init(blinker_init);  
module_exit(blinker_exit);
