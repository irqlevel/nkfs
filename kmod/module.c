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
		case IOCTL_DS_DEV_QUERY:
			err = ds_dev_query(cmd->u.dev_query.dev_name,
				&cmd->u.dev_query.info);
			break;
		case IOCTL_DS_SRV_START:
			err = ds_server_start(cmd->u.server_start.ip,
				cmd->u.server_start.port);	
			break;
		case IOCTL_DS_SRV_STOP:
			err = ds_server_stop(cmd->u.server_stop.ip,
				cmd->u.server_stop.port);
			break;
		case IOCTL_DS_NEIGH_ADD:
			err = ds_neigh_add(cmd->u.neigh_add.d_ip,
				cmd->u.neigh_add.d_port,
				cmd->u.neigh_add.s_ip,
				cmd->u.neigh_add.s_port);
			break;
		case IOCTL_DS_NEIGH_REMOVE:
			err = ds_neigh_remove(cmd->u.neigh_remove.d_ip,
				cmd->u.neigh_remove.d_port);
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

	err = ds_route_init();
	if (err) {
		KLOG(KL_ERR, "ds_route_init err %d", err);
		goto out_srv_release;
	}

	KLOG(KL_INF, "inited");

	return 0;

out_srv_release:
	ds_server_finit();
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
	ds_route_finit();
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

