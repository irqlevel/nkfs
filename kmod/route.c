#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "route"
#define HOST_TIMER_TIMEOUT_MS 1000

struct ds_host *ds_host;
struct kmem_cache *ds_neigh_cachep;

void ds_neighs_remove(struct ds_host *host,
	struct ds_neigh *neigh);

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

struct ds_neigh *__ds_neighs_lookup(struct ds_host *host,
	struct ds_obj_id *host_id)
{
	struct rb_node *n = host->neighs.rb_node;
	struct ds_neigh *found = NULL;

	while (n) {
		struct ds_neigh *neigh;
		int cmp;

		neigh = rb_entry(n, struct ds_neigh, neighs_link);
		cmp = ds_obj_id_cmp(host_id, &neigh->host_id);
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
	struct ds_obj_id *host_id)
{	
	struct ds_neigh *neigh;
	read_lock_irq(&host->neighs_lock);
	neigh = __ds_neighs_lookup(host, host_id);
	if (neigh != NULL)
		NEIGH_REF(neigh);
	read_unlock_irq(&host->neighs_lock);
	return neigh;
}

void ds_neighs_remove(struct ds_host *host,
	struct ds_neigh *neigh)
{
	struct ds_neigh *found;

	write_lock_irq(&host->neighs_lock);
	found = __ds_neighs_lookup(host, &neigh->host_id);
	if (found) {
		BUG_ON(found != neigh);
		rb_erase(&found->neighs_link, &host->neighs);
		host->neighs_active--;
	}
	write_unlock_irq(&host->neighs_lock);
}

struct ds_neigh *ds_neighs_insert(struct ds_host *host,
		struct ds_neigh *neigh)
{
	struct rb_node **p = &host->neighs.rb_node;
	struct rb_node *parent = NULL;
	struct ds_neigh *inserted = NULL;

	write_lock_irq(&host->neighs_lock);
	while (*p) {
		struct ds_neigh *found;
		int cmp;
		parent = *p;
		found = rb_entry(parent, struct ds_neigh, neighs_link);
		cmp = ds_obj_id_cmp(&neigh->host_id, &found->host_id);
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

int ds_neigh_do_connect(struct ds_neigh *neigh)
{
	int err;
	BUG_ON(neigh->con);

	err = ds_con_connect(neigh->ip, neigh->port, &neigh->con);
	if (err)
		return err;

	neigh->state = DS_NEIGH_CONNECTED;

	return err;
}

static void ds_neigh_connect_work(struct work_struct *wrk)
{
	struct ds_neigh *neigh = container_of(wrk, struct ds_neigh, work);
	if (neigh->state == DS_NEIGH_INITED) {
		ds_neigh_do_connect(neigh);
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
		if (neigh->state == DS_NEIGH_INITED)
			ds_neigh_queue_work(neigh,
				ds_neigh_connect_work, NULL);
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

void ds_host_add_neigh(struct ds_host *host, struct ds_neigh *neigh)
{
	neigh->host = host;
	write_lock_irq(&host->neighs_lock);
	list_add_tail(&neigh->list, &host->neigh_list);
	write_unlock_irq(&host->neighs_lock);
}

int ds_host_remove_neigh(struct ds_host *host, u32 ip, int port)
{
	struct ds_neigh *neigh, *tmp;
	struct list_head neigh_list;	
	int found = 0;

	INIT_LIST_HEAD(&neigh_list);

	write_lock_irq(&host->neighs_lock);
	list_for_each_entry_safe(neigh, tmp, &host->neigh_list, list) {
		if (neigh->ip == ip && neigh->port == port) {
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

int ds_neigh_add(u32 ip, int port)
{
	struct ds_neigh *neigh;
	neigh = ds_neigh_alloc();
	if (!neigh) {
		KLOG(KL_ERR, "no mem");
		return -ENOMEM;
	}
	neigh->ip = ip;
	neigh->port = port;
	neigh->state = DS_NEIGH_INITED;
	ds_host_add_neigh(ds_host, neigh); 
	return 0;
}

int ds_neigh_remove(u32 ip, int port)
{
	return ds_host_remove_neigh(ds_host, ip, port);
}
