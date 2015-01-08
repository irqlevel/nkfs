#include <inc/ds_priv.h>

MODULE_LICENSE("GPL");

#define __SUBCOMPONENT__ "mod"
#define DS_MISC_DEV_NAME "ds_ctl"

static int ds_mod_get(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE)) {
		KLOG(KL_ERR, "cant ref module");
		return -EINVAL;
	}
	return 0;
}

static int ds_mod_put(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static long ds_ioctl(struct file *file, unsigned int code, unsigned long arg)
{
	int err = -EINVAL;
	struct ds_cmd *cmd = NULL;	

	cmd = kmalloc(sizeof(struct ds_cmd), GFP_NOFS);
	if (!cmd) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, (const void *)arg, sizeof(struct ds_cmd))) {
		err = -EFAULT;
		goto out_free_cmd;
	}

	switch (code) {
		case IOCTL_DS_DEV_ADD:
			err = ds_dev_add(cmd->u.dev_add.dev_name,
					cmd->u.dev_add.format);
			break;
		case IOCTL_DS_DEV_REMOVE:
			err = ds_dev_remove(cmd->u.dev_remove.dev_name);
			break;
		case IOCTL_DS_DEV_QUERY: {
				struct ds_dev *dev;
				dev = ds_dev_lookup(cmd->u.dev_query.dev_name);
				if (dev) {
					BUG_ON(!dev->sb);
					ds_obj_id_copy(&cmd->u.dev_query.sb_id,
						&dev->sb->id);
					ds_dev_deref(dev);
					err = 0;
				} else {
					err = -ENOENT;
				}
			}
			break;
		case IOCTL_DS_SRV_START:
			err = ds_server_start(cmd->u.server_start.port);	
			break;
		case IOCTL_DS_SRV_STOP:
			err = ds_server_stop(cmd->u.server_stop.port);
			break;
		case IOCTL_DS_OBJ_INSERT: {
				struct ds_sb *sb;
				sb = ds_sb_lookup(&cmd->u.obj_insert.sb_id);
				if (sb) {
					err = ds_sb_insert_obj(sb,
						&cmd->u.obj_insert.obj_id,
						cmd->u.obj_insert.value,
						cmd->u.obj_insert.replace);
					ds_sb_deref(sb);
				} else {
					err = -EINVAL;
				}
			}
			break;
		case IOCTL_DS_OBJ_FIND: {
				struct ds_sb *sb;
				sb = ds_sb_lookup(&cmd->u.obj_find.sb_id);
				if (sb) {
					err = ds_sb_find_obj(sb,
						&cmd->u.obj_find.obj_id,
						&cmd->u.obj_find.value);
					ds_sb_deref(sb);
				} else {
					err = -EINVAL;
				}
			}
			break;
		case IOCTL_DS_OBJ_DELETE:{
				struct ds_sb *sb;
				sb = ds_sb_lookup(&cmd->u.obj_delete.sb_id);
				if (sb) {
					err = ds_sb_delete_obj(sb,
						&cmd->u.obj_delete.obj_id);
					ds_sb_deref(sb);
				} else {
					err = -EINVAL;
				}
			}
			break;
		default:
			KLOG(KL_ERR, "unknown ioctl=%d", code);
			err = DS_E_UNK_IOCTL;
			break;
	}
	cmd->err = err;
	if (copy_to_user((void *)arg, cmd, sizeof(struct ds_cmd))) {
		err = -EFAULT;
		goto out_free_cmd;
	}

	KLOG(KL_DBG, "ctl %d err %d", code, err);
	err = 0;
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
	
	err = klog_init();
	if (err) {
		printk(KERN_ERR "KLOG_init failed with err=%d", err);
		goto out;
	}

	KLOG(KL_DBG, "initing");

	err = ds_random_init();
	if (err) {
		KLOG(KL_ERR, "ds_random_init err %d", err);
		goto out_klog_release;
	}
	err = amap_sys_init();
	if (err) {
		KLOG(KL_ERR, "amap_sys_init err %d", err);
		goto out_random_release;
	}

	err = misc_register(&ds_misc);
	if (err) {
		KLOG(KL_ERR, "misc_register err=%d", err);
		goto out_amap_release; 
	}
	err = btree_init();
	if (err) {
		KLOG(KL_ERR, "btree_init err %d", err);
		goto out_misc_release;
	}
	err = ds_sb_init();
	if (err) {
		KLOG(KL_ERR, "ds_sb_init err %d", err);
		goto out_btree_release;
	}
	err = ds_dev_init();
	if (err) {
		KLOG(KL_ERR, "ds_dev_init err %d", err);
		goto out_sb_release;
	}

	KLOG(KL_DBG, "inited");

#if __SHA_TEST__
	__sha256_test();
#endif

#if __BTREE_TEST__
	btree_test(100000);
#endif


	return 0;

out_sb_release:
	ds_sb_finit();
out_btree_release:
	btree_finit();
out_misc_release:
	misc_deregister(&ds_misc);
out_amap_release:
	amap_sys_release();
out_random_release:
	ds_random_release();
out_klog_release:
	klog_release();
out:
	return err;
}

static void __exit ds_exit(void)
{
	KLOG(KL_DBG, "exiting");

	ds_random_release();	
	misc_deregister(&ds_misc);
	ds_server_stop_all();
	ds_dev_finit();
	ds_sb_finit();
	btree_finit();
	amap_sys_release();

	KLOG(KL_DBG, "exited");
	klog_release();
}

module_init(ds_init);
module_exit(ds_exit);

