#include <inc/nkfs_priv.h>

#define __SUBCOMPONENT__ "route"
#define HOST_TIMER_TIMEOUT_MS 5000

#define KLOG_HOST(lvl, h)					\
	do {							\
		char *host_id, *net_id;				\
		host_id = nkfs_obj_id_str(&((h)->host_id));	\
		net_id = nkfs_obj_id_str(&((h)->net_id));	\
		KLOG((lvl), "host %p hid %s nid %s",		\
			(h), host_id, net_id);			\
		if (host_id)					\
			crt_free(host_id);			\
		if (net_id)					\
			crt_free(net_id);			\
	} while (0);						\

#define KLOG_NEIGH(lvl, n)							\
	do {									\
		char *host_id = NULL;						\
		if ((n)->hid)							\
			host_id = nkfs_obj_id_str(&((n)->hid)->host_id);	\
		KLOG((lvl), "n %p hid %s s%d %x:%d -> %x:%d hbt %llu",		\
			(n), host_id, (n)->state, (n)->s_ip,			\
			(n)->s_port, (n)->d_ip, (n)->d_port,			\
			(n)->hbt_delay);					\
		if (host_id)							\
			crt_free(host_id);					\
	} while (0);								\

struct nkfs_host *nkfs_host;
struct kmem_cache *nkfs_neigh_cachep;
struct kmem_cache *nkfs_host_id_cachep;
struct kmem_cache *nkfs_host_work_cachep;

static void __nkfs_neighs_remove(struct nkfs_host *host,
	struct nkfs_neigh *neigh);

void __nkfs_host_ids_remove(struct nkfs_host *host,
	struct nkfs_host_id *host_id);

static int nkfs_neigh_connect(struct nkfs_neigh *neigh)
{
	int err;

	NKFS_BUG_ON(neigh->con);
	err = nkfs_con_connect(neigh->d_ip, neigh->d_port, &neigh->con);
	return err;
}

static void nkfs_neigh_close(struct nkfs_neigh *neigh)
{
	if (neigh->con) {
		nkfs_con_close(neigh->con);
		neigh->con = NULL;
	}
}

static struct nkfs_neigh *nkfs_neigh_alloc(void)
{
	struct nkfs_neigh *neigh;

	neigh = kmem_cache_alloc(nkfs_neigh_cachep, GFP_NOIO);
	if (!neigh) {
		KLOG(KL_ERR, "cant alloc neigh");
		return NULL;
	}
	memset(neigh, 0, sizeof(*neigh));
	atomic_set(&neigh->ref, 1);
	INIT_LIST_HEAD(&neigh->neigh_list);
	INIT_LIST_HEAD(&neigh->hid_list);
	init_rwsem(&neigh->rw_sem);

	return neigh;
}

static void nkfs_host_work_free(struct nkfs_host_work *work)
{
	kmem_cache_free(nkfs_host_work_cachep, work);
}

static void nkfs_host_work_deref(struct nkfs_host_work *work)
{
	NKFS_BUG_ON(atomic_read(&work->ref) <= 0);
	if (atomic_dec_and_test(&work->ref))
		nkfs_host_work_free(work);
}

static void nkfs_neigh_free(struct nkfs_neigh *neigh)
{
	kmem_cache_free(nkfs_neigh_cachep, neigh);
}

static void nkfs_neigh_detach_hid(struct nkfs_neigh *neigh)
{
	struct nkfs_host_id *hid = neigh->hid;
	if (hid) {
		write_lock_irq(&hid->neigh_list_lock);
		list_del_init(&neigh->hid_list);
		write_unlock_irq(&hid->neigh_list_lock);
		HOST_ID_DEREF(hid);
	}
}

static void nkfs_neigh_release(struct nkfs_neigh *neigh)
{
	nkfs_neigh_detach_hid(neigh);
	if (neigh->host) {
		write_lock(&neigh->host->neighs_lock);
		__nkfs_neighs_remove(neigh->host, neigh);
		write_unlock(&neigh->host->neighs_lock);
	}
	nkfs_neigh_close(neigh);
	nkfs_neigh_free(neigh);
}

void nkfs_neigh_ref(struct nkfs_neigh *neigh)
{
	NKFS_BUG_ON(atomic_read(&neigh->ref) <= 0);
	atomic_inc(&neigh->ref);
}

void nkfs_neigh_deref(struct nkfs_neigh *neigh)
{
	NKFS_BUG_ON(atomic_read(&neigh->ref) <= 0);	
	if (atomic_dec_and_test(&neigh->ref))
		nkfs_neigh_release(neigh);	
}

struct nkfs_host_id *nkfs_hid_alloc(void)
{
	struct nkfs_host_id *hid;

	hid = kmem_cache_alloc(nkfs_host_id_cachep, GFP_NOIO);
	if (!hid) {
		KLOG(KL_ERR, "cant alloc host_id");
		return NULL;
	}
	memset(hid, 0, sizeof(*hid));
	INIT_LIST_HEAD(&hid->neigh_list);
	rwlock_init(&hid->neigh_list_lock);
	atomic_set(&hid->ref, 1);
	return hid;
}

void nkfs_hid_free(struct nkfs_host_id *hid)
{
	kmem_cache_free(nkfs_host_id_cachep, hid);
}

static void nkfs_hid_release(struct nkfs_host_id *hid)
{
	struct nkfs_host *host = hid->host;
	KLOG(KL_DBG, "hid %p, host %p", hid, host);
	if (host) {
		write_lock_irq(&host->host_ids_lock);
		__nkfs_host_ids_remove(host, hid);
		write_unlock_irq(&host->host_ids_lock);
	}
	nkfs_hid_free(hid);
}

void nkfs_hid_ref(struct nkfs_host_id *hid)
{
	NKFS_BUG_ON(atomic_read(&hid->ref) <= 0);
	atomic_inc(&hid->ref);
}

void nkfs_hid_deref(struct nkfs_host_id *hid)
{
	NKFS_BUG_ON(atomic_read(&hid->ref) <= 0);
	if (atomic_dec_and_test(&hid->ref))
		nkfs_hid_release(hid);
}

struct nkfs_host_id *__nkfs_host_ids_lookup(struct nkfs_host *host, struct nkfs_obj_id *host_id)
{
	struct rb_node *n = host->host_ids.rb_node;
	struct nkfs_host_id *found = NULL;

	while (n) {
		struct nkfs_host_id *hid;
		int cmp;

		hid = rb_entry(n, struct nkfs_host_id, host_ids_link);
		cmp = nkfs_obj_id_cmp(host_id, &hid->host_id);
		KLOG(KL_DBG, "host_id %p, hid %p, cmp=%d", host_id, hid, cmp);
		if (cmp < 0) {
			n = n->rb_left;
		} else if (cmp > 0) {
			n = n->rb_right;
		} else {
			found = hid;
			break;
		}
	}
	return found;
}

void __nkfs_host_ids_remove(struct nkfs_host *host,
	struct nkfs_host_id *host_id)
{
	struct nkfs_host_id *found;

	found = __nkfs_host_ids_lookup(host, &host_id->host_id);
	KLOG(KL_DBG, "found %p", found);
	if (found) {
		NKFS_BUG_ON(found != host_id);
		rb_erase(&found->host_ids_link, &host->host_ids);
		host->host_ids_active--;
	}
}

struct nkfs_host_id *__nkfs_host_id_insert(struct nkfs_host *host,
		struct nkfs_host_id *host_id)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct nkfs_host_id *inserted = NULL;

	p = &host->host_ids.rb_node;
	while (*p) {
		struct nkfs_host_id *found;
		int cmp;
		parent = *p;
		found = rb_entry(parent, struct nkfs_host_id, host_ids_link);
		cmp = nkfs_obj_id_cmp(&host_id->host_id, &found->host_id);
		if (cmp < 0) {
			p = &(*p)->rb_left;
		} else if (cmp > 0) {
			p = &(*p)->rb_right;
		} else {
			inserted = found;
			break;
		}
	}

	if (!inserted) {
		rb_link_node(&host_id->host_ids_link, parent, p);
		rb_insert_color(&host_id->host_ids_link, &host->host_ids);
		host_id->host = host;
		host->host_ids_active++;
		inserted = host_id;
	}
	HOST_ID_REF(inserted);
	return inserted;
}

struct nkfs_host_id *nkfs_host_id_lookup_or_create(struct nkfs_host *host,
	struct nkfs_obj_id *host_id)
{
	struct nkfs_host_id *hid, *inserted;
	hid = nkfs_hid_alloc();
	if (!hid)
		return NULL;

	nkfs_obj_id_copy(&hid->host_id, host_id);
	write_lock_irq(&host->host_ids_lock);
	inserted = __nkfs_host_id_insert(host, hid);
	write_unlock_irq(&host->host_ids_lock);
	if (inserted != hid) {
		HOST_ID_DEREF(hid);
		hid = inserted;
	} else {
		HOST_ID_DEREF(hid);
	}

	return hid;
}

struct nkfs_host_id *nkfs_host_id_lookup(struct nkfs_host *host,
	struct nkfs_obj_id *host_id)
{
	struct nkfs_host_id *hid;

	read_lock_irq(&host->host_ids_lock);
	hid = __nkfs_host_ids_lookup(host, host_id);
	if (hid)
		HOST_ID_REF(hid);
	read_unlock_irq(&host->host_ids_lock);
	return hid;
}

static struct nkfs_neigh *__nkfs_neighs_lookup(struct nkfs_host *host, u32 d_ip, int d_port)
{
	struct rb_node *n = host->neighs.rb_node;
	struct nkfs_neigh *found = NULL;

	while (n) {
		struct nkfs_neigh *neigh;
		int cmp;

		neigh = rb_entry(n, struct nkfs_neigh, neighs_link);
		cmp = nkfs_ip_port_cmp(d_ip, d_port, neigh->d_ip, neigh->d_port);
		if (cmp < 0) {
			n = n->rb_left;
		} else if (cmp > 0) {
			n = n->rb_right;
		} else {
			found = neigh;
			break;
		}
	}
	return found;
}

static void __nkfs_neighs_remove(struct nkfs_host *host,
	struct nkfs_neigh *neigh)
{
	struct nkfs_neigh *found;

	found = __nkfs_neighs_lookup(host, neigh->d_ip, neigh->d_port);
	if (found) {
		NKFS_BUG_ON(found != neigh);
		rb_erase(&found->neighs_link, &host->neighs);
		host->neighs_active--;
	}
}

static struct nkfs_neigh *__nkfs_neighs_insert(struct nkfs_host *host,
		struct nkfs_neigh *neigh)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct nkfs_neigh *inserted = NULL;

	p = &host->neighs.rb_node;
	while (*p) {
		struct nkfs_neigh *found;
		int cmp;
		parent = *p;
		found = rb_entry(parent, struct nkfs_neigh, neighs_link);
		cmp = nkfs_ip_port_cmp(neigh->d_ip, neigh->d_port,
			found->d_ip, found->d_port);
		if (cmp < 0) {
			p = &(*p)->rb_left;
		} else if (cmp > 0) {
			p = &(*p)->rb_right;
		} else {
			inserted = found;
			break;
		}
	}
	if (!inserted) {
		rb_link_node(&neigh->neighs_link, parent, p);
		rb_insert_color(&neigh->neighs_link, &host->neighs);
		neigh->host = host;
		host->neighs_active++;
		inserted = neigh;
	}
	NEIGH_REF(inserted);
	return inserted;
}


static void nkfs_host_work_func(struct work_struct *wrk)
{
	struct nkfs_host_work *work = container_of(wrk,
			struct nkfs_host_work, work);

	work->func(work);
	nkfs_host_work_deref(work);
}

static int nkfs_host_queue_work(struct nkfs_host *host,
	nkfs_host_work_func_t func, void *data)
{
	struct nkfs_host_work *work = NULL;

	work = kmem_cache_alloc(nkfs_host_work_cachep, GFP_ATOMIC);
	if (!work) {
		KLOG(KL_ERR, "cant alloc work");
		return -ENOMEM;
	}

	memset(work, 0, sizeof(*work));
	INIT_WORK(&work->work, nkfs_host_work_func);
	work->host = host;
	work->data = data;
	work->func = func;

	atomic_set(&work->ref, 1);

	if (!queue_work(host->wq, &work->work)) {
		nkfs_host_work_deref(work);
		KLOG(KL_ERR, "cant queue work");
		return -ENOMEM;
	}

	return 0;
}

static void nkfs_neigh_attach_host_id(struct nkfs_neigh *neigh,
	struct nkfs_host_id *hid)
{
	NKFS_BUG_ON(neigh->hid);
	NKFS_BUG_ON(!list_empty(&neigh->hid_list));

	write_lock_irq(&hid->neigh_list_lock);
	neigh->hid = hid;	
	list_add_tail(&neigh->hid_list, &hid->neigh_list); 
	write_unlock_irq(&hid->neigh_list_lock);
}

static int nkfs_neigh_do_handshake(struct nkfs_neigh *neigh)
{
	int err;
	struct nkfs_net_pkt *req, *reply;
	struct nkfs_host *host = neigh->host;
	struct nkfs_host_id *hid;

	down_write(&neigh->rw_sem);

	if (!test_bit(NKFS_NEIGH_S_INITED, &neigh->state)) {
		err = -EINVAL;
		KLOG(KL_ERR, "invalid neigh");
		goto unlock;
	}

	if (test_bit(NKFS_NEIGH_S_SHAKED, &neigh->state)) {
		err = 0;
		KLOG(KL_INF, "already shaked");
		goto unlock;
	}

	err = nkfs_neigh_connect(neigh);
	if (err) {
		KLOG(KL_ERR, "cant connect err %d", err);
		goto unlock;
	}

	req = net_pkt_alloc();
	if (!req) {
		KLOG(KL_ERR, "no memory");
		goto close_con;
	}

	reply = net_pkt_alloc();
	if (!reply) {
		KLOG(KL_ERR, "no memory");
		goto free_req;
	}

	req->type = NKFS_NET_PKT_NEIGH_HANDSHAKE;
	nkfs_obj_id_copy(&req->u.neigh_handshake.src_net_id, &host->net_id);
	nkfs_obj_id_copy(&req->u.neigh_handshake.src_host_id, &host->host_id);

	req->u.neigh_handshake.s_ip = neigh->d_ip;
	req->u.neigh_handshake.s_port = neigh->d_port;
	req->u.neigh_handshake.d_ip = neigh->s_ip;
	req->u.neigh_handshake.d_port = neigh->s_port;

	KLOG(KL_DBG, "send neigh %x:%d -> %x:%d", req->u.neigh_handshake.s_ip,
		req->u.neigh_handshake.s_port, req->u.neigh_handshake.d_ip,
		req->u.neigh_handshake.d_port);

	err = nkfs_con_send_pkt(neigh->con, req);
	if (err) {
		KLOG(KL_ERR, "send err %d", err);
		goto free_reply;
	}

	err = nkfs_con_recv_pkt(neigh->con, reply);
	if (err) {
		KLOG(KL_ERR, "recv err %d", err);
		goto free_reply;
	}

	if (reply->err) {
		KLOG(KL_ERR, "reply err %d", reply->err);
		err = reply->err;
		goto free_reply;
	}

	hid = nkfs_host_id_lookup_or_create(neigh->host,
		&reply->u.neigh_handshake.reply_host_id);
	if (!hid) {
		err = -ENOMEM;
		KLOG(KL_ERR, "cant get host_id %d", err);
		goto free_reply;
	}
	nkfs_neigh_attach_host_id(neigh, hid);
	set_bit(NKFS_NEIGH_S_SHAKED, &neigh->state);
	KLOG_NEIGH(KL_INF, neigh);

free_reply:
	crt_free(reply);
free_req:
	crt_free(req);
close_con:
	nkfs_neigh_close(neigh);
unlock:
	up_write(&neigh->rw_sem);

	return err;
}

static void nkfs_neigh_handshake_work(struct nkfs_host_work *work)
{
	struct nkfs_neigh *neigh = work->data;

	nkfs_neigh_do_handshake(neigh);
}

static void nkfs_host_handshake_work(struct nkfs_host_work *work)
{
	struct nkfs_host *host = work->host;
	struct nkfs_neigh *neigh, *tmp;

	read_lock(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, neigh_list) {
		if (test_bit(NKFS_NEIGH_S_INITED, &neigh->state) &&
			!test_bit(NKFS_NEIGH_S_SHAKED, &neigh->state))
			nkfs_host_queue_work(host, nkfs_neigh_handshake_work, neigh);
	}
	read_unlock(&host->neighs_lock);
}

static int nkfs_neigh_do_heartbeat(struct nkfs_neigh *neigh)
{
	int err;
	struct nkfs_net_pkt *req, *reply;
	struct nkfs_host *host = neigh->host;

	down_write(&neigh->rw_sem);
	if (!test_bit(NKFS_NEIGH_S_INITED, &neigh->state) ||
		!test_bit(NKFS_NEIGH_S_SHAKED, &neigh->state)) {
		err = 0;
		KLOG(KL_INF, "not ready");
		goto unlock;
	}

	neigh->hbt_time = get_jiffies_64();

	err = nkfs_neigh_connect(neigh);
	if (err) {
		KLOG(KL_ERR, "cant connect err %d", err);
		goto unlock;
	}

	req = net_pkt_alloc();
	if (!req) {
		KLOG(KL_ERR, "no memory");
		goto close_con;
	}

	reply = net_pkt_alloc();
	if (!reply) {
		KLOG(KL_ERR, "no memory");
		goto free_req;
	}

	req->type = NKFS_NET_PKT_NEIGH_HEARTBEAT;
	nkfs_obj_id_copy(&req->u.neigh_heartbeat.src_net_id, &host->net_id);
	nkfs_obj_id_copy(&req->u.neigh_heartbeat.src_host_id, &host->host_id);
	nkfs_obj_id_copy(&req->u.neigh_heartbeat.dst_host_id, &neigh->hid->host_id);

	err = nkfs_con_send_pkt(neigh->con, req);
	if (err) {
		KLOG(KL_ERR, "send err %d", err);
		goto free_reply;
	}

	err = nkfs_con_recv_pkt(neigh->con, reply);
	if (err) {
		KLOG(KL_ERR, "recv err %d", err);
		goto free_reply;
	}

	if (reply->err) {
		KLOG(KL_ERR, "reply err %d", reply->err);
		err = reply->err;
		goto free_reply;
	}

	neigh->hbt_delay = get_jiffies_64() - neigh->hbt_time;
	KLOG_NEIGH(KL_INF, neigh);

free_reply:
	crt_free(reply);
free_req:
	crt_free(req);
close_con:
	nkfs_neigh_close(neigh);
unlock:
	if (err)
		neigh->hbt_err = err;

	up_write(&neigh->rw_sem);

	return err;
}


static void nkfs_neigh_heartbeat_work(struct nkfs_host_work *work)
{
	struct nkfs_neigh *neigh = work->data;

	nkfs_neigh_do_heartbeat(neigh);
}

static void nkfs_host_heartbeat_work(struct nkfs_host_work *work)
{
	struct nkfs_host *host = work->host;
	struct nkfs_neigh *neigh, *tmp;

	read_lock(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, neigh_list) {
		if (test_bit(NKFS_NEIGH_S_INITED, &neigh->state) &&
			test_bit(NKFS_NEIGH_S_SHAKED, &neigh->state))
			nkfs_host_queue_work(host, nkfs_neigh_heartbeat_work, neigh);
	}
	read_unlock(&host->neighs_lock);
}

static void nkfs_host_timer_callback(unsigned long data)
{
	struct nkfs_host *host = (struct nkfs_host *)data;

	nkfs_host_queue_work(host, nkfs_host_handshake_work, NULL);
	nkfs_host_queue_work(host, nkfs_host_heartbeat_work, NULL);

	if (!host->stopping) {
		mod_timer(&host->timer, jiffies +
			msecs_to_jiffies(HOST_TIMER_TIMEOUT_MS));
	}
}

static struct nkfs_host *nkfs_host_alloc(void)
{
	struct nkfs_host *host;

	host = kmalloc(sizeof(*host), GFP_NOIO);
	if (!host)
		return NULL;
	memset(host, 0, sizeof(*host));
	return host;
}	

static void nkfs_host_free(struct nkfs_host *host)
{
	kfree(host);
}

static struct nkfs_host *nkfs_host_create(void)
{
	struct nkfs_host *host;
	int err;

	host = nkfs_host_alloc();
	if (!host)
		return NULL;
	
	nkfs_obj_id_gen(&host->host_id);
	host->neighs = RB_ROOT;
	rwlock_init(&host->neighs_lock);

	host->host_ids = RB_ROOT;
	rwlock_init(&host->host_ids_lock);

	INIT_LIST_HEAD(&host->neigh_list);

	host->wq = alloc_workqueue("nkfs_route_wq", WQ_UNBOUND, 1);
	if (!host->wq) {
		KLOG(KL_ERR, "cant create wq");
		goto free_host;
	}

	setup_timer(&host->timer, nkfs_host_timer_callback, (unsigned long)host);
	err = mod_timer(&host->timer,
			jiffies +
			msecs_to_jiffies(HOST_TIMER_TIMEOUT_MS));
	if (err) {
		KLOG(KL_ERR, "mod_timer failed with err=%d", err);
		goto del_wq;
	}

	KLOG_HOST(KL_INF, host);
	return host;

del_wq:
	destroy_workqueue(host->wq);
free_host:
	nkfs_host_free(host);	
	return NULL;
}

static void nkfs_host_release(struct nkfs_host *host)
{
	struct nkfs_neigh *neigh, *tmp;
	struct list_head neigh_list;

	host->stopping = 1;
	del_timer_sync(&host->timer);
	destroy_workqueue(host->wq);
	
	INIT_LIST_HEAD(&neigh_list);
	
	write_lock_irq(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, neigh_list) {
		list_del_init(&neigh->neigh_list);
		list_add_tail(&neigh->neigh_list, &neigh_list);
	}
	write_unlock_irq(&host->neighs_lock);	

	list_for_each_entry_safe(neigh, tmp, &neigh_list, neigh_list) {
		list_del_init(&neigh->neigh_list);
		NEIGH_DEREF(neigh);
	}

	KLOG(KL_DBG, "neighs %d host_ids %d",
		host->neighs_active, host->host_ids_active);

	NKFS_BUG_ON(host->neighs_active);
	NKFS_BUG_ON(host->host_ids_active);
	nkfs_host_free(host);
}

int nkfs_route_init(void)
{
	int err;

	nkfs_host = nkfs_host_create();
	if (!nkfs_host) {
		KLOG(KL_ERR, "cant create host");
		return -ENOMEM;
	}

	nkfs_neigh_cachep = kmem_cache_create("nkfs_neigh_cache",
		sizeof(struct nkfs_neigh), 0, SLAB_MEM_SPREAD, NULL);
	if (!nkfs_neigh_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto rel_host;
	}

	nkfs_host_id_cachep = kmem_cache_create("nkfs_host_id_cache",
		sizeof(struct nkfs_host_id), 0, SLAB_MEM_SPREAD, NULL);
	if (!nkfs_host_id_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_neigh_cache;
	}	

	nkfs_host_work_cachep = kmem_cache_create("nkfs_host_work_cache",
		sizeof(struct nkfs_host_work), 0, SLAB_MEM_SPREAD, NULL);
	if (!nkfs_host_work_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_host_cache;
	}	


	return 0;

del_host_cache:
	kmem_cache_destroy(nkfs_host_work_cachep);
del_neigh_cache:
	kmem_cache_destroy(nkfs_neigh_cachep);
rel_host:
	nkfs_host_release(nkfs_host);
	return err;
}

void nkfs_route_finit(void)
{
	nkfs_host_release(nkfs_host);
	kmem_cache_destroy(nkfs_neigh_cachep);
	kmem_cache_destroy(nkfs_host_id_cachep);
	kmem_cache_destroy(nkfs_host_work_cachep);
}

int nkfs_host_add_neigh(struct nkfs_host *host, struct nkfs_neigh *neigh)
{
	struct nkfs_neigh *inserted;
	int err = -EEXIST;

	KLOG(KL_DBG, "adding neigh %x:%d -> %x:%d", neigh->s_ip, neigh->s_port,
		neigh->d_ip, neigh->d_port);

	write_lock_irq(&host->neighs_lock);
	inserted = __nkfs_neighs_insert(host, neigh);
	NKFS_BUG_ON(!inserted);
	if (inserted == neigh) {
		list_add_tail(&neigh->neigh_list, &host->neigh_list);
		err = 0;
	}
	NEIGH_DEREF(inserted);
	write_unlock_irq(&host->neighs_lock);
	if (!err)
		KLOG_NEIGH(KL_INF, neigh);

	return err;
}

int nkfs_host_remove_neigh(struct nkfs_host *host, u32 d_ip, int d_port)
{
	struct nkfs_neigh *neigh, *tmp;
	struct list_head neigh_list;	
	int found = 0;

	INIT_LIST_HEAD(&neigh_list);

	write_lock_irq(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, neigh_list) {
		if (neigh->d_ip == d_ip && neigh->d_port == d_port) {
			list_del_init(&neigh->neigh_list);
			list_add_tail(&neigh->neigh_list, &neigh_list);
		}
	}
	write_unlock_irq(&host->neighs_lock);

	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, neigh_list) {
		found++;
		list_del_init(&neigh->neigh_list);
		NEIGH_DEREF(neigh);
	}

	return (found) ? 0 : -ENOENT;
}

int nkfs_route_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port)
{
	struct nkfs_neigh *neigh;
	int err;

	neigh = nkfs_neigh_alloc();
	if (!neigh) {
		KLOG(KL_ERR, "no mem");
		return -ENOMEM;
	}
	neigh->d_ip = d_ip;
	neigh->d_port = d_port;
	neigh->s_ip = s_ip;
	neigh->s_port = s_port;
	set_bit(NKFS_NEIGH_S_INITED, &neigh->state);
	err = nkfs_host_add_neigh(nkfs_host, neigh); 
	if (err) {
		KLOG(KL_ERR, "cant add neigh err %d", err);
		NEIGH_DEREF(neigh);
	}

	return err;
}

int nkfs_route_neigh_remove(u32 d_ip, int d_port)
{
	return nkfs_host_remove_neigh(nkfs_host, d_ip, d_port);
}

static int nkfs_neigh_handshake(struct nkfs_obj_id *src_net_id,
	struct nkfs_obj_id *src_host_id,
	u32 d_ip, int d_port,
	u32 s_ip, int s_port,
	struct nkfs_obj_id *reply_host_id)
{
	struct nkfs_neigh *neigh;
	struct nkfs_host_id *hid;
	int err;

	if (0 != nkfs_obj_id_cmp(src_net_id, &nkfs_host->net_id)) {
		KLOG(KL_ERR, "diff net id");
		return -EINVAL;
	}

	neigh = nkfs_neigh_alloc();
	if (!neigh) {
		KLOG(KL_ERR, "no mem");
		return -ENOMEM;
	}

	neigh->d_ip = d_ip;
	neigh->d_port = d_port;
	neigh->s_ip = s_ip;
	neigh->s_port = s_port;

	hid = nkfs_host_id_lookup_or_create(nkfs_host, src_host_id);
	if (!hid) {
		err = -ENOMEM;
		KLOG(KL_ERR, "cat ref hid");
		goto deref_neigh;
	}
	nkfs_neigh_attach_host_id(neigh, hid);
	set_bit(NKFS_NEIGH_S_SHAKED, &neigh->state);

	err = nkfs_host_add_neigh(nkfs_host, neigh);
	if (err) {
		KLOG(KL_ERR, "cant add neigh err %d", err);
		goto deref_neigh;
	}
	nkfs_obj_id_copy(reply_host_id, &nkfs_host->host_id);

	return 0;

deref_neigh:
	NEIGH_DEREF(neigh);
	return err;
}

int nkfs_route_neigh_handshake(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;

	KLOG(KL_ERR, "peer %x", nkfs_con_peer_addr(con));

	err = nkfs_neigh_handshake(&pkt->u.neigh_handshake.src_net_id,
		&pkt->u.neigh_handshake.src_host_id,
		pkt->u.neigh_handshake.d_ip,
		pkt->u.neigh_handshake.d_port,
		pkt->u.neigh_handshake.s_ip,
		pkt->u.neigh_handshake.s_port,
		&reply->u.neigh_handshake.reply_host_id);

	return nkfs_con_send_reply(con, reply, err);
}

static int nkfs_neigh_heartbeat(struct nkfs_obj_id *src_net_id,
	struct nkfs_obj_id *src_host_id,
	struct nkfs_obj_id *dst_host_id,
	struct nkfs_obj_id *reply_host_id,
	struct nkfs_net_peer *reply_neighs,
	int reply_max_neighs,
	int *preply_nr_neighs)
{
	int err;
	struct nkfs_host_id *hid;

	KLOG(KL_INF, "handler");
	if (0 != nkfs_obj_id_cmp(src_net_id, &nkfs_host->net_id)) {
		KLOG(KL_ERR, "diff net id");
		err = -EINVAL;
		goto out;
	}

	if (0 != nkfs_obj_id_cmp(dst_host_id, &nkfs_host->host_id)) {
		KLOG(KL_ERR, "diff host id");
		err = -EINVAL;
		goto out;
	}

	hid = nkfs_host_id_lookup(nkfs_host, src_host_id);
	if (!hid) {
		KLOG(KL_ERR, "hid not found");
		err = -EINVAL;
		goto out;
	}

	err = 0;
	nkfs_obj_id_copy(reply_host_id, &nkfs_host->host_id);
	*preply_nr_neighs = 0;

	HOST_ID_DEREF(hid);
out:
	return err;
}

int nkfs_route_neigh_heartbeat(struct nkfs_con *con, struct nkfs_net_pkt *pkt,
	struct nkfs_net_pkt *reply)
{
	int err;

	KLOG(KL_ERR, "peer %x", nkfs_con_peer_addr(con));

	err = nkfs_neigh_heartbeat(&pkt->u.neigh_heartbeat.src_net_id,
			&pkt->u.neigh_heartbeat.src_host_id,
			&pkt->u.neigh_heartbeat.dst_host_id,
			&reply->u.neigh_heartbeat.reply_host_id,
			reply->u.neigh_heartbeat.reply_neighs,
			ARRAY_SIZE(reply->u.neigh_heartbeat.reply_neighs),
			&reply->u.neigh_heartbeat.reply_nr_neighs);

	return nkfs_con_send_reply(con, reply, err);
}
