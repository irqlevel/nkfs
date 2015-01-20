#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "route"
#define HOST_TIMER_TIMEOUT_MS 5000

struct ds_host *ds_host;
struct kmem_cache *ds_neigh_cachep;

static void ds_neighs_remove(struct ds_host *host,
	struct ds_neigh *neigh);

static int ds_neigh_connect(struct ds_neigh *neigh)
{
	int err;

	BUG_ON(neigh->con);
	err = ds_con_connect(neigh->d_ip, neigh->d_port, &neigh->con);
	return err;
}

static void ds_neigh_close(struct ds_neigh *neigh)
{
	if (neigh->con) {
		ds_con_close(neigh->con);
		neigh->con = NULL;
	}
}

static struct ds_neigh *ds_neigh_alloc(void)
{
	struct ds_neigh *neigh;

	neigh = kmem_cache_alloc(ds_neigh_cachep, GFP_NOIO);
	if (!neigh) {
		KLOG(KL_ERR, "cant alloc neigh");
		return NULL;
	}
	memset(neigh, 0, sizeof(*neigh));
	atomic_set(&neigh->ref, 1);
	atomic_set(&neigh->work_used, 0);
	return neigh;
}

static void ds_neigh_free(struct ds_neigh *neigh)
{
	kmem_cache_free(ds_neigh_cachep, neigh);
}

static void ds_neigh_release(struct ds_neigh *neigh)
{
	ds_neighs_remove(neigh->host, neigh);
	ds_neigh_close(neigh);
	ds_neigh_free(neigh);
}

void ds_neigh_ref(struct ds_neigh *neigh)
{
	BUG_ON(atomic_read(&neigh->ref) <= 0);
	atomic_inc(&neigh->ref);
}

void ds_neigh_deref(struct ds_neigh *neigh)
{
	BUG_ON(atomic_read(&neigh->ref) <= 0);	
	if (atomic_dec_and_test(&neigh->ref))
		ds_neigh_release(neigh);	
}

static struct ds_neigh *__ds_neighs_lookup(struct ds_host *host, u32 d_ip, int d_port)
{
	struct rb_node *n = host->neighs.rb_node;
	struct ds_neigh *found = NULL;

	while (n) {
		struct ds_neigh *neigh;
		int cmp;

		neigh = rb_entry(n, struct ds_neigh, neighs_link);
		cmp = ds_ip_port_cmp(d_ip, d_port, neigh->d_ip, neigh->d_port);
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

struct ds_neigh * ds_neighs_lookup(struct ds_host *host,
	u32 d_ip, int d_port)
{	
	struct ds_neigh *neigh;
	read_lock_irq(&host->neighs_lock);
	neigh = __ds_neighs_lookup(host, d_ip, d_port);
	if (neigh != NULL)
		NEIGH_REF(neigh);
	read_unlock_irq(&host->neighs_lock);
	return neigh;
}

static void ds_neighs_remove(struct ds_host *host,
	struct ds_neigh *neigh)
{
	struct ds_neigh *found;

	write_lock_irq(&host->neighs_lock);
	found = __ds_neighs_lookup(host, neigh->d_ip, neigh->d_port);
	if (found) {
		BUG_ON(found != neigh);
		rb_erase(&found->neighs_link, &host->neighs);
		host->neighs_active--;
	}
	write_unlock_irq(&host->neighs_lock);
}

static struct ds_neigh *__ds_neighs_insert(struct ds_host *host,
		struct ds_neigh *neigh)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ds_neigh *inserted = NULL;

	p = &host->neighs.rb_node;
	while (*p) {
		struct ds_neigh *found;
		int cmp;
		parent = *p;
		found = rb_entry(parent, struct ds_neigh, neighs_link);
		cmp = ds_ip_port_cmp(neigh->d_ip, neigh->d_port,
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
		host->neighs_active++;
		inserted = neigh;
	}
	NEIGH_REF(inserted);
	return inserted;
}

struct ds_neigh *ds_neighs_insert(struct ds_host *host,
		struct ds_neigh *neigh)
{
	struct ds_neigh *inserted;
	write_lock_irq(&host->neighs_lock);
	inserted = __ds_neighs_insert(host, neigh);
	write_unlock_irq(&host->neighs_lock);
	return inserted;
}

static int ds_host_queue_work(struct ds_host *host,
	work_func_t func, void *data)
{
	struct ds_host_work *work = NULL;

	work = kmalloc(sizeof(struct ds_host_work), GFP_ATOMIC);
	if (!work) {
		KLOG(KL_ERR, "cant alloc work");
		return -ENOMEM;
	}

	memset(work, 0, sizeof(*work));
	INIT_WORK(&work->work, func);
	work->host = host;
	work->data = data;

	if (!queue_work(host->wq, &work->work)) {
		kfree(work);
		KLOG(KL_ERR, "cant queue work");
		return -ENOMEM;
	}

	return 0;
}

static int ds_neigh_queue_work(struct ds_neigh *neigh,
	work_func_t func, void *data)
{
	if (0 == atomic_cmpxchg(&neigh->work_used, 0, 1)) {
		neigh->work_data = data;
		INIT_WORK(&neigh->work, func);
		if (!queue_work(neigh->host->wq, &neigh->work)) {
			atomic_set(&neigh->work_used, 0);
			return -EFAULT;
		}
		return 0;
	}
	return -EAGAIN;
}

static int ds_neigh_do_handshake(struct ds_neigh *neigh)
{
	int err;
	struct ds_net_pkt *req, *reply;
	struct ds_host *host = neigh->host;
	BUG_ON(neigh->con);

	KLOG(KL_INF, "handshake with %x:%d srv %x:%d",
		neigh->d_ip, neigh->d_port, neigh->s_ip, neigh->s_port);

	err = ds_neigh_connect(neigh);
	if (err) {
		KLOG(KL_ERR, "cant connect err %d", err);
		return err;
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

	req->type = DS_NET_PKT_NEIGH_HANDSHAKE;
	ds_obj_id_copy(&req->u.neigh_handshake.net_id, &host->net_id);
	ds_obj_id_copy(&req->u.neigh_handshake.host_id, &host->host_id);

	req->u.neigh_handshake.s_ip = neigh->d_ip;
	req->u.neigh_handshake.s_port = neigh->d_port;
	req->u.neigh_handshake.d_ip = neigh->s_ip;
	req->u.neigh_handshake.d_port = neigh->s_port;

	err = ds_con_send_pkt(neigh->con, req);
	if (err) {
		KLOG(KL_ERR, "send err %d", err);
		goto free_reply;
	}

	err = ds_con_recv_pkt(neigh->con, reply);
	if (err) {
		KLOG(KL_ERR, "recv err %d", err);
		goto free_reply;
	}
	
	ds_obj_id_copy(&neigh->host_id,
		&reply->u.neigh_handshake.reply_host_id);

	neigh->state = DS_NEIGH_VALID;
	KLOG(KL_INF, "neigh %p state %d", neigh, neigh->state);

free_reply:
	crt_free(reply);
free_req:
	crt_free(req);
close_con:
	ds_neigh_close(neigh);
	return err;
}

static void ds_neigh_handshake_work(struct work_struct *wrk)
{
	struct ds_neigh *neigh = container_of(wrk, struct ds_neigh, work);
	if (neigh->state == DS_NEIGH_INIT) {
		ds_neigh_do_handshake(neigh);
	}
	atomic_set(&neigh->work_used, 0);	
}

static void ds_host_connect_work(struct work_struct *wrk)
{
	struct ds_host_work *work = container_of(wrk,
			struct ds_host_work, work);
	struct ds_host *host = work->host;
	struct ds_neigh *neigh, *tmp;

	read_lock(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, list) {
		if (neigh->state == DS_NEIGH_INIT)
			ds_neigh_queue_work(neigh, ds_neigh_handshake_work,
					NULL);
	}
	read_unlock(&host->neighs_lock);
	kfree(work);
}

static void ds_host_timer_callback(unsigned long data)
{
	struct ds_host *host = (struct ds_host *)data;

	ds_host_queue_work(host, ds_host_connect_work, NULL);
	if (!host->stopping) {
		mod_timer(&host->timer, jiffies +
			msecs_to_jiffies(HOST_TIMER_TIMEOUT_MS));
	}
}

static struct ds_host *ds_host_alloc(void)
{
	struct ds_host *host;

	host = kmalloc(sizeof(*host), GFP_NOIO);
	if (!host)
		return NULL;
	memset(host, 0, sizeof(*host));
	return host;
}	

static void ds_host_free(struct ds_host *host)
{
	kfree(host);
}

static struct ds_host *ds_host_create(void)
{
	struct ds_host *host;
	int err;

	host = ds_host_alloc();
	if (!host)
		return NULL;
	
	ds_obj_id_gen(&host->net_id);
	ds_obj_id_gen(&host->host_id);

	host->neighs = RB_ROOT;
	rwlock_init(&host->neighs_lock);
	INIT_LIST_HEAD(&host->neigh_list);

	host->wq = alloc_workqueue("ds_route_wq",
			WQ_MEM_RECLAIM|WQ_UNBOUND, 1);
	if (!host->wq) {
		KLOG(KL_ERR, "cant create wq");
		goto free_host;
	}

	setup_timer(&host->timer, ds_host_timer_callback, (unsigned long)host);
	err = mod_timer(&host->timer,
			jiffies +
			msecs_to_jiffies(HOST_TIMER_TIMEOUT_MS));
	if (err) {
		KLOG(KL_ERR, "mod_timer failed with err=%d", err);
		goto del_wq;
	}

	return host;

del_wq:
	destroy_workqueue(host->wq);
free_host:
	ds_host_free(host);	
	return NULL;
}

static void ds_host_release(struct ds_host *host)
{
	struct ds_neigh *neigh, *tmp;
	struct list_head neigh_list;

	host->stopping = 1;
	del_timer_sync(&host->timer);
	destroy_workqueue(host->wq);
	
	INIT_LIST_HEAD(&neigh_list);
	
	write_lock_irq(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, list) {
		list_del_init(&neigh->list);
		list_add_tail(&neigh->list, &neigh_list);
	}
	write_unlock_irq(&host->neighs_lock);	

	list_for_each_entry_safe(neigh, tmp, &neigh_list, list) {
		list_del_init(&neigh->list);
		NEIGH_DEREF(neigh);
	}
	BUG_ON(host->neighs_active);

	ds_host_free(host);
}

int ds_route_init(void)
{
	int err;

	ds_host = ds_host_create();
	if (!ds_host) {
		KLOG(KL_ERR, "cant create host");
		return -ENOMEM;
	}

	ds_neigh_cachep = kmem_cache_create("ds_neigh_cache",
		sizeof(struct ds_neigh), 0, SLAB_MEM_SPREAD, NULL);
	if (!ds_neigh_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto rel_host;
	}

	return 0;
rel_host:
	ds_host_release(ds_host);
	return err;
}

void ds_route_finit(void)
{
	ds_host_release(ds_host);
	kmem_cache_destroy(ds_neigh_cachep);
}

int ds_host_add_neigh(struct ds_host *host, struct ds_neigh *neigh)
{
	struct ds_neigh *inserted;
	int err = -EEXIST;

	write_lock_irq(&host->neighs_lock);
	neigh->host = host;	
	inserted = __ds_neighs_insert(host, neigh);
	BUG_ON(!inserted);
	if (inserted == neigh) {
		list_add_tail(&neigh->list, &host->neigh_list);
		err = 0;
	}
	NEIGH_DEREF(inserted);
	write_unlock_irq(&host->neighs_lock);
	return err;
}

int ds_host_remove_neigh(struct ds_host *host, u32 d_ip, int d_port)
{
	struct ds_neigh *neigh, *tmp;
	struct list_head neigh_list;	
	int found = 0;

	INIT_LIST_HEAD(&neigh_list);

	write_lock_irq(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, list) {
		if (neigh->d_ip == d_ip && neigh->d_port == d_port) {
			list_del_init(&neigh->list);
			list_add_tail(&neigh->list, &neigh_list);
		}
	}
	write_unlock_irq(&host->neighs_lock);

	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, list) {
		found++;
		list_del_init(&neigh->list);
		NEIGH_DEREF(neigh);
	}

	return (found) ? 0 : -ENOENT;
}

int ds_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port)
{
	struct ds_neigh *neigh;
	int err;

	neigh = ds_neigh_alloc();
	if (!neigh) {
		KLOG(KL_ERR, "no mem");
		return -ENOMEM;
	}
	neigh->d_ip = d_ip;
	neigh->d_port = d_port;
	neigh->s_ip = s_ip;
	neigh->s_port = s_port;
	neigh->state = DS_NEIGH_INIT;
	err = ds_host_add_neigh(ds_host, neigh); 
	if (err) {
		KLOG(KL_ERR, "cant add neigh");
		ds_neigh_free(neigh);
	}

	return err;
}

int ds_neigh_remove(u32 d_ip, int d_port)
{
	return ds_host_remove_neigh(ds_host, d_ip, d_port);
}

int ds_neigh_handshake(struct ds_obj_id *net_id,
	struct ds_obj_id *host_id, 
	u32 d_ip, int d_port,
	u32 s_ip, int s_port,
	struct ds_obj_id *reply_host_id)
{
	struct ds_neigh *neigh;
	int err;

	if (0 != ds_obj_id_cmp(net_id, &ds_host->net_id)) {
		KLOG(KL_ERR, "diff net id");
		return -EINVAL;
	}

	neigh = ds_neigh_alloc();
	if (!neigh) {
		KLOG(KL_ERR, "no mem");
		return -ENOMEM;
	}

	neigh->d_ip = d_ip;
	neigh->d_port = d_port;
	neigh->s_ip = s_ip;
	neigh->s_port = s_port;
	ds_obj_id_copy(&neigh->host_id, host_id);
	neigh->state = DS_NEIGH_VALID;

	err = ds_host_add_neigh(ds_host, neigh);
	if (err) {
		KLOG(KL_ERR, "cant add neigh");
		goto free_neigh;
	}
	ds_obj_id_copy(reply_host_id, &ds_host->host_id);

	return 0;

free_neigh:
	ds_neigh_free(neigh);
	return err;
}
