#include <inc/nkfs_priv.h>

MODULE_LICENSE("GPL");

#define __SUBCOMPONENT__ "mod"

static int nkfs_mod_get(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE)) {
		KLOG(KL_ERR, "cant ref module");
		return -EINVAL;
	}
	return 0;
}

static int nkfs_mod_put(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static int nkfs_klog_ctl(int level, int sync)
{
	if (sync)
		KLOG_SYNC();
	return 0;
}

static long nkfs_ioctl(struct file *file, unsigned int code, unsigned long arg)
{
	int err = -EINVAL;
	struct nkfs_ctl *cmd = NULL;	

	cmd = kmalloc(sizeof(struct nkfs_ctl), GFP_NOIO);
	if (!cmd) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, (const void *)arg, sizeof(struct nkfs_ctl))) {
		err = -EFAULT;
		goto out_free_cmd;
	}

	switch (code) {
		case IOCTL_NKFS_DEV_ADD:
			err = nkfs_dev_add(cmd->u.dev_add.dev_name,
					cmd->u.dev_add.format);
			break;
		case IOCTL_NKFS_DEV_REMOVE:
			err = nkfs_dev_remove(cmd->u.dev_remove.dev_name);
			break;
		case IOCTL_NKFS_DEV_QUERY:
			err = nkfs_dev_query(cmd->u.dev_query.dev_name,
				&cmd->u.dev_query.info);
			break;
		case IOCTL_NKFS_SRV_START:
			err = nkfs_server_start(cmd->u.server_start.ip,
				cmd->u.server_start.port);	
			break;
		case IOCTL_NKFS_SRV_STOP:
			err = nkfs_server_stop(cmd->u.server_stop.ip,
				cmd->u.server_stop.port);
			break;
		case IOCTL_NKFS_NEIGH_ADD:
			err = nkfs_route_neigh_add(cmd->u.neigh_add.ip,
				cmd->u.neigh_add.port);
			break;
		case IOCTL_NKFS_NEIGH_REMOVE:
			err = nkfs_route_neigh_remove(cmd->u.neigh_remove.ip,
				cmd->u.neigh_remove.port);
			break;
		case IOCTL_NKFS_KLOG_CTL:
			err = nkfs_klog_ctl(cmd->u.klog_ctl.level,
				cmd->u.klog_ctl.sync);
			break;
		default:
			KLOG(KL_ERR, "unknown ioctl=%d", code);
			err = NKFS_E_UNK_IOCTL;
			break;
	}
	cmd->err = err;
	if (copy_to_user((void *)arg, cmd, sizeof(struct nkfs_ctl))) {
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

static const struct file_operations nkfs_fops = {
	.owner = THIS_MODULE,
	.open = nkfs_mod_get,
	.release = nkfs_mod_put,
	.unlocked_ioctl = nkfs_ioctl,
};

static struct miscdevice nkfs_misc = {
	.fops = &nkfs_fops,
	.minor = MISC_DYNAMIC_MINOR,
	.name = NKFS_CTL_DEV_NAME,	
};

static int __init nkfs_init(void)
{	
	int err = -EINVAL;
	
	KLOG(KL_INF, "initing");
	
	err = dio_init();
	if (err) {
		KLOG(KL_ERR, "dio_init err %d", err);
		goto out;
	}

	err = misc_register(&nkfs_misc);
	if (err) {
		KLOG(KL_ERR, "misc_register err=%d", err);
		goto out_dio_release; 
	}

	err = nkfs_btree_init();
	if (err) {
		KLOG(KL_ERR, "btree_init err %d", err);
		goto out_misc_release;
	}
	
	err = nkfs_inode_init();
	if (err) {
		KLOG(KL_ERR, "inode_init err %d", err);
		goto out_btree_release;
	}

	err = nkfs_sb_init();
	if (err) {
		KLOG(KL_ERR, "nkfs_sb_init err %d", err);
		goto out_inode_release;
	}

	err = nkfs_dev_init();
	if (err) {
		KLOG(KL_ERR, "nkfs_dev_init err %d", err);
		goto out_sb_release;
	}

	err = nkfs_server_init();
	if (err) {
		KLOG(KL_ERR, "nkfs_server_init err %d", err);
		goto out_dev_release;
	}

	err = nkfs_route_init();
	if (err) {
		KLOG(KL_ERR, "nkfs_route_init err %d", err);
		goto out_srv_release;
	}

	KLOG(KL_INF, "inited");

	return 0;

out_srv_release:
	nkfs_server_finit();
out_dev_release:
	nkfs_dev_finit();
out_sb_release:
	nkfs_sb_finit();
out_inode_release:
	nkfs_inode_finit();
out_btree_release:
	nkfs_btree_finit();
out_misc_release:
	misc_deregister(&nkfs_misc);
out_dio_release:
	dio_finit();
out:
	return err;
}

static void __exit nkfs_exit(void)
{
	KLOG(KL_INF, "exiting");

	misc_deregister(&nkfs_misc);
	nkfs_route_finit();
	nkfs_server_finit();
	nkfs_dev_finit();
	nkfs_sb_finit();
	nkfs_btree_finit();
	nkfs_inode_finit();
	dio_finit();

	KLOG(KL_INF, "exited");
}

module_init(nkfs_init);
module_exit(nkfs_exit);

