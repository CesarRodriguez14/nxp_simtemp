#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#include "gaussian_random.h"
#include "simtemp.h"

//Temperature generation modes
#define MODE_NRM 0
#define MODE_NSY 1
#define MODE_RMP 2

//Ring buffer size
#define FIFO_SIZE 256

//Temperature simulation
#define TEMP_MEAN_mC 20000
#define TEMP_MAX 100000
#define TEMP_MIN -50000

//Sampling range 
#define TIME_MIN_us 50 // 20 KHz
#define TIME_MAX_us 10000000 // 10s

//Events flags
#define FLAG_NEW_SAMPLE (1<<0)
#define FLAG_THRESHOLD_CROSSED (1<<1)

//Temperature modes names
const char * modes[] ={"normal","noisy","ramp"};

//Kernel space variables
volatile __u32 sampling_us = 100000; //100 ms
volatile __s32 threshold_mC = 20000; //20 °C
volatile __u8 mode = MODE_RMP;
volatile __u8  flags = 0;
volatile __u32 TEMP_STD_mC = 100; // Temperature standard deviation 0.1 °C

//Statically allocated FIFO and waitqueue
static DEFINE_KFIFO(CBuffer,struct simtemp_sample,FIFO_SIZE);
static DECLARE_WAIT_QUEUE_HEAD(wq);

//Define mutexes
static DEFINE_MUTEX(sampling_us_lock);
static DEFINE_MUTEX(threshold_mC_lock);
static DEFINE_MUTEX(mode_lock);

//Define spinlock
static DEFINE_SPINLOCK(fifo_lock);
static DEFINE_SPINLOCK(flags_lock);

//Hrtimer variables
static struct hrtimer my_timer;
static ktime_t kt_period;

dev_t dev = 0;
static struct class *dev_class;
static struct cdev k_cdev;
struct kobject *kobj_ref;

static struct simtemp_sample current_sample={.timestamp_ns = 0, .temp_mC = TEMP_MEAN_mC, .flags = 0};
static struct simtemp_flags e_flags={.counter = 0, .alert = 0, .l_error = E_NO_ERR}; 

//Prototypes
static int __init nxp_simtemp_init(void);
static void __exit nxp_simtemp_exit(void);

//Driver functions
static int nxp_simtemp_open(struct inode *inode,struct file *file);
static int nxp_simtemp_release(struct inode *inode, struct file *file);
static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t len, loff_t* off);
static ssize_t nxp_simtemp_write(struct file *file, const char *buf, size_t len, loff_t* off);
static unsigned int nxp_simtemp_poll(struct file *file, poll_table *wait);

//sysfs functions
static ssize_t sampling_us_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sampling_us_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t threshold_mC_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t threshold_mC_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t stats_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

struct kobj_attribute attr_sampling_us = __ATTR(sampling_us, 0660, sampling_us_show,sampling_us_store);
struct kobj_attribute attr_threshold_mC = __ATTR(threshold_mC, 0660, threshold_mC_show,threshold_mC_store);
struct kobj_attribute attr_mode= __ATTR(mode, 0660, mode_show,mode_store);
struct kobj_attribute attr_stats = __ATTR(stats, 0440, stats_show,stats_store);

// Probe and remove functions
static int nxp_simtemp_probe(struct platform_device *pdev);
static int nxp_simtemp_remove(struct platform_device *pdev);


static struct attribute *sim_temp_attrs[] = {
	&attr_sampling_us.attr,
	&attr_threshold_mC.attr,
	&attr_mode.attr,
	&attr_stats.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = sim_temp_attrs,
};

//File operation structure
static struct file_operations fops =
{
	.owner		= THIS_MODULE,
	.read		= nxp_simtemp_read,
	.write		= nxp_simtemp_write,
	.poll		= nxp_simtemp_poll,
	.open		= nxp_simtemp_open,
	.release	= nxp_simtemp_release,
};

static struct of_device_id nxp_simtemp_ids[]= {
	{
		.compatible = "nxp,simtemp",
	}, {/* sentinel */}
};

MODULE_DEVICE_TABLE(of,nxp_simtemp_ids);

static struct platform_driver nxp_simtemp_driver = {
	.probe = nxp_simtemp_probe,
	.remove = nxp_simtemp_remove,
	.driver = {
		.name = "nxp_simtemp_driver",
		.of_match_table = nxp_simtemp_ids,
	},
};

//Function called when loading the driver
static int nxp_simtemp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	
	int sampling_us_dt, threshold_mC_dt, ret = 0;
	
	printk("nxp_simtemp: Probe function\n");
	
	if(!device_property_present(dev,"sampling-ms"))
	{
		printk("nxp_simtemp: Device property sampling-ms not found\n");
		return -1;
	}
	if(!device_property_present(dev,"threshold-mc"))
	{
		printk("nxp_simtemp: Device property threshold-mc not found\n");
		return -1;
	}
	
	ret = of_property_read_u32(np, "sampling-ms", &sampling_us_dt);
	if(ret)
	{
		printk("nxp_simtemp: Could not read samplings-ms from DT\n");
		return ret;
	}
	
	if(sampling_us_dt > TIME_MAX_us || sampling_us_dt < TIME_MIN_us)
	{
		printk("nxp_simtemp: samplings-ms from DT out of range\n");
		return -EINVAL;
	}
	
	sampling_us = sampling_us_dt;
	
	printk("nxp_simtemp: from DT sampling_us = %u\n",sampling_us);
	
	ret = of_property_read_s32(np, "threshold-mc", &threshold_mC_dt);
	if(ret)
	{
		printk("nxp_simtemp: Could not read threshold-mc from DT\n");
		return ret;
	}
	if(threshold_mC_dt > TEMP_MAX || threshold_mC_dt < TEMP_MIN)
	{
		printk("nxp_simtemp: threshold-mc from DT out of range\n");
		return -EINVAL;
	}
	
	threshold_mC = threshold_mC_dt;
	
	printk("nxp_simtemp: from DT threshold_mC = %d\n",threshold_mC);
	
	return 0;
}

//Function called on unloading the driver

static int nxp_simtemp_remove(struct platform_device *pdev)
{
	printk("nxp_simtemp: Remove function\n");
	return 0;
}


//Function called when we read the sysfs file
static ssize_t sampling_us_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	pr_info("nxp_simtemp: sampling_us - Read\n");
	return sprintf(buf,"%u\n",sampling_us);
}

static ssize_t sampling_us_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long flags;
	__u32 sampling_us_temp;
	
	pr_info("nxp_simtemp: sampling_us - Write\n");
	ret = sscanf(buf,"%u",&sampling_us_temp);
	if(ret)
	{
		if(sampling_us_temp > TIME_MAX_us || sampling_us_temp < TIME_MIN_us)
		{
			spin_lock_irqsave(&flags_lock,flags);
			e_flags.l_error = E_OR_S_US;
			spin_unlock_irqrestore(&flags_lock,flags);
			return -EINVAL;
		}
		mutex_lock(&sampling_us_lock);
		sampling_us = sampling_us_temp;
		mutex_unlock(&sampling_us_lock);
		//Cancel timer
		hrtimer_cancel(&my_timer);
		//Update timer
		kt_period = ktime_set(0,sampling_us*1000);
		//Restart the timer
		hrtimer_start(&my_timer,kt_period,HRTIMER_MODE_REL);
		return count;
	}
	else
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_EV_S_US;
		spin_unlock_irqrestore(&flags_lock,flags);
		return -EINVAL;
	}
}

static ssize_t threshold_mC_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	pr_info("nxp_simtemp: threshold_mC - Read\n");
	return sprintf(buf,"%d\n",threshold_mC);
}

static ssize_t threshold_mC_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long flags;
	__s32 threshold_mC_temp;
	
	pr_info("nxp_simtemp: threshold_mC - Write\n");
	ret = sscanf(buf,"%d",&threshold_mC_temp);
	
	if(ret)
	{
		if(threshold_mC_temp > TEMP_MAX || threshold_mC_temp < TEMP_MIN)
		{
			spin_lock_irqsave(&flags_lock,flags);
			e_flags.l_error = E_OR_S_US;
			spin_unlock_irqrestore(&flags_lock,flags);
			return -EINVAL;
		}
		mutex_lock(&threshold_mC_lock);
		threshold_mC = threshold_mC_temp;
		mutex_unlock(&threshold_mC_lock);
		return count;
	}
	else
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_EV_TH;
		spin_unlock_irqrestore(&flags_lock,flags);
		return -EINVAL;
	}
}

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	pr_info("nxp_simtemp: mode - Read\n");
	return sprintf(buf,"%s\n",modes[mode]);
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	char tmp_mode;
	unsigned long flags;
	
	pr_info("nxp_simtemp: mode - Write\n");
	sscanf(buf,"%c",&tmp_mode);
	
	switch(tmp_mode)
	{
		case '0':
			mutex_lock(&mode_lock);
			mode = MODE_NRM;
			TEMP_STD_mC = 100;
			mutex_unlock(&mode_lock);
			pr_info("nxp_simtemp: New mode %d : %s",mode,modes[mode]);
			break;
		case '1':
			mutex_lock(&mode_lock);
			mode = MODE_NSY;
			TEMP_STD_mC = 2000;
			mutex_unlock(&mode_lock);
			pr_info("nxp_simtemp: New mode %d : %s",mode,modes[mode]);
			break;
		case '2':
			mutex_lock(&mode_lock);
			mode = MODE_RMP;
			mutex_unlock(&mode_lock);
			pr_info("nxp_simtemp: New mode %d : %s",mode,modes[mode]);
			break;
		default:
			pr_info("nxp_simtemp: Invalid mode %d",mode);
			spin_lock_irqsave(&flags_lock,flags);
			e_flags.l_error = E_EV_MD;
			spin_unlock_irqrestore(&flags_lock,flags);
			return -EINVAL;
	}
	
	return count;
}

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned long flags;
	int ret = 0;
	
	spin_lock_irqsave(&flags_lock,flags);
	ret = sprintf(buf," Counter:\t%llu\n Alerts:\t%llu\n Last_error:\t%s\n",e_flags.counter,e_flags.alert,sim_errors[e_flags.l_error-10]);
	spin_unlock_irqrestore(&flags_lock,flags);
	
	pr_info("nxp_simtemp: flags - Read\n");
	
	return ret; 
}

static ssize_t stats_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	pr_info("nxp_simtemp: flags - Write not available\n");
	return count;
}

static int nxp_simtemp_open(struct inode *inode,struct file *file)
{
	pr_info("nxp_simtemp: Device File Opened \n");
	return 0;
}

static int nxp_simtemp_release(struct inode *inode, struct file *file)
{
	pr_info("nxp_simtemp: Device File Closed \n");
	return 0;
}

static ssize_t nxp_simtemp_read(struct file *file, char __user *buf, size_t len, loff_t* off)
{
	unsigned long flags;
	
	spin_lock_irqsave(&fifo_lock,flags);
	kfifo_out(&CBuffer,&current_sample,1);
	spin_unlock_irqrestore(&fifo_lock,flags);
	
	copy_to_user((void __user *)buf,&current_sample,sizeof(current_sample));
	return sizeof(current_sample);
}

static ssize_t nxp_simtemp_write(struct file *file, const char *buf, size_t len, loff_t* off)
{
	pr_info("nxp_simtemp: Write function not available\n");
	return len;
}

static unsigned int nxp_simtemp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	unsigned long flags;
	
	poll_wait(file,&wq,wait);
	//pr_info("nxp_simtemp: Set POLLIN | POLLRDNORM flags\n");
	
	spin_lock_irqsave(&fifo_lock,flags);
	if(!kfifo_is_empty(&CBuffer))
	{
		mask |= POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&fifo_lock,flags);
	
	return mask;
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct simtemp_sample old_value;
	unsigned long flags;
	
	if(mode == MODE_NRM || mode == MODE_NSY)
	{
		current_sample.temp_mC = gaussian_s32_clt(TEMP_MEAN_mC,TEMP_STD_mC);
	}
	else
	{
		current_sample.temp_mC = ((current_sample.temp_mC + 1000 - TEMP_MIN) % (TEMP_MAX - TEMP_MIN + 1)) + TEMP_MIN;
	}
	current_sample.timestamp_ns = ktime_get_real_ns();
	current_sample.flags = FLAG_NEW_SAMPLE;
	
	spin_lock_irqsave(&flags_lock,flags);
	e_flags.counter += 1;
	spin_unlock_irqrestore(&flags_lock,flags);
	
	if (current_sample.temp_mC > threshold_mC)
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.alert += 1;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		current_sample.flags |= FLAG_THRESHOLD_CROSSED; 
	}
	
	spin_lock_irqsave(&fifo_lock,flags);
	
	if(kfifo_is_full(&CBuffer))
	{
		kfifo_out(&CBuffer,&old_value,1);
		//pr_info("nxp_simtemp: CBuffer full, oldest value %d dropped\n", old_value);
	}
	kfifo_in(&CBuffer,&current_sample,1);
	//pr_info("nxp_simtemp: %llu | Inserted %d into CBuffer\n",current_sample.timestamp_ns,current_sample.temp_mC);
	spin_unlock_irqrestore(&fifo_lock,flags);
	
	wake_up_interruptible(&wq);
	
	//Re-arm the timer
	hrtimer_forward_now(timer,kt_period);
	return HRTIMER_RESTART;
}

static int __init nxp_simtemp_init(void)
{
	unsigned long flags;
	
	// Load the driver
	if(platform_driver_register(&nxp_simtemp_driver))
	{
		printk("nxp_simtemp: Error.Could not load the driver\n");
		return -1;
	}
	
	// Init spinlocks
	spin_lock_init(&fifo_lock);
	spin_lock_init(&flags_lock);
	
	// Set the timer interval
	kt_period = ktime_set(0,sampling_us*1000); // seconds, nanoseconds
	
	// Initialize the hrtimer
	hrtimer_init(&my_timer,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
	my_timer.function = timer_callback;
	
	//Allocate Major Number
	if((alloc_chrdev_region(&dev, 0, 1,"simtemp"))<0)
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_MN_ALLOC;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_info("nxp_simtemp: Cannot allocate major number\n");
		return -1;
	}
	pr_info("nxp_simtemp: Major = %d Minor = %d \n", MAJOR(dev),MINOR(dev));
	
	//Create cdev structure
	cdev_init(&k_cdev,&fops);
	
	//Adding character device to the system
	if((cdev_add(&k_cdev,dev,1)) < 0)
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_DV_ADD;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_info("nxp_simtemp: Cannot add the device to the system\n");
		goto r_class;
	}
	
	//Create struct class
	if(IS_ERR(dev_class = class_create(THIS_MODULE,"simtemp")))
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_CL_CREATE;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_info("nxp_simtemp: Cannot create the struct class\n");
		goto r_class;
	}
	
	//Create device
	if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"simtemp")))
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_DV_CREATE;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_info("nxp_simtemp: Cannot create the Device 1\n");
		goto r_device;
		
	}
	
	//Create a directory in /sys/kernel/
	kobj_ref = kobject_create_and_add("simtemp",kernel_kobj);
	if(!kobj_ref)
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_KO_CREATE;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_err("Cannot create kobject\n");
		return -ENOMEM;
	}
	
	//Create sysfs file for k_value
	if(sysfs_create_group(kobj_ref,&attr_group))
	{
		spin_lock_irqsave(&flags_lock,flags);
		e_flags.l_error = E_SYS_CREATE;
		spin_unlock_irqrestore(&flags_lock,flags);
		
		pr_err("Cannot create sysfs group\n");
		goto r_sysfs;			
	}
	
	//Start the timer
	hrtimer_start(&my_timer,kt_period,HRTIMER_MODE_REL);
	
	pr_info("nxp_simtemp: Device Driver Insert Done\n");
	return 0;

r_sysfs:
	sysfs_remove_group(kobj_ref,&attr_group);
	kobject_put(kobj_ref);
	return -ENOMEM;
r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	cdev_del(&k_cdev);
	return -1;
}

static void __exit nxp_simtemp_exit(void)
{
	struct simtemp_sample val;
	int ret = 0;
	int i = 1;
	unsigned long flags;
	
	//Cancel the timer if it's active
	ret = hrtimer_cancel(&my_timer);
	if(ret)
		pr_info("nxp_simtemp: Timer was still active and canceled\n");
	
	//Drain and free FIFO
	spin_lock_irqsave(&fifo_lock,flags);
	while(!kfifo_is_empty(&CBuffer))
	{
		kfifo_out(&CBuffer,&val,1);
		pr_info("nxp_simtemp: %d Drained %d\n",i++,val.temp_mC);
	}
	spin_unlock_irqrestore(&fifo_lock,flags);
	
	sysfs_remove_group(kobj_ref,&attr_group);
	kobject_put(kobj_ref);
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&k_cdev);
	unregister_chrdev_region(dev,1);
	platform_driver_unregister(&nxp_simtemp_driver);
	pr_info("nxp_simtemp: Device Driver Remove Done\n");
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LASEC TECHNOLOGIES SYSTEMS");
MODULE_DESCRIPTION("Simple Linux device driver (sysfs)");
MODULE_VERSION("0.1");
