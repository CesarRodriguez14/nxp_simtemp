#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kfifo.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include "gaussian_random.h"
#include "simtemp.h"

#define TIMER_INTERVAL_NS 250000000LL //0.25 second in nanoseconds
#define FIFO_SIZE 8
#define T_MEAN 2000
#define T_STD 100
#define NEW_SAMPLE (1<<0)
#define THRESHOLD_CROSSED (1<<1)

static __s32 threshold_mC = 2100; // 2.1°C

//Statically allocated FIFO
static DEFINE_KFIFO(CBuffer,struct simtemp_sample,FIFO_SIZE);
static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(fifo_lock);

static struct hrtimer my_timer;
static ktime_t kt_period;

static struct simtemp_sample current_sample;


//device driver
dev_t dev = 0;
static struct class *dev_class;
static struct cdev k_cdev;

//Driver functions
static int simtemp_open(struct inode *inode,struct file *file);
static int simtemp_release(struct inode *inode, struct file *file);
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t len, loff_t* off);
static ssize_t simtemp_write(struct file *file, const char *buf, size_t len, loff_t* off);
static unsigned int simtemp_poll(struct file *file, poll_table *wait);

//File operation structure
static struct file_operations fops =
{
	.owner		= THIS_MODULE,
	.read		= simtemp_read,
	.write		= simtemp_write,
	.poll		= simtemp_poll,
	.open		= simtemp_open,
	.release	= simtemp_release,
};

static int simtemp_open(struct inode *inode,struct file *file)
{
	//pr_info("kfifo_hrtimer: Opening file\n");
	//if((kernel_buffer = kmalloc(sizeof(current_sample),GFP_KERNEL)) == 0)
	//{
	//	pr_info("kfifo_hrtimer: Cannot allocate memory in kernel\n");
	//	return -1;
	//}
	pr_info("kfifo_hrtimer: Device File Opened \n");
	return 0;
}

static int simtemp_release(struct inode *inode, struct file *file)
{
	pr_info("kfifo_hrtimer: Device File Closed \n");
	return 0;
}

static ssize_t simtemp_read(struct file *file, char __user *buf, size_t len, loff_t* off)
{
	mutex_lock(&fifo_lock);
	kfifo_out(&CBuffer,&current_sample,1);
	copy_to_user((void __user *)buf,&current_sample,sizeof(current_sample));
	mutex_unlock(&fifo_lock);
	pr_info("kfifo_hrtimer: Read function\n");
	return sizeof(current_sample);
}


static ssize_t simtemp_write(struct file *file, const char *buf, size_t len, loff_t* off)
{
	//copy_from_user(kernel_buffer,buf,len);
	pr_info("kfifo_hrtimer: Write function\n");
	return len;
}
static unsigned int simtemp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	
	poll_wait(file,&wq,wait);
	pr_info("kfifo_hrtimer: Set POLLIN | POLLRDNORM flags\n");
	
	mutex_lock(&fifo_lock);
	if(!kfifo_is_empty(&CBuffer))
	{
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&fifo_lock);
	
	return mask;
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct simtemp_sample old_value;
	
	mutex_lock(&fifo_lock);
	if(kfifo_is_full(&CBuffer))
	{
		kfifo_out(&CBuffer,&old_value,1);
		//pr_info("kfifo_hrtimer: CBuffer full, oldest value %d dropped\n", old_value);
	}
	current_sample.temp_mC = gaussian_s32_clt(T_MEAN,T_STD);
	current_sample.timestamp_ns = ktime_get_real_ns();
	current_sample.flags = NEW_SAMPLE;
	if (current_sample.temp_mC > threshold_mC)
	{
		current_sample.flags |= THRESHOLD_CROSSED; 
	}
	kfifo_in(&CBuffer,&current_sample,1);
	wake_up_interruptible(&wq);
	pr_info("kfifo_hrtimer: %llu | Inserted %d into CBuffer\n",current_sample.timestamp_ns,current_sample.temp_mC);
	mutex_unlock(&fifo_lock);
	
	//Re-arm the timer
	hrtimer_forward_now(timer,kt_period);
	return HRTIMER_RESTART;
}

static int __init kfifo_hrtimer_init(void)
{
	pr_info("kfifo_hrtimer: Module loading ...\n");
	
	// Set the timer interval
	kt_period = ktime_set(0,TIMER_INTERVAL_NS); // seconds, nanoseconds
	
	// Initialize the hrtimer
	hrtimer_init(&my_timer,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
	my_timer.function = timer_callback;
	
		//Allocate Major Number
	if((alloc_chrdev_region(&dev, 0, 1,"sim_temp"))<0)
	{
		pr_info("kfifo_hrtimer: Cannot allocate major number\n");
		return -1;
	}
	pr_info("kfifo_hrtimer: Major = %d Minor = %d \n", MAJOR(dev),MINOR(dev));
	
	//Create cdev structure
	cdev_init(&k_cdev,&fops);
	
	//Adding character device to the system
	if((cdev_add(&k_cdev,dev,1)) < 0)
	{
		pr_info("kfifo_hrtimer: Cannot add the device to the system\n");
		goto r_class;
	}
	
	//Create struct class
	if(IS_ERR(dev_class = class_create(THIS_MODULE,"dev_class")))
	{
		pr_info("kfifo_hrtimer: Cannot create the struct class\n");
		goto r_class;
	}
	
	//Create device
	if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"my_device")))
	{
		pr_info("kfifo_hrtimer: Cannot create the Device 1\n");
		goto r_device;
		
	}
	
	//Start the timer
	hrtimer_start(&my_timer,kt_period,HRTIMER_MODE_REL);
	
	pr_info("kfifo_hrtimer: Module loaded\n");
	return 0;
	
r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	cdev_del(&k_cdev);
	return -1;
}

static void __exit kfifo_hrtimer_exit(void)
{
	struct simtemp_sample val;
	int ret = 0;
	int i = 1;
	
	pr_info("kfifo_hrtimer:Module unloading...\n");
	
	//Cancel the timer if it's active
	ret = hrtimer_cancel(&my_timer);
	if(ret)
		pr_info("kfifo_hrtimer: Timer was still active and canceled\n");
	
	//Drain and free FIFO
	mutex_lock(&fifo_lock);
	while(!kfifo_is_empty(&CBuffer))
	{
		kfifo_out(&CBuffer,&val,1);
		pr_info("kfifo_hrtimer: %d Drained %d\n",i++,val.temp_mC);
	}
	mutex_unlock(&fifo_lock);
	
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&k_cdev);
	unregister_chrdev_region(dev,1);
	
	
	pr_info("kfifo_hrtimer: Module unloaded \n");
}

module_init(kfifo_hrtimer_init);
module_exit(kfifo_hrtimer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LASEC TECHNNOLOGIES");
MODULE_DESCRIPTION("Example: kfifo filled by hrtimer every second using kfifo_alloc()");
