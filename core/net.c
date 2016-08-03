#include "net.h"
#include "super.h"
#include "upages.h"
#include "ksocket.h"
#include "helpers.h"
#include "route.h"

#include <crt/include/crt.h>

#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/in.h>
#include <linux/delay.h>

#define LISTEN_RESTART_TIMEOUT_MS 2000

static DEFINE_MUTEX(srv_list_lock);
static LIST_HEAD(srv_list);

static void nkfs_con_wait(struct nkfs_con *con)
{
	kthread_stop(con->thread);
}

static void nkfs_con_free(struct nkfs_con *con)
{
	crt_kfree(con);
}

u16 nkfs_con_peer_port(struct nkfs_con *con)
{
	return ksock_peer_port(con->sock);
}

u16 nkfs_con_self_port(struct nkfs_con *con)
{
	return ksock_self_port(con->sock);
}

u32 nkfs_con_peer_addr(struct nkfs_con *con)
{
	return ksock_peer_addr(con->sock);
}

u32 nkfs_con_self_addr(struct nkfs_con *con)
{
	return ksock_self_addr(con->sock);
}

void nkfs_con_close(struct nkfs_con *con)
{
	ksock_release(con->sock);
	if (con->thread)
		put_task_struct(con->thread);
	nkfs_con_free(con);
}

void nkfs_con_fail(struct nkfs_con *con, int err)
{
	if (!con->err) {
		con->err = err;
	}
}

int nkfs_con_recv(struct nkfs_con *con, void *buffer, u32 nob)
{
	u32 read;
	int err;

	if (con->err)
		return con->err;

	err = ksock_read(con->sock, buffer, nob, &read);
	if (err) {
		nkfs_con_fail(con, err);
		return err;
	}

	if (nob != read) {
		err = -EIO;
		nkfs_con_fail(con, err);
		return err;
	}

	return 0;
}

int nkfs_con_send(struct nkfs_con *con, void *buffer, u32 nob)
{
	u32 wrote;
	int err;

	if (con->err)
		return con->err;

	err = ksock_write(con->sock, buffer, nob, &wrote);
	if (err) {
		nkfs_con_fail(con, err);
		return err;
	}

	if (nob != wrote) {
		err = -EIO;
		nkfs_con_fail(con, err);
		return err;
	}

	return 0;
}

int nkfs_con_send_pkt(struct nkfs_con *con, struct nkfs_net_pkt *pkt)
{
	int err;

	net_pkt_sign(pkt);
	err = nkfs_con_send(con, pkt, sizeof(*pkt));
	return err;
}

int nkfs_con_send_reply(struct nkfs_con *con,
		struct nkfs_net_pkt *reply, int err)
{
	reply->err = err;
	return nkfs_con_send_pkt(con, reply);
}

int nkfs_con_recv_pkt(struct nkfs_con *con,
		struct nkfs_net_pkt *pkt)
{
	int err;

	err = nkfs_con_recv(con, pkt, sizeof(*pkt));
	if (err) {
		return err;
	}

	err = net_pkt_check(pkt);
	if (err) {
		nkfs_con_fail(con, -EINVAL);
	}
	return err;
}

static int nkfs_con_recv_pages(struct nkfs_con *con,
	struct nkfs_net_pkt *pkt, struct nkfs_pages *ppages)
{
	struct csum dsum;
	struct nkfs_pages pages;
	int err;
	u32 read, llen;
	void *buf;
	int i;

	if (pkt->dsize == 0 || pkt->dsize > NKFS_NET_PKT_MAX_DSIZE) {
		return -EINVAL;
	}

	err = nkfs_pages_create(pkt->dsize, &pages);
	if (err) {
		return err;
	}

	read = pkt->dsize;
	i = 0;
	llen = 0;
	while (read > 0) {
		NKFS_BUG_ON(i >= pages.nr_pages);
		buf = kmap(pages.pages[i]);
		llen = (read > PAGE_SIZE) ? PAGE_SIZE : read;
		err = nkfs_con_recv(con, buf, llen);
		kunmap(pages.pages[i]);
		if (err) {
			nkfs_con_fail(con, err);
			goto free_pages;
		}
		i++;
		read -= llen;
	}

	err = nkfs_pages_dsum(&pages, &dsum, pkt->dsize);
	if (err) {
		goto free_pages;
	}

	err = net_pkt_check_dsum(pkt, &dsum);
	if (err) {
		goto free_pages;
	}

	memcpy(ppages, &pages, sizeof(pages));
	return 0;

free_pages:
	nkfs_pages_release(&pages);
	return err;
}

static int nkfs_con_send_pages(struct nkfs_con *con,
	struct nkfs_pages *pages, u32 len)
{
	u32 i, ilen;
	void *ibuf;
	int err;

	if (len > pages->len)
		return -EINVAL;

	i = 0;
	while (len > 0) {
		NKFS_BUG_ON(i >= pages->nr_pages);
		ilen = (len > PAGE_SIZE) ? PAGE_SIZE : len;
		ibuf = kmap(pages->pages[i]);
		err = nkfs_con_send(con, ibuf, ilen);
		kunmap(pages->pages[i]);
		if (err) {
			nkfs_con_fail(con, err);
			goto out;
		}
		len -= ilen;
		i++;
	}
	err = 0;
out:
	return err;
}

static int nkfs_con_get_obj(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;
	struct nkfs_pages pages;
	struct csum dsum;
	u32 read;

	if (pkt->dsize == 0 || pkt->dsize > NKFS_NET_PKT_MAX_DSIZE) {
		return -EINVAL;
	}

	err = nkfs_pages_create(pkt->dsize, &pages);
	if (err) {
		nkfs_con_send_reply(con, reply, err);
		goto out;
	}

	err = nkfs_sb_list_get_obj(&pkt->u.get_obj.obj_id,
		pkt->u.get_obj.off,
		0,
		pkt->dsize,
		pages.pages,
		pages.nr_pages,
		&read);
	if (err) {
		nkfs_con_send_reply(con, reply, err);
		goto free_pages;
	}

	if (read) {
		err = nkfs_pages_dsum(&pages, &dsum, read);
		if (err) {
			nkfs_con_send_reply(con, reply, err);
			goto free_pages;
		}
		memcpy(&reply->dsum, &dsum, sizeof(dsum));
	}

	reply->dsize = read;

	err = nkfs_con_send_reply(con, reply, 0);
	if (err)
		goto free_pages;

	if (read)
		err = nkfs_con_send_pages(con, &pages, read);

free_pages:
	nkfs_pages_release(&pages);
out:
	return err;
}

static int nkfs_con_put_obj(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;
	struct nkfs_pages pages;

	err = nkfs_con_recv_pages(con, pkt, &pages);
	if (err) {
		if (!con->err)
			nkfs_con_send_reply(con, reply, err);
		goto out;
	}

	err = nkfs_sb_list_put_obj(&pkt->u.put_obj.obj_id,
		pkt->u.put_obj.off,
		0,
		pkt->dsize,
		pages.pages,
		pages.nr_pages);

	err = nkfs_con_send_reply(con, reply, err);

	nkfs_pages_release(&pages);
out:
	return err;
}

static int nkfs_con_create_obj(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;

	err = nkfs_sb_list_create_obj(&reply->u.create_obj.obj_id);
	return nkfs_con_send_reply(con, reply, err);
}

static int nkfs_con_delete_obj(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;

	err = nkfs_sb_list_delete_obj(&pkt->u.delete_obj.obj_id);
	return nkfs_con_send_reply(con, reply, err);
}

static int nkfs_con_echo(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	return nkfs_con_send_reply(con, reply, 0);
}

static int nkfs_con_query_obj(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;

	err = nkfs_sb_list_query_obj(&pkt->u.query_obj.obj_id,
		&reply->u.query_obj.obj_info);
	return nkfs_con_send_reply(con, reply, err);
}

static int nkfs_con_process_pkt(struct nkfs_con *con, struct nkfs_net_pkt *pkt)
{
	struct nkfs_net_pkt *reply;
	int err;

	reply = net_pkt_alloc();
	if (!reply) {
		err = -ENOMEM;
		nkfs_con_fail(con, err);
		return err;
	}

	switch (pkt->type) {
	case NKFS_NET_PKT_ECHO:
		err = nkfs_con_echo(con, pkt, reply);
		break;
	case NKFS_NET_PKT_PUT_OBJ:
		err = nkfs_con_put_obj(con, pkt, reply);
		break;
	case NKFS_NET_PKT_GET_OBJ:
		err = nkfs_con_get_obj(con, pkt, reply);
		break;
	case NKFS_NET_PKT_DELETE_OBJ:
		err = nkfs_con_delete_obj(con, pkt, reply);
		break;
	case NKFS_NET_PKT_CREATE_OBJ:
		err = nkfs_con_create_obj(con, pkt, reply);
		break;
	case NKFS_NET_PKT_QUERY_OBJ:
		err = nkfs_con_query_obj(con, pkt, reply);
		break;
	case NKFS_NET_PKT_NEIGH_HANDSHAKE:
		err = nkfs_route_neigh_handshake(con, pkt, reply);
		break;
	case NKFS_NET_PKT_NEIGH_HEARTBEAT:
		err = nkfs_route_neigh_heartbeat(con, pkt, reply);
		break;
	default:
		err = nkfs_con_send_reply(con, reply, -EINVAL);
		break;
	}
	crt_free(reply);
	return err;
}

static int nkfs_con_thread_routine(void *data)
{
	struct nkfs_con *con = (struct nkfs_con *)data;
	struct nkfs_server *server = con->server;
	int err;

	NKFS_BUG_ON(con->thread != current);

	while (!kthread_should_stop() && !con->err) {
		struct nkfs_net_pkt *pkt = net_pkt_alloc();

		if (!pkt) {
			nkfs_con_fail(con, -ENOMEM);
			break;
		}
		err = nkfs_con_recv_pkt(con, pkt);
		if (err) {
			crt_free(pkt);
			break;
		}
		nkfs_con_process_pkt(con, pkt);
		crt_free(pkt);
	}

	if (!server->stopping) {
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&con->list))
			list_del_init(&con->list);
		else
			con = NULL;
		mutex_unlock(&server->con_list_lock);

		if (con)
			nkfs_con_close(con);
	}

	return 0;
}

static struct nkfs_con *nkfs_con_alloc(void)
{
	struct nkfs_con *con;

	con = crt_kmalloc(sizeof(*con), GFP_NOIO);
	if (!con) {
		return NULL;
	}
	memset(con, 0, sizeof(*con));
	return con;
}

int nkfs_con_connect(u32 ip, int port, struct nkfs_con **pcon)
{
	struct nkfs_con *con;
	int err;

	con = nkfs_con_alloc();
	if (!con)
		return -ENOMEM;

	err = ksock_connect(&con->sock, 0, 0, ip, port);
	if (err) {
		goto free_con;
	}

	*pcon = con;
	return 0;
free_con:
	nkfs_con_free(con);
	return err;
}

static struct nkfs_con *nkfs_con_start(struct nkfs_server *server,
	struct socket *sock)
{
	struct nkfs_con *con;
	int err = -EINVAL;

	con = nkfs_con_alloc();
	if (!con)
		return NULL;

	con->server = server;
	con->thread = NULL;
	con->sock = sock;
	con->thread = kthread_create(nkfs_con_thread_routine, con, "nkfs_con");
	if (IS_ERR(con->thread)) {
		err = PTR_ERR(con->thread);
		con->thread = NULL;
		goto out;
	}

	get_task_struct(con->thread);
	mutex_lock(&server->con_list_lock);
	list_add_tail(&con->list, &server->con_list);
	mutex_unlock(&server->con_list_lock);

	wake_up_process(con->thread);

	return con;
out:
	nkfs_con_free(con);
	return NULL;
}

static void nkfs_server_free(struct nkfs_server *server)
{
	crt_kfree(server);
}

static int nkfs_server_thread_routine(void *data)
{
	struct nkfs_server *server = (struct nkfs_server *)data;
	struct socket *lsock = NULL;
	struct socket *con_sock = NULL;
	struct nkfs_con *con = NULL;
	int err = 0;
	u32 listen_attempts = 3;

	if (server->thread != current)
		NKFS_BUG_ON(1);

	while (!kthread_should_stop()) {
		if (!server->sock) {
			err = ksock_listen(&lsock, (server->bind_ip) ?
					   server->bind_ip : INADDR_ANY,
					   server->port, 5);
			if (err == -EADDRINUSE && listen_attempts) {
				msleep_interruptible(LISTEN_RESTART_TIMEOUT_MS);
				if (listen_attempts > 0)
					listen_attempts--;
				continue;
			} else if (!err) {
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
			err = ksock_accept(&con_sock, server->sock);
			if (err) {
				continue;
			}
			if (!nkfs_con_start(server, con_sock)) {
				ksock_release(con_sock);
				continue;
			}
		}
	}

	err = 0;
	mutex_lock(&server->lock);
	lsock = server->sock;
	server->sock = NULL;
	mutex_unlock(&server->lock);

	if (lsock)
		ksock_release(lsock);

	for (;;) {
		con = NULL;
		mutex_lock(&server->con_list_lock);
		if (!list_empty(&server->con_list)) {
			con = list_first_entry(&server->con_list,
					       struct nkfs_con, list);
			list_del_init(&con->list);
		}
		mutex_unlock(&server->con_list_lock);
		if (!con)
			break;

		nkfs_con_wait(con);
		nkfs_con_free(con);
	}

	return 0;
}

static void nkfs_server_do_stop(struct nkfs_server *server)
{
	if (server->stopping) {
		return;
	}

	server->stopping = 1;
	if (server->sock)
		ksock_abort_accept(server->sock);

	kthread_stop(server->thread);
	put_task_struct(server->thread);
}

static struct nkfs_server *nkfs_server_create_start(u32 bind_ip, u32 ext_ip,
						    int port)
{
	char thread_name[10];
	int err;
	struct nkfs_server *server;

	server = crt_kmalloc(sizeof(*server), GFP_NOIO);
	if (!server) {
		return NULL;
	}

	memset(server, 0, sizeof(*server));
	INIT_LIST_HEAD(&server->con_list);
	INIT_LIST_HEAD(&server->srv_list);
	mutex_init(&server->lock);
	mutex_init(&server->con_list_lock);
	server->port = port;
	server->bind_ip = bind_ip;
	server->ext_ip = ext_ip;
	init_completion(&server->comp);

	snprintf(thread_name, sizeof(thread_name), "%s-%d", "nkfs_srv", port);
	server->thread = kthread_create(nkfs_server_thread_routine, server,
					thread_name);
	if (IS_ERR(server->thread)) {
		err = PTR_ERR(server->thread);
		server->thread = NULL;
		nkfs_server_free(server);
		return NULL;
	}
	get_task_struct(server->thread);
	wake_up_process(server->thread);
	wait_for_completion(&server->comp);

	return server;
}

int nkfs_server_start(u32 bind_ip, u32 ext_ip, int port)
{
	int err;
	struct nkfs_server *server;

	if (ext_ip == 0 || port == 0) {
		return -EINVAL;
	}

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->bind_ip == bind_ip) {
			err = -EEXIST;
			mutex_unlock(&srv_list_lock);
			return err;
		}
	}

	server = nkfs_server_create_start(bind_ip, ext_ip, port);
	if (server && !server->err) {
		list_add_tail(&server->srv_list, &srv_list);
		err = 0;
	} else {
		err = (!server) ? -ENOMEM : server->err;
		if (server) {
			nkfs_server_do_stop(server);
			nkfs_server_free(server);
		}
	}
	mutex_unlock(&srv_list_lock);

	return err;
}

int nkfs_server_select_one(u32 *pip, int *pport)
{
	int err = -ENOENT;
	struct nkfs_server *server;

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (!server->err) {
			*pip = server->ext_ip;
			*pport = server->port;
			err = 0;
			break;
		}
	}
	mutex_unlock(&srv_list_lock);
	return err;
}

int nkfs_server_stop(u32 bind_ip, int port)
{
	int err = -EINVAL;
	struct nkfs_server *server;

	if (port <= 0 || port > 64000) {
		return -EINVAL;
	}

	mutex_lock(&srv_list_lock);
	list_for_each_entry(server, &srv_list, srv_list) {
		if (server->port == port && server->bind_ip == bind_ip) {
			nkfs_server_do_stop(server);
			list_del(&server->srv_list);
			nkfs_server_free(server);
			err = 0;
			break;
		}
	}
	mutex_unlock(&srv_list_lock);
	return err;
}

static void nkfs_server_stop_all(void)
{
	struct nkfs_server *server;
	struct nkfs_server *tmp;

	mutex_lock(&srv_list_lock);
	list_for_each_entry_safe(server, tmp, &srv_list, srv_list) {
		nkfs_server_do_stop(server);
		list_del(&server->srv_list);
		nkfs_server_free(server);
	}
	mutex_unlock(&srv_list_lock);
}

int nkfs_server_init(void)
{
	return 0;
}

void nkfs_server_finit(void)
{
	nkfs_server_stop_all();
}

int nkfs_ip_port_cmp(u32 ip1, int port1, u32 ip2, int port2)
{
	if (ip1 == ip2) {
		if (port1 < port2)
			return -1;
		else if (port1 > port2)
			return 1;
		else
			return 0;
	} else if (ip1 > ip2) {
		return 1;
	} else {
		return -1;
	}
}
