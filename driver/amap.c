#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ds-amap"

/* Disk map is a limited cache of pages of on disk data that
 * frequently used by client of disk map.
 * Structures:
 * 	struct amap - one per disk, contains settings:
 * 				disk read/write provider link,
 * 				node settings
 * 				and set of nodes with LRU evicting by age
 * 	struct amap_node - structure that caches continues region of
 * 				disk (set of pages)
 *
 * Algorithm:
 * 	once disk map inited, user can read/write buf's with offset and size
 * 	to disk through such amap.
 * 	disk map will read data into corresponding map nodes,
 * 	so user will read data directly from memory instead of disk access.
 * 	each reference of disk map node increase it's age by shift of age bits.
 * 	there is a timer that scans all disk maps and evicts disk map nodes by
 * 	lower age. User can pin nodes that he wants to be resident, so they are
 * 	not evicted from disk map. Dirty pages are flushed to disk when nodes
 * 	evicted or count of nodes reached limit or disk map stopped.
 */

static atomic_t amn_count = ATOMIC_INIT(0);
static struct kmem_cache *amap_nodes_cachep;
static struct timer_list amap_timer;
static struct workqueue_struct *amap_wq;

/* Disk maps global timer timeout - 1sec */
#define AMAP_TIMER_TIMEOUT_MSECS 1000

static DEFINE_MUTEX(amaps_list_lock);
static LIST_HEAD(amaps_list);

static void amap_node_free(struct amap_node *node)
{
	if (node->page)
		put_page(node->page);

	kmem_cache_free(amap_nodes_cachep, node);
	atomic_dec(&amn_count);
}

void amap_node_deref(struct amap_node *node)
{
	if (atomic_read(&node->refs) < 1) {
		klog(KL_ERR, "node=%p, index=%lu", node, node->index);
	}

	BUG_ON(atomic_read(&node->refs) < 1);
	if (atomic_dec_and_test(&node->refs)) {
		amap_node_free(node);
	}
}

static struct
amap_node *amap_node_alloc(unsigned long index)
{
	struct amap_node *node;

	node = kmem_cache_alloc(amap_nodes_cachep, GFP_NOIO);
	if (!node) {
		klog(KL_ERR, "cant alloc node");
		return NULL;
	}
	atomic_inc(&amn_count);
	memset(node, 0, sizeof(*node));
	atomic_set(&node->refs, 1);
	node->index = index;
	node->page = alloc_page(GFP_NOIO);
	if (!node->page) {
		klog(KL_ERR, "cant alloc page");
		goto fail;
	}

	return node;
fail:
	amap_node_deref(node);
	return NULL;
}

static void amap_node_age(struct amap_node *node)
{
	node->age = node->age >> 1;
}

int amap_init(struct amap *map, void *io, int nr_max_nodes, io_op_t io_op)
{
	memset(map, 0, sizeof(struct amap));
	atomic_set(&map->refs, 1);
	spin_lock_init(&map->nodes_lock);
	spin_lock_init(&map->lru_list_lock);
	INIT_RADIX_TREE(&map->nodes, GFP_ATOMIC);
	INIT_LIST_HEAD(&map->lru_list);

	map->nr_nodes = 0;
	map->nr_max_nodes = nr_max_nodes;
	map->io = io;
	map->io_op = io_op;

	mutex_init(&map->age_mutex);
	mutex_lock(&amaps_list_lock);
	list_add_tail(&map->maps_list, &amaps_list);
	mutex_unlock(&amaps_list_lock);

	klog(KL_INF, "nr_max_nodes %u", map->nr_max_nodes);

	return 0;
}

static int amap_insert(struct amap *map, struct amap_node *node)
{
	int err;

	if (radix_tree_preload(GFP_NOIO))
		return -ENOMEM;

	spin_lock(&map->nodes_lock);
	if (radix_tree_insert(&map->nodes, node->index, node)) {
		err = -EINVAL;
	} else {
		map->nr_nodes++;
		atomic_inc(&node->refs);
		err = 0;
	}
	spin_unlock(&map->nodes_lock);
	radix_tree_preload_end();

	if (!err) {
		spin_lock(&map->lru_list_lock);
		list_add_tail(&node->lru_list, &map->lru_list);
		atomic_inc(&node->refs);
		spin_unlock(&map->lru_list_lock);
	}

	return 0;
}

static struct
amap_node *amap_lookup(struct amap *map, unsigned long index)
{
	struct amap_node *node;

	spin_lock(&map->nodes_lock);
	node = radix_tree_lookup(&map->nodes, index);
	if (node)
		atomic_inc(&node->refs);
	spin_unlock(&map->nodes_lock);

	if (node) {
		node->age |= (1ull << 63);
		spin_lock(&map->lru_list_lock);
		list_del(&node->lru_list);
		list_add_tail(&node->lru_list, &map->lru_list);
		spin_unlock(&map->lru_list_lock);
	}

	return node;
}

static struct
amap_node *amap_remove(struct amap *map, unsigned long index)
{
	struct amap_node *node;

	spin_lock(&map->nodes_lock);
	node = radix_tree_delete(&map->nodes, index);
	if (node) {
		BUG_ON(node->index != index);
		map->nr_nodes--;
		amap_node_deref(node);
	}
	spin_unlock(&map->nodes_lock);
	if (node) {
		spin_lock(&map->lru_list_lock);
		list_del(&node->lru_list);
		amap_node_deref(node);
		spin_unlock(&map->lru_list_lock);
	}

	return node;
}

static void amap_nodes_free(struct amap *map)
{
	struct amap_node *batch[4];
	int nr_found, index;
	struct amap_node *node, *removed;

	klog(KL_INF, "map->nr_nodes=%d", map->nr_nodes);
	klog(KL_INF, "amn_count=%d", atomic_read(&amn_count));

	for (;;) {
		spin_lock(&map->nodes_lock);
		nr_found = radix_tree_gang_lookup(&map->nodes, (void **)batch,
				0, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			atomic_inc(&node->refs);
		}
		spin_unlock(&map->nodes_lock);
		if (nr_found == 0)
			break;

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			removed = amap_remove(map, node->index);
			BUG_ON(removed != node);
			amap_node_deref(node);
			amap_node_deref(node);
		}
	}

	klog(KL_INF, "amn_count=%d", atomic_read(&amn_count));
	klog(KL_INF, "map->nr_nodes=%d", map->nr_nodes);
	BUG_ON(map->nr_nodes != 0);
	BUG_ON(!list_empty(&map->lru_list));
}

static void __amap_release(struct amap *map)
{
	amap_nodes_free(map);
	klog(KL_INF, "released");
}

static void amap_deref(struct amap *map)
{
	if (atomic_read(&map->refs) < 1) {
		klog(KL_ERR, "map=%p", map);
	}

	BUG_ON(atomic_read(&map->refs) < 1);
	if (atomic_dec_and_test(&map->refs)) {
		__amap_release(map);
	}
}

void amap_release(struct amap *map)
{
	klog(KL_INF, "releasing");
	mutex_lock(&amaps_list_lock);
	list_del(&map->maps_list);
	mutex_unlock(&amaps_list_lock);
	amap_deref(map);
}

static int amap_node_age_cmp(const void *a, const void *b)
{
	struct amap_node *node_a = *((struct amap_node **)a);
	struct amap_node *node_b = *((struct amap_node **)b);

	if (node_a->age > node_b->age)
		return 1;
	else if (node_a->age < node_b->age)
		return -1;
	else
		return 0;
}

static void amap_node_ptr_swap(void *a, void *b, int size)
{
	struct amap_node **node_a = a;
	struct amap_node **node_b = b;
	struct amap_node *tmp;

	tmp = *node_a;
	*node_a = *node_b;
	*node_b = tmp;
}

static void amap_lru_frees(struct amap *map)
{
	struct amap_node *batch[4];
	int nr_found;
	unsigned long index, first_index = 0;
	struct amap_node *node, *removed, *prev;
	struct amap_node **nodes;
	struct page *page;
	int nr_nodes = 0;
	int nodes_limit = PAGE_SIZE/sizeof(struct amap_node *);

	/* If count of nodes lower than a limit no evicting required */
	if (map->nr_nodes <= map->nr_max_nodes)
		return;

	/* Alloc page that will contains set of pointers to disk map nodes
	 * that are sorted by it's age. So we cant sort more than
	 * PAGE_SIZE/sizeof(void *) nodes.
	 */
	page = alloc_page(GFP_NOIO);
	if (!page) {
		klog(KL_ERR, "no memory");
		return;
	}

	nodes = kmap(page);

	/* Lock disk map age mutex, so timer work will not updates age's
	 * while sorting nodes
	 */
	mutex_lock(&map->age_mutex);
	for (;;) {

		/* Snapshot piece of nodes tree, increase
		 * search start index - first_index to get next piece
		 * next time
		 */
		spin_lock(&map->nodes_lock);
		nr_found = radix_tree_gang_lookup(&map->nodes, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			atomic_inc(&node->refs);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock(&map->nodes_lock);

		/* Nothing found means scanned all tree, so stop */
		if (nr_found == 0)
			break;

		/* Insert pointers of dirty nodes that we found in nodes tree
		 * into page
		 */
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			if (nr_nodes < nodes_limit) {
				nodes[nr_nodes++] = node;
			} else {
				klog(KL_ERR, "nr_nodes=%d\
						vs. nodes_limit=%d",
						nr_nodes, nodes_limit);
				goto release;
			}
		}
	}

	/* Quick sort nodes by it's age */
	sort(nodes, nr_nodes, sizeof(struct amap_node *),
			amap_node_age_cmp, amap_node_ptr_swap);
	prev = NULL;
	for (index = 0; index < nr_nodes; index++) {
		node = nodes[index];
		if (prev && (prev->age > node->age)) {
			int i;
			struct amap_node *n;
			for (i = 0; i < nr_nodes; i++) {
				n = nodes[i];
				klog(KL_ERR, "node=%p, age=%lx, index=%lu",
						n, n->age, n->index);
			}
			BUG_ON(1);
		}
		prev = node;
		/* Evict nodes from disk map with lower age */
		if (map->nr_nodes > map->nr_max_nodes) {
			removed = amap_remove(map, node->index);
			BUG_ON(removed != node);
			amap_node_deref(node);
		}
	}

release:
	mutex_unlock(&map->age_mutex);
	for (index = 0; index < nr_nodes; index++) {
		node = nodes[index];
		amap_node_deref(node);
	}

	kunmap(page);
	put_page(page);
}

/* Dump disk map nodes info by printk. For debug. */
void amap_dump(struct amap *map)
{
	struct amap_node *batch[4];
	int nr_found;
	unsigned long index, first_index = 0;
	struct amap_node *node;

	for (;;) {
		spin_lock(&map->nodes_lock);
		nr_found = radix_tree_gang_lookup(&map->nodes, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			atomic_inc(&node->refs);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock(&map->nodes_lock);
		if (nr_found == 0) {
			break;
		}

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			klog(KL_INF, "n=%p i=%lu age=%lx refs=%d",
					node, node->index, node->age, atomic_read(&node->refs));
			amap_node_deref(node);
		}
	}
}

/* Compact disk map by evicting dirties LRU nodes */
static void amap_compact(struct amap *map)
{
	for (;;) {
		if (map->nr_nodes > map->nr_max_nodes) {
			amap_lru_frees(map);
		} else {
			break;
		}
	}
}

/* Find and reference map node by it's index */
static int amap_get_node(struct amap *map, unsigned long index,
		struct amap_node **pnode)
{
	struct amap_node *node;
	int err;

	*pnode = NULL;
	node = amap_lookup(map, index);
	if (node) {
		*pnode = node;
		return 0;
	}

	amap_compact(map);
	node = amap_node_alloc(index);
	if (!node) {
		klog(KL_ERR, "cant alloc map node");
		return -ENOMEM;
	}

	err = amap_insert(map, node);
	if (err) {
		amap_node_deref(node);
		klog(KL_ERR, "cant insert map node, err=%d", err);
		return err;
	}
	atomic_inc(&node->refs);
	*pnode = node;
	return 0;
}

static void amap_req_free(struct amap_req *req)
{
	BUG_ON(!list_empty(&req->req_list));

	kfree(req);
}

void amap_node_io_complete(void *context, int err, int rw, struct page *page, u64 off)
{
	struct amap_node *node = (struct amap_node *)context;
	struct amap_req *req, *tmp, *nreq = NULL;
	struct list_head req_list, comp_list, n_list;
	int state;

	INIT_LIST_HEAD(&req_list);
	INIT_LIST_HEAD(&comp_list);
	INIT_LIST_HEAD(&n_list);

	spin_lock_irq(&node->lock);

	BUG_ON(node->state != AMAP_NODE_S_LOCKED);
	BUG_ON(!node->req);
	BUG_ON(node->req->page != page);
	BUG_ON(node->req->off != off);
	BUG_ON(!list_empty(&node->req->req_list));

	req = node->req;
	req->err = err;
	list_add_tail(&req->req_list, &comp_list);
	node->req = NULL;
	state = node->state;
	if (!err) {
		memcpy(page_address(node->page), page_address(page), PAGE_SIZE);
		state = AMAP_NODE_S_READY;
	}

	list_for_each_entry_safe(req, tmp, &node->req_list, req_list) {
		list_del_init(&req->req_list);
		list_add_tail(&req->req_list, &req_list);
	}
	
	spin_unlock_irq(&node->lock);

	list_for_each_entry_safe(req, tmp, &req_list, req_list) {
		list_del_init(&req->req_list);
		switch (state) {
			case AMAP_NODE_S_READY:
				if (!req->rw) {
					memcpy(page_address(req->page), page_address(node->page), PAGE_SIZE);
					list_add_tail(&req->req_list, &comp_list);
				} else {
					BUG_ON(!nreq);
					nreq = req;
					state = AMAP_NODE_S_LOCKED;
				}
				break;
			case AMAP_NODE_S_LOCKED:
				list_add_tail(&req->req_list, &n_list);
				break;
			case AMAP_NODE_S_INVALID:
				BUG_ON(!nreq);
				nreq = req;
				state = AMAP_NODE_S_LOCKED;
				break;
		}
	}
	
	list_for_each_entry_safe(req, tmp, &comp_list, req_list) {
		req->complete(req->context, req->err, req->rw, req->page, req->off);
		amap_req_free(req);
	}

	spin_lock_irq(&node->lock);
	list_for_each_entry_safe_reverse(req, tmp, &n_list, req_list) {
		list_del_init(&req->req_list);
		list_add(&req->req_list, &node->req_list);
	}

	BUG_ON(node->req);
	if (nreq) {
		node->req = nreq;
	}
	node->state = state;
	spin_unlock_irq(&node->lock);
	if (nreq)
		node->owner->io_op(node->owner->io,
			nreq->rw, nreq->page, nreq->off, node, amap_node_io_complete);
	else
		amap_node_deref(node);
}

void amap_io(struct amap *map, int rw, struct page *page, u64 off, void *context, io_complete_t complete)
{
	unsigned long index = off/PAGE_SIZE;
	int err;
	struct amap_req *req;
	struct amap_node *node;

	BUG_ON(off & (PAGE_SIZE - 1));

	req = kmalloc(sizeof(struct amap_req), GFP_ATOMIC);
	if (!req) {
		complete(context, -ENOMEM, rw, page, off);
		return;	
	}

	memset(req, 0, sizeof(*req));
	INIT_LIST_HEAD(&req->req_list);
	req->page = page;
	req->context = context;	
	req->off = off;
	req->rw = 0;
	
	err = amap_get_node(map, index, &node);
	if (err) {
		complete(context, -ENOMEM, rw, page, off);
		amap_req_free(req);
		return;
	}

	spin_lock_irq(&node->lock);
	switch (node->state) {
		case AMAP_NODE_S_READY:
			BUG_ON(node->req);
			if (!rw) {
				memcpy(page_address(page), page_address(node->page), PAGE_SIZE);
				spin_unlock_irq(&node->lock);
				amap_req_free(req);
				complete(context, 0, rw, page, off);
				return;
			} else {
				node->req = req;
				node->state = AMAP_NODE_S_LOCKED;
				spin_unlock_irq(&node->lock);
				map->io_op(map->io, rw, page, off, node, amap_node_io_complete);
			}
		case AMAP_NODE_S_LOCKED:
			list_add_tail(&req->req_list, &node->req_list);
			spin_unlock_irq(&node->lock);
			return;
		case AMAP_NODE_S_INVALID:
			BUG_ON(node->req);
			node->req = req;
			node->state = AMAP_NODE_S_LOCKED;
			spin_unlock_irq(&node->lock);
			map->io_op(map->io, rw, page, off, node, amap_node_io_complete);
			return;
		default:
			BUG();
			spin_unlock_irq(&node->lock);
			return;
	}
}

/* Do aging of disk map nodes */
static void amap_do_age(struct amap *map)
{
	struct amap_node *batch[4];
	int nr_found;
	unsigned long index, first_index = 0;
	struct amap_node *node;

	mutex_lock(&map->age_mutex);
	for (;;) {
		spin_lock(&map->nodes_lock);
		nr_found = radix_tree_gang_lookup(&map->nodes, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			atomic_inc(&node->refs);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock(&map->nodes_lock);
		if (nr_found == 0) {
			break;
		}

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			amap_node_age(node);
			amap_node_deref(node);
		}
	}
	mutex_unlock(&map->age_mutex);
	amap_deref(map);
}

/* Scan all disk maps and aging nodes */
static void amap_age_work(struct work_struct *work)
{
	struct amap *map, *next;
	mutex_lock(&amaps_list_lock);
	list_for_each_entry_safe(map, next, &amaps_list, maps_list) {
		atomic_inc(&map->refs);
		amap_do_age(map);
	}
	mutex_unlock(&amaps_list_lock);
	kfree(work);
}

/* Alloc work and queue it's to worker */
static int amap_queue_work(work_func_t func)
{
	struct work_struct *work = NULL;

	work = kzalloc(sizeof(struct work_struct), GFP_ATOMIC);
	if (!work) {
		klog(KL_ERR, "cant alloc work");
		return -ENOMEM;
	}

	INIT_WORK(work, func);
	if (!queue_work(amap_wq, work)) {
		kfree(work);
		klog(KL_ERR, "cant queue work");
		return -ENOMEM;
	}
	return 0;
}

/* Timer callback, do work and set timer to fire next period */
static void amap_timer_callback(unsigned long data)
{
	amap_queue_work(amap_age_work);
	mod_timer(&amap_timer,
			jiffies +
			msecs_to_jiffies(AMAP_TIMER_TIMEOUT_MSECS));
}

int amap_sys_init(void)
{
	int err;

	amap_nodes_cachep = kmem_cache_create("amap_nodes",
		sizeof(struct amap_node),
		__alignof__(struct amap_node), 0, NULL);
	if (!amap_nodes_cachep) {
		klog(KL_ERR, "kmem_cache_create failed");
		err = -ENOMEM;
		goto out;
	}

	amap_wq = alloc_workqueue("amap_wq",
			WQ_MEM_RECLAIM|WQ_UNBOUND, 1);
	if (!amap_wq) {
		klog(KL_ERR, "cant create wq");
		err = -ENOMEM;
		goto rel_cache;
	}

	setup_timer(&amap_timer, amap_timer_callback, 0);
	err = mod_timer(&amap_timer,
			jiffies +
			msecs_to_jiffies(AMAP_TIMER_TIMEOUT_MSECS));
	if (err) {
		klog(KL_ERR, "mod_timer failed with err=%d", err);
		goto del_wq;
	}

	return 0;
del_wq:
	destroy_workqueue(amap_wq);
rel_cache:
	kmem_cache_destroy(amap_nodes_cachep);
out:
	return err;
}

void amap_sys_release(void)
{
	del_timer_sync(&amap_timer);
	destroy_workqueue(amap_wq);
	kmem_cache_destroy(amap_nodes_cachep);
}
