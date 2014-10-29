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

#define LISTEN_RESTART_TIMEOUT_MS 5000

#define DS_MISC_DEV_NAME "ds_ctl"

static DEFINE_MUTEX(srv_list_lock);
static LIST_HEAD(srv_list);

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

static struct ds_con *ds_con_start(struct ds_server *server, struct socket *sock)
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

static int ds_thread_routine(void *data)
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
				klog(KL_ERR, "listen done at port=%d", server->port);
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
					klog(KL_WRN, "csock_accept err=%d", err);
				else
					klog(KL_ERR, "csock_accept err=%d", err);
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
	klog(KL_DBG, "releasing listen socket");
	
	mutex_lock(&server->lock);
	lsock = server->sock;
	server->sock = NULL;
	mutex_unlock(&server->lock);

	if (lsock)
		ksock_release(lsock);
	
	klog(KL_DBG, "releasing cons");

	for (;;) {
		con = NULL;
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&server->con_list)) {
			con = list_first_entry(&server->con_list, struct ds_con, con_list);
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
	server->thread = kthread_create(ds_thread_routine, server, thread_name);
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
	struct task_struct *task = NULL;
	
	server->stopping = 1;
	mutex_lock(&server->lock);
	if (server->sock) {
		ksock_abort_accept(server->sock);
		server->sock = NULL;
	}
	if (server->thread) {
		task = server->thread;
		server->thread = NULL;
	}
	mutex_unlock(&server->lock);
	
	if (task) {
		kthread_stop(task);
		put_task_struct(task);
	}
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

static void ds_stop_all(void)
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

	cmd = kmalloc(sizeof(struct ds_cmd), GFP_KERNEL);
	if (!cmd) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, (const void *)arg, sizeof(struct ds_cmd))) {
		err = -EFAULT;
		goto out_free_cmd;
	}
	
	switch (code) {
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
	ds_stop_all();

	klog(KL_DBG, "exited");
	klog_release();
}

module_init(ds_init);
module_exit(ds_exit);

