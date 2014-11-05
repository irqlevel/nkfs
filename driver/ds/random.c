#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <ds.h>
#include <ds_cmd.h>
#include <klog.h>
#include <ksocket.h>
#include <ds_priv.h>


#define __SUBCOMPONENT__ "ds-random"
#define __LOGNAME__ "ds.log"

static struct file *dev_random;
static struct file *dev_urandom;

int ds_random_buf_read(void *buf, __u32 len, int urandom)
{
	loff_t off = 0;
	int err;

	err = file_read((urandom) ? dev_urandom : dev_random, buf, len, &off);
	return err;
}

int ds_random_init(void)
{
	dev_random = filp_open("/dev/random", O_RDONLY, 0);
	if (!dev_random)
		return -ENOMEM;
	dev_urandom = filp_open("/dev/urandom", O_RDONLY, 0);
	if (!dev_urandom) {
		fput(dev_random);
		return -ENOMEM;
	}

	return 0;
}

void ds_random_release(void)
{
	fput(dev_random);
	fput(dev_urandom);
}

