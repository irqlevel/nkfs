#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "net"

#define LISTEN_RESTART_TIMEOUT_MS 2000

static DEFINE_MUTEX(srv_list_lock);
static LIST_HEAD(srv_list);

static void ds_con_wait(struct ds_con *con)
{
	kthread_stop(con->thread);
}

static void ds_con_free(struct ds_con *con)
{
	KLOG(KL_DBG, "releasing sock %p", con->sock);
	ksock_release(con->sock);
	put_task_struct(con->thread);
	kfree(con);
}

static int ds_con_thread_routine(void *data)
{
	struct ds_con *con = (struct ds_con *)data;
	struct ds_server *server = con->server;

	BUG_ON(con->thread != current);

	KLOG(KL_DBG, "inside con thread %p, sock %p", con->thread, con->sock);


	KLOG(KL_DBG, "closing sock %p", con->sock);
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
		KLOG(KL_ERR, "cant alloc ds_con");
		return NULL;
	}

	con->server = server;
	con->thread = NULL;
	con->sock = sock;
	con->thread = kthread_create(ds_con_thread_routine, con, "ds_con");
	if (IS_ERR(con->thread)) {
		err = PTR_ERR(con->thread);
		con->thread = NULL;
		KLOG(KL_ERR, "kthread_create err=%d", err);
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
	u32 listen_attempts = 10;

	if (server->thread != current)
		BUG_ON(1);

	while (!kthread_should_stop()) {
		if (!server->sock) {
			err = ksock_listen(&lsock, (server->ip) ? server->ip :
				INADDR_ANY, server->port, 5);
			if (err == EADDRINUSE && listen_attempts) {
				KLOG(KL_ERR, "csock_listen err=%d", err);
				msleep_interruptible(LISTEN_RESTART_TIMEOUT_MS);
				if (listen_attempts > 0)
					listen_attempts--;
				continue;
			} else if (!err) {
				KLOG(KL_INF, "listen done at port=%d",
						server->port);
				mutex_lock(&server->lock);
				server->sock = lsock;
				mutex_unlock(&server->lock);
			}
			server->err = err;
			complete(&server->comp);
			if (err)
				break;
		}

		if (server->sock && !server->stopping) {
			KLOG(KL_DBG, "accepting");
			err = ksock_accept(&con_sock, server->sock);
			if (err) {
				if (err == -EAGAIN) {
					KLOG(KL_WRN, "accept err=%d", err);
				} else {
					KLOG(KL_ERR, "accept err=%d", err);
				}
				continue;
			}
			KLOG(KL_DBG, "accepted con_sock=%p", con_sock);

			if (!ds_con_start(server, con_sock)) {
				KLOG(KL_ERR, "ds_con_start failed");
				ksock_release(con_sock);
				continue;
			}
		}
	}

	err = 0;
	mutex_lock(&server->lock);
	lsock = server->sock;
	KLOG(KL_DBG, "releasing listen socket=%p", lsock);
	server->sock = NULL;
	mutex_unlock(&server->lock);

	if (lsock)
		ksock_release(lsock);
	
	KLOG(KL_DBG, "releasing cons");

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

	KLOG(KL_DBG, "released cons");
	return 0;
}

static void ds_server_do_stop(struct ds_server *server)
{
	if (server->stopping) {
		KLOG(KL_ERR, "server %p %u-%d already stopping",
			server, server->ip, server->port);
		return;
	}

	server->stopping = 1;
	if (server->sock) {
		ksock_abort_accept(server->sock);
	}

	kthread_stop(server->thread);
	put_task_struct(server->thread);
	KLOG(KL_INF, "stopped server on ip %u port %d",
		server->ip, server->port);
}

struct ds_server *ds_server_create_start(u32 ip, int port)
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
	server->ip = ip;
	init_completion(&server->comp);

	snprintf(thread_name, sizeof(thread_name), "%s-%d", "ds_srv", port);
	server->thread = kthread_create(ds_server_thread_routine, server, thread_name);
	if (IS_ERR(server->thread)) {
		err = PTR_ERR(server->thread);
		server->thread = NULL;
		KLOG(KL_ERR, "kthread_create err=%d", err);
		kfree(server);
		return NULL;
	}
	get_task_struct(server->thread);
	wake_up_process(server->thread);
	wait_for_completion(&server->comp);
	
	return server;
}

int ds_server_start(u32 ip, int port)
{
	int err;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->ip == ip) {
			KLOG(KL_INF, "server for ip %u port %d already exists",
				ip, port);
			err = -EEXIST;
			mutex_unlock(&srv_list_lock);
			return err;
		}
	}
	server = ds_server_create_start(ip, port);
	if (server && !server->err) {
		KLOG(KL_INF, "started server on ip %u port %d",
			ip, port);
		list_add_tail(&server->srv_list, &srv_list);
		err = 0;
	} else {
		err = (!server) ? -ENOMEM : server->err;
		if (server) {
			ds_server_do_stop(server);
			kfree(server);
		}
	}
	mutex_unlock(&srv_list_lock);

	return err;
}

int ds_server_stop(u32 ip, int port)
{
	int err = -EINVAL;
	struct ds_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->ip == ip) {
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

void ds_server_stop_all(void)
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

