#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "route"

struct ds_host *host;
struct kmem_cache *ds_neigh_cachep;

void ds_neighs_remove(struct ds_host *host,
	struct ds_neigh *neigh);

struct ds_neigh *ds_neigh_alloc(void)
{
	struct ds_neigh *neigh;

	neigh = kmem_cache_alloc(ds_neigh_cachep, GFP_NOIO);
	if (!neigh) {
		KLOG(KL_ERR, "cant alloc neigh");
		return NULL;
	}
	memset(neigh, 0, sizeof(*neigh));
	atomic_set(&neigh->ref, 1);
	return neigh;
}

void ds_neigh_free(struct ds_neigh *neigh)
{
	kmem_cache_free(ds_neigh_cachep, neigh);
}

void ds_neigh_release(struct ds_neigh *neigh)
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

int ds_route_init(void)
{
	ds_neigh_cachep = kmem_cache_create("ds_neigh_cache",
		sizeof(struct ds_neigh), 0, SLAB_MEM_SPREAD, NULL);
	if (!ds_neigh_cachep) {
		KLOG(KL_ERR, "cant create cache");
		return -ENOMEM;
	}

	return 0;
}

void ds_route_finit(void)
{
	kmem_cache_destroy(ds_neigh_cachep);
}
