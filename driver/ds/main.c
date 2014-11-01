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
#include <ds_priv.h>
#include <ds_cmd.h>
#include <klog.h>
#include <ksocket.h>

MODULE_LICENSE("GPL");

#define __SUBCOMPONENT__ "ds"
#define __LOGNAME__ "ds.log"

#define LISTEN_RESTART_TIMEOUT_MS 2000

#define DS_MISC_DEV_NAME "ds_ctl"

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);

static DEFINE_MUTEX(srv_list_lock);
static LIST_HEAD(srv_list);

struct ds_dev {
	char			*dev_name;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	struct task_struct  	*thread;
	int			stopping;
};

struct ds_con {
	struct task_struct 	*thread;
	struct socket 		*sock;
	struct list_head	con_list;
	struct ds_server	*server;
};

struct ds_server {
	struct task_struct	*thread;
	struct socket		*sock;
	struct mutex		lock;
	struct	list_head	srv_list;
	struct	list_head	con_list;
	struct	mutex		con_list_lock;
	int			port;
	int			stopping;
};

static void ds_con_wait(struct ds_con *con)
{
	kthread_stop(con->thread);
}

static void ds_con_free(struct ds_con *con)
{
	klog(KL_DBG, "releasing sock %p", con->sock);
	ksock_release(con->sock);
	put_task_struct(con->thread);
	kfree(con);
}

static int ds_con_thread_routine(void *data)
{
	struct ds_con *con = (struct ds_con *)data;
	struct ds_server *server = con->server;

	BUG_ON(con->thread != current);

	klog(KL_DBG, "inside con thread %p, sock %p", con->thread, con->sock);


	klog(KL_DBG, "closing sock %p", con->sock);
	if (!server->stopping) {
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&con->con_list))
			list_del_init(&con->con_list);	
		else
			con = NULL;
		mutex_unlock(&server->con_list_lock);

		if (con)
			ds_con_free(con);
	}

	return 0;
}

static struct ds_con *ds_con_start(struct ds_server *server,
	struct socket *sock)
{
	struct ds_con *con = kmalloc(sizeof(struct ds_con), GFP_KERNEL);
	int err = -EINVAL;
	if (!con) {
		klog(KL_ERR, "cant alloc ds_con");
		return NULL;
	}

	con->server = server;
	con->thread = NULL;
	con->sock = sock;
	con->thread = kthread_create(ds_con_thread_routine, con, "ds_con");
	if (IS_ERR(con->thread)) {
		err = PTR_ERR(con->thread);
		con->thread = NULL;
		klog(KL_ERR, "kthread_create err=%d", err);
		goto out;
	}

	get_task_struct(con->thread);	
	mutex_lock(&server->con_list_lock);
	list_add_tail(&con->con_list, &server->con_list);
	mutex_unlock(&server->con_list_lock);

	wake_up_process(con->thread);

	return con;	
out:
	kfree(con);
	return NULL;
}

static int ds_server_thread_routine(void *data)
{
	struct ds_server *server = (struct ds_server *)data;
	struct socket *lsock = NULL;
	struct socket *con_sock = NULL;
	struct ds_con *con = NULL;
	int err = 0;

	if (server->thread != current)
		BUG_ON(1);

	while (!kthread_should_stop()) {
		if (!server->sock) {
			err = ksock_listen(&lsock, INADDR_ANY, server->port, 5);
			if (err) {
				klog(KL_ERR, "csock_listen err=%d", err);
				msleep_interruptible(LISTEN_RESTART_TIMEOUT_MS);
				continue;
			} else {
				klog(KL_ERR, "listen done at port=%d",
						server->port);
				mutex_lock(&server->lock);
				server->sock = lsock;
				mutex_unlock(&server->lock);
			}
		}

		if (server->sock && !server->stopping) {
			klog(KL_DBG, "accepting");
			err = ksock_accept(&con_sock, server->sock);
			if (err) {
				if (err == -EAGAIN)
					klog(KL_WRN, "accept err=%d", err);
				else
					klog(KL_ERR, "accept err=%d", err);
				continue;
			}
			klog(KL_DBG, "accepted con_sock=%p", con_sock);

			if (!ds_con_start(server, con_sock)) {
				klog(KL_ERR, "ds_con_start failed");
				ksock_release(con_sock);
				continue;
			}
		}
	}

	err = 0;
	mutex_lock(&server->lock);
	lsock = server->sock;
	klog(KL_DBG, "releasing listen socket=%p", lsock);
	server->sock = NULL;
	mutex_unlock(&server->lock);

	if (lsock)
		ksock_release(lsock);
	
	klog(KL_DBG, "releasing cons");

	for (;;) {
		con = NULL;
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&server->con_list)) {
			con = list_first_entry(&server->con_list, struct ds_con,
					con_list);
			list_del_init(&con->con_list);		
		}
		mutex_unlock(&server->con_list_lock);
		if (!con)
			break;

		ds_con_wait(con);
		ds_con_free(con);
	}

	klog(KL_DBG, "released cons");	
	return 0;
}

struct ds_server *ds_server_create_start(int port)
{
	char thread_name[10];
	int err;
	struct ds_server *server;

	server = kmalloc(sizeof(struct ds_server), GFP_KERNEL);
	if (!server)
		return NULL;

	memset(server, 0, sizeof(*server));
	INIT_LIST_HEAD(&server->con_list);
	INIT_LIST_HEAD(&server->srv_list);
	mutex_init(&server->lock);
	mutex_init(&server->con_list_lock);
	server->port = port;

	snprintf(thread_name, sizeof(thread_name), "%s-%d", "ds_srv", port);
	server->thread = kthread_create(ds_server_thread_routine, server, thread_name);
	if (IS_ERR(server->thread)) {
		err = PTR_ERR(server->thread);
		server->thread = NULL;
		klog(KL_ERR, "kthread_create err=%d", err);
		kfree(server);
		return NULL;
	}
	get_task_struct(server->thread);
	wake_up_process(server->thread);
	
	return server;
}

static int ds_server_start(int port)
{
	int err;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port) {
			klog(KL_INF, "server for port %d already exists", port);
			err = -EEXIST;
			mutex_unlock(&srv_list_lock);
			return err;
		}
	}
	server = ds_server_create_start(port);
	if (server) {
		list_add_tail(&server->srv_list, &srv_list);
		err = 0;
	} else
		err = -ENOMEM;
	mutex_unlock(&srv_list_lock);

	return err;
}

static void ds_server_do_stop(struct ds_server *server)
{
	if (server->stopping) {
		klog(KL_ERR, "server %p-%d already stopping",
			server, server->port);
		return;
	}

	server->stopping = 1;
	if (server->sock) {
		ksock_abort_accept(server->sock);
	}

	kthread_stop(server->thread);
	put_task_struct(server->thread);
}

static int ds_server_stop(int port)
{
	int err = -EINVAL;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port) {
			ds_server_do_stop(server);
			list_del(&server->srv_list);
			kfree(server);
			err = 0;
			break;
		}
	}
	mutex_unlock(&srv_list_lock);
	return err;
}

static void ds_dev_free(struct ds_dev *dev)
{
	if (dev->dev_name)
		kfree(dev->dev_name);
	kfree(dev);
}

static int ds_dev_insert(struct ds_dev *cand)
{
	struct ds_dev *dev;
	int err;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, cand->dev_name,
			strlen(cand->dev_name)+1)) {
			err = -EEXIST;
			break;
		}
	}
	list_add_tail(&cand->dev_list, &dev_list);
	err = 0;
	mutex_unlock(&dev_list_lock);
	return err;
}

static void ds_dev_release(struct ds_dev *dev)
{
	klog(KL_DBG, "releasing dev=%p bdev=%p", dev, dev->bdev);

	if (dev->bdev)
		blkdev_put(dev->bdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
}

static void ds_dev_unlink(struct ds_dev *dev)
{
	mutex_lock(&dev_list_lock);
	list_del(&dev->dev_list);
	mutex_unlock(&dev_list_lock);
}

static struct ds_dev *ds_dev_lookup_unlink(char *dev_name)
{
	struct ds_dev *dev;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, dev_name,
			strlen(dev_name)+1)) {
			list_del(&dev->dev_list);
			mutex_unlock(&dev_list_lock);
			return dev;
		}
	}
	mutex_unlock(&dev_list_lock);
	return NULL;
}

static struct ds_dev *ds_dev_create(char *dev_name)
{
	struct ds_dev *dev;
	int len;
	int err;

	len = strlen(dev_name);
	if (len == 0) {
		klog(KL_ERR, "len=%d", len);
		return NULL;
	}

	dev = kmalloc(sizeof(struct ds_dev), GFP_KERNEL);
	if (!dev) {
		klog(KL_ERR, "dev alloc failed");
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));
	dev->dev_name = kmalloc(len + 1, GFP_KERNEL);
	if (!dev->dev_name) {
		klog(KL_ERR, "dev_name alloc failed");
		ds_dev_free(dev);
		return NULL;
	}

	memcpy(dev->dev_name, dev_name, len + 1);
	dev->bdev = blkdev_get_by_path(dev->dev_name,
		FMODE_READ|FMODE_WRITE|FMODE_EXCL, dev);
	if ((err = IS_ERR(dev->bdev))) {
		dev->bdev = NULL;
		klog(KL_ERR, "bkdev_get_by_path failed err %d", err);
		ds_dev_free(dev);
		
		return NULL;
	}

	return dev;
}

struct ds_dev_io {
	struct ds_dev 		*dev;
	struct bio    		*bio;
	int			err;
	int			(*clb)(struct ds_dev_io *io);
	struct completion	*complete;
}

static void ds_dev_io_free(struct ds_dev_io *io)
{
	kfree(io->bio->bi_io_vec);
	kfree(io->bio);
	if (io->complete)
		kfree(io->complete);
}

static void ds_dev_bio_end(struct bio *bio, int err)
{
	struct ds_dev_io *io = (struct ds_dev_io *)bio->bi_private;
	
	BUG_ON(bio != io->bio);
	io->err = err;

	if (io->clb)
		io->clb(io);

	if (io->complete)
		complete(io->complete);
	else
		ds_dev_io_free(io);
}

static int ds_dev_io_page(struct ds_dev *dev, struct page *page, int bi_flags,
		int rw_flags, void (*clb)(struct *ds_dev_io *io), int wait)
{
	struct bio *bio;
	struct bio_vec *bio_vec;
	struct ds_dev_io *io;

	bio = kmalloc(sizeof(struct bio), GFP_NOIO);
	if (!bio)
		return -ENOMEM;
	memset(bio, 0, sizeof(*bio));
	bio_vec = kmalloc(sizeof(struct bio), GFP_NOIO);
	if (!bio_vec) {
		kfree(bio);
		return -ENOMEM;
	}
	memset(bio_vec, 0, sizeof(*bio_vec));
	io = kmalloc(sizeof(struct ds_dev_io), GFP_NOIO);
	if (!io) {
		kfree(bio_vec);
		kfree(bio);
		return -ENOMEM;
	}
	memset(io, 0, sizeof(*io));
	if (wait) {
		io->complete = kmalloc(sizeof(struct complete), GFP_NOIO);
		if (!io->complete) {
			kfree(bio_vec);
			kfree(bio);
			kfree(io);			
		}
		memset(io->complete, 0, sizeof(struct complete));
		init_completion(io->complete);
	}
	io->dev = dev;
	io->bio = bio;
	io->clb = clb
	bio_init(bio);
	bio->bi_io_vec = bio_vec;
	bio->bi_io_vec->bv_page = page;
	bio->bi_io_vec->bv_len = PAGE_SIZE;
	bio->bi_io_vec->bv_offset = 0;
	bio->bi_vcnt = 1;
	bio->bi_iter.bi_size = PAGE_SIZE;
	bio->bi_bdev = dev->bdev;
	bio->bi_flags |= bi_flags;
	bio->bi_rw |= rw_flags;
	bio->bi_private = io;
	bio->bi_end_io = ds_dev_bio_end;

	generic_make_request(bio);

	if (io->complete) {
		wait_for_completion(io->complete);
		err = io->err;
		dev_io_free(io);
	} else {
		
	}
}

static int ds_dev_touch0_page(struct ds_dev *dev)
{
	struct page *page;
	void *page_va;
	page = alloc_page(GFP_NOIO);
	if (!page) {
		return -ENOMEM;
	}
	err = ds_dev_io_page(dev, page, 0, REQ_READ, NULL, 1);

}

static int ds_dev_thread_routine(void *data)
{
	struct ds_dev *dev = (struct ds_dev *)data;
	int err = 0;

	klog(KL_DBG, "dev %p thread starting");

	if (dev->thread != current)
		BUG_ON(1);

	err = ds_dev_touch0_page(dev);

	while (!kthread_should_stop()) {
		msleep_interruptible(100);
		if (dev->stopping)
			break;
	}

	klog(KL_DBG, "dev %p exiting");
	return err;
}

static int ds_dev_start(struct ds_dev *dev)
{
	int err;
	dev->thread = kthread_create(ds_dev_thread_routine, dev, "ds_dev_th");
	if (IS_ERR(dev->thread)) {
		err = PTR_ERR(dev->thread);
		dev->thread = NULL;
		klog(KL_ERR, "kthread_create err=%d", err);
		return err;
	}
	get_task_struct(dev->thread);
	wake_up_process(dev->thread);
	err = 0;
	return err;
}

static void ds_dev_stop(struct ds_dev *dev)
{
	dev->stopping = 1;
	if (dev->thread) {
		kthread_stop(dev->thread);
		put_task_struct(dev->thread);
	}
}

static int ds_dev_add(char *dev_name)
{
	int err;
	struct ds_dev *dev;

	klog(KL_DBG, "inserting dev %s", dev_name);
	dev = ds_dev_create(dev_name);
	if (!dev) {
		return -ENOMEM;
	}

	err = ds_dev_insert(dev);
	if (err) {
		klog(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	err = ds_dev_start(dev);
	if (err) {
		klog(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_unlink(dev);		
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	return err;
}

static int ds_dev_remove(char *dev_name)
{
	int err;
	struct ds_dev *dev;

	klog(KL_DBG, "removing dev %s", dev_name);
	dev = ds_dev_lookup_unlink(dev_name);
	if (dev) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		ds_dev_free(dev);
		err = 0;
	} else {
		klog(KL_ERR, "dev with name %s not found", dev_name);
		err = -ENOENT;
	}

	return err;
}

static void ds_dev_release_all(void)
{
	struct ds_dev *dev;
	struct ds_dev *tmp;
	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(dev, tmp, &dev_list, dev_list) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		list_del(&dev->dev_list);
		ds_dev_free(dev);
	}
	mutex_unlock(&srv_list_lock);
}

static void ds_server_stop_all(void)
{
	struct ds_server *server;
	struct ds_server *tmp;
	mutex_lock(&srv_list_lock);
	list_for_each_entry_safe(server, tmp, &srv_list, srv_list) {
		ds_server_do_stop(server);
		list_del(&server->srv_list);
		kfree(server);
	}
	mutex_unlock(&srv_list_lock);
}

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

