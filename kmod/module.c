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

static int ds_ioctl_put_obj(struct ds_ctl *cmd)
{
	struct ds_user_pages up;
	int err;

	err = ds_get_user_pages((unsigned long)cmd->u.put_obj.buf,
				cmd->u.put_obj.len, 0, &up);
	if (err) {
		KLOG(KL_ERR, "cant get user pages at %p %d",
			cmd->u.put_obj.buf, cmd->u.put_obj.len);
		goto out;
	}

	err = ds_sb_list_put_obj(&cmd->u.put_obj.obj_id,
		cmd->u.put_obj.off,
		up.pages,
		up.nr_pages);

	ds_release_user_pages(&up);
out:
	return err;
}

static int ds_ioctl_get_obj(struct ds_ctl *cmd)
{
	struct ds_user_pages up;
	int err;

	err = ds_get_user_pages((unsigned long)cmd->u.get_obj.buf,
				cmd->u.get_obj.len, 1, &up);
	if (err) {
		KLOG(KL_ERR, "cant get user pages at %p %d",
			cmd->u.get_obj.buf, cmd->u.get_obj.len);
		goto out;
	}

	err = ds_sb_list_get_obj(&cmd->u.get_obj.obj_id,
		cmd->u.get_obj.off,
		up.pages,
		up.nr_pages);

	ds_release_user_pages(&up);
out:
	return err;
}

static int ds_ioctl_delete_obj(struct ds_ctl *cmd)
{
	return ds_sb_list_delete_obj(&cmd->u.delete_obj.obj_id);
}

static int ds_ioctl_create_obj(struct ds_ctl *cmd)
{
	return ds_sb_list_create_obj(&cmd->u.create_obj.obj_id);
}

static long ds_ioctl(struct file *file, unsigned int code, unsigned long arg)
{
	int err = -EINVAL;
	struct ds_ctl *cmd = NULL;	

	cmd = kmalloc(sizeof(struct ds_ctl), GFP_NOIO);
	if (!cmd) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, (const void *)arg, sizeof(struct ds_ctl))) {
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
			err = ds_server_start(cmd->u.server_start.ip,
				cmd->u.server_start.port);	
			break;
		case IOCTL_DS_SRV_STOP:
			err = ds_server_stop(cmd->u.server_stop.ip,
				cmd->u.server_stop.port);
			break;
		case IOCTL_DS_PUT_OBJ:
			err = ds_ioctl_put_obj(cmd);	
			break;
		case IOCTL_DS_GET_OBJ:
			err = ds_ioctl_get_obj(cmd);
			break;
		case IOCTL_DS_DELETE_OBJ:
			err = ds_ioctl_delete_obj(cmd);
			break;
		case IOCTL_DS_CREATE_OBJ:
			err = ds_ioctl_create_obj(cmd);
			break;
		default:
			KLOG(KL_ERR, "unknown ioctl=%d", code);
			err = DS_E_UNK_IOCTL;
			break;
	}
	cmd->err = err;
	if (copy_to_user((void *)arg, cmd, sizeof(struct ds_ctl))) {
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
	
	KLOG(KL_INF, "initing");
	
	err = amap_sys_init();
	if (err) {
		KLOG(KL_ERR, "amap_sys_init err %d", err);
		goto out;
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
	err = ds_inode_init();
	if (err) {
		KLOG(KL_ERR, "inode_init err %d", err);
		goto out_btree_release;
	}
	err = ds_sb_init();
	if (err) {
		KLOG(KL_ERR, "ds_sb_init err %d", err);
		goto out_inode_release;
	}
	err = ds_dev_init();
	if (err) {
		KLOG(KL_ERR, "ds_dev_init err %d", err);
		goto out_sb_release;
	}
	err = ds_server_init();
	if (err) {
		KLOG(KL_ERR, "ds_server_init err %d", err);
		goto out_dev_release;
	}

	KLOG(KL_INF, "inited");

	return 0;

out_dev_release:
	ds_dev_finit();
out_sb_release:
	ds_sb_finit();
out_inode_release:
	ds_inode_finit();
out_btree_release:
	btree_finit();
out_misc_release:
	misc_deregister(&ds_misc);
out_amap_release:
	amap_sys_release();
out:
	return err;
}

static void __exit ds_exit(void)
{
	KLOG(KL_INF, "exiting");

	misc_deregister(&ds_misc);
	ds_server_finit();
	ds_dev_finit();
	ds_sb_finit();
	btree_finit();
	ds_inode_finit();
	amap_sys_release();

	KLOG(KL_INF, "exited");
}

module_init(ds_init);
module_exit(ds_exit);

