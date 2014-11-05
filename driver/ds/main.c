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

MODULE_LICENSE("GPL");

#define __SUBCOMPONENT__ "ds-main"
#define __LOGNAME__ "ds.log"

#define DS_MISC_DEV_NAME "ds_ctl"

static int ds_mod_get(struct inode *inode, struct file *file)
{
	klog(KL_DBG, "in open");
	if (!try_module_get(THIS_MODULE)) {
		klog(KL_ERR, "cant ref module");
		return -EINVAL;
	}
	klog(KL_DBG, "opened");
	return 0;
}

static int ds_mod_put(struct inode *inode, struct file *file)
{
	klog(KL_DBG, "in release");
	module_put(THIS_MODULE);
	klog(KL_DBG, "released");
	return 0;
}

static long ds_ioctl(struct file *file, unsigned int code, unsigned long arg)
{
	int err = -EINVAL;
	struct ds_cmd *cmd = NULL;	

	klog(KL_DBG, "ctl code %d", code);

	cmd = kmalloc(sizeof(struct ds_cmd), GFP_KERNEL);
	if (!cmd) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, (const void *)arg, sizeof(struct ds_cmd))) {
		err = -EFAULT;
		goto out_free_cmd;
	}

	klog(KL_DBG, "ctl code %d", code);	
	switch (code) {
		case IOCTL_DS_DEV_ADD:
			err = ds_dev_add(cmd->u.dev_add.dev_name);
			break;
		case IOCTL_DS_DEV_REMOVE:
			err = ds_dev_remove(cmd->u.dev_remove.dev_name);
			break;
		case IOCTL_DS_SRV_START:
			err = ds_server_start(cmd->u.server_start.port);	
			break;
		case IOCTL_DS_SRV_STOP:
			err = ds_server_stop(cmd->u.server_stop.port);
			break;
		default:
			klog(KL_ERR, "unknown ioctl=%d", code);
			err = -EINVAL;
			break;
	}
	cmd->err = err;
	if (copy_to_user((void *)arg, cmd, sizeof(struct ds_cmd))) {
		err = -EFAULT;
		goto out_free_cmd;
	}
	
	return 0;
out_free_cmd:
	kfree(cmd);
out:
	return err;	
}

static const struct file_operations ds_fops = {
	.owner = THIS_MODULE,
	.open = ds_mod_get,
	.release = ds_mod_put,
	.unlocked_ioctl = ds_ioctl,
};

static struct miscdevice ds_misc = {
	.fops = &ds_fops,
	.minor = MISC_DYNAMIC_MINOR,
	.name = DS_MISC_DEV_NAME,	
};


static int __init ds_init(void)
{	
	int err = -EINVAL;
	
	err = klog_init(KL_DBG_L);
	if (err) {
		printk(KERN_ERR "klog_init failed with err=%d", err);
		goto out;
	}

	klog(KL_DBG, "initing");

	err = misc_register(&ds_misc);
	if (err) {
		klog(KL_ERR, "misc_register err=%d", err);
		goto out_klog_release; 
	}

	klog(KL_DBG, "inited");
	return 0;
out_klog_release:
	klog_release();
out:
	return err;
}

static void __exit ds_exit(void)
{
	klog(KL_DBG, "exiting");
	
	misc_deregister(&ds_misc);
	ds_server_stop_all();
	ds_dev_release_all();
	klog(KL_DBG, "exited");
	klog_release();
}

module_init(ds_init);
module_exit(ds_exit);

