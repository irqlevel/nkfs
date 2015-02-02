#pragma once

static struct kmem_cache *dio_dev_cachep;
static struct kmem_cache *dio_clu_cachep;

static void dio_dev_init(struct dio_dev *dev, struct bdev *bdev, u32 clu_size, u32 nr_max_clus)
{
	memset(dev, 0, sizeof(dev));
	atomic_set(&dev->ref, 1);
	spin_lock_init(&dev->clus_lock);

	INIT_RADIX_TREE(&dev->clus_root, GFP_NOIO);

	dev->nr_clus = 0;
	dev->nr_max_clus = nr_max_clus;

	mutex_init(&map->age_mutex);
	mutex_lock(&dio_dev_list_lock);
	list_add_tail(&dev->list, &dio_dev_list);
	mutex_unlock(&dio_dev_list_lock);
}

struct dio_dev *dio_dev_create(struct bdev *bdev, u32 clu_size, u32 nr_max_clus)
{
	struct dio_dev *dev;

	if (clu_size & (PAGE_SIZE - 1))
		return NULL;
	if (nr_max_clus == 0)
		return NULL;
	if (clu_size > (DIO_CLU_MAX_PAGES*PAGE_SIZE))
		return NULL;

	dev = kmem_cache_alloc(dio_dev_cachep, GFP_NOIO);
	if (!dev)
		return NULL;
	
	dio_dev_init(dev, bdev, clu_size, nr_max_clus);

	return dev;
}

void dio_dev_deref(struct dio_dev *dev)
{
	if (atomic_dec_and_test(&dev->ref))
		dio_dev_release(dev);
}

static struct dio_cluster *dio_clu_alloc(struct dio_dev *dev)
{
	struct dio_cluster *cluster;
	u32 nr_pages, i;

	BUG_ON(dev->clu_size & (PAGE_SIZE - 1));
	BUG_ON(dev->clu_size < PAGE_SIZE);
	BUG_ON(dev->clu_size > (DIO_CLU_MAX_PAGES * PAGE_SIZE));

	nr_pages = dev->clu_size >> PAGE_SHIFT;
	cluster = kmem_cache_alloc(dio_clu_cachep);
	if (!cluster)
		return NULL;

	memset(cluster, 0, sizeof(*cluster));
	set_bit(DIO_CLU_INV, &cluster->flags);
	for (i = 0; i < nr_pages; i++) {
		cluster->pages[i] = alloc_page(GFP_NOIO);
		if (!cluster->pages[i]) {
			u32 j;
			for (j = 0; j < i; j++)
				put_page(cluster->pages[j]);
			kmem_cache_free(dio_clu_cachep, cluster);
			return NULL;
		}
	}
	atomic_set(&cluster->ref, 1);
	cluster->dev = dev;
	cluster->nr_pages = nr_pages;
	return cluster;
}

static int dio_clu_insert(struct dio_dev *dev, struct dio_cluster *cluster)
{
	int err;

	if (radix_tree_preload(GFP_NOIO))
		return -ENOMEM;

	spin_lock_irq(&dev->clus_lock);
	if (radix_tree_insert(&dev->clus_root, cluster->index, cluster)) {
		err = -EINVAL;
	} else {
		dev->nr_clus++;
		atomic_inc(&cluster->ref);
		err = 0;
	}
	spin_unlock_irq(&dev->clus_lock);
	radix_tree_preload_end();

	return err;
}

static struct
dio_cluster *dio_clu_lookup(struct dio_dev *dev, unsigned long index)
{
	struct dio_cluster *cluster;

	spin_lock_irq(&dev->clus_lock);
	cluster = radix_tree_lookup(&dev->clus_root, index);
	if (cluster)
		atomic_inc(&cluster->ref);
	spin_unlock_irq(&dev->clus_lock);

	if (cluster) {
		cluster->age |= (1ull << 63);
	}

	return cluster;
}

static struct
dio_cluster *dio_clu_remove(struct dio_dev *dev, unsigned long index)
{
	struct dio_cluster *cluster;

	spin_lock_irq(&dev->clus_lock);
	cluster = radix_tree_delete(&dev->clus_root, index);
	if (cluster) {
		BUG_ON(cluster->index != index);
		dev->nr_clus--;
	}
	spin_unlock_irq(&dev->clus_lock);
	if (cluster)
		dio_clu_deref(cluster);

	return cluster;
}

static void dio_clus_release(struct dio_dev *dev)
{
	struct dio_cluster *batch[16];
	int nr_found, index;
	struct dio_cluster *cluster, *removed;

	KLOG(KL_INF, "nr_clus=%d", dev->nr_clus);

	for (;;) {
		spin_lock_irq(&dev->clus_lock);
		nr_found = radix_tree_gang_lookup(&dev->clus_root, (void **)batch,
				0, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			cluster = batch[index];
			atomic_inc(&cluster->ref);
		}
		spin_unlock(&dev->clus_lock);
		if (nr_found == 0)
			break;

		for (index = 0; index < nr_found; index++) {
			cluster = batch[index];
			removed = dio_clu_remove(dev, cluster->index);
			BUG_ON(removed != cluster);
			dio_cluster_deref(node);
			dio_cluster_deref(node);
		}
	}

	BUG_ON(dev->nr_clus);
}

static void dio_dev_release(struct dio_dev *dev)
{
	mutex_lock(&dio_dev_list_lock);
	list_del(&dev->list);
	mutex_unlock(&dio_dev_list_lock);

	dio_clus_release(dev);
	kmem_cache_free(dio_dev_cachep, dev);
}

void dio_dev_deref(struct dio_dev *dev)
{
	if (atomic_dec_and_test(&dev->ref))
		dio_dev_release(dev);
}

static int dio_clu_age_cmp(const void *a, const void *b)
{
	struct dio_cluster *node_a = *((struct dio_cluster **)a);
	struct dio_cluster *node_b = *((struct dio_cluster **)b);

	if (node_a->age > node_b->age)
		return 1;
	else if (node_a->age < node_b->age)
		return -1;
	else
		return 0;
}

static void dio_clu_ptr_swap(void *a, void *b, int size)
{
	struct dio_cluster **node_a = a;
	struct dio_cluster **node_b = b;
	struct dio_cluster *tmp;

	tmp = *node_a;
	*node_a = *node_b;
	*node_b = tmp;
}

static void dio_clus_lru_frees(struct dio_dev *dev)
{
	struct dio_cluster *batch[16];
	int nr_found;
	unsigned long index, first_index = 0;
	struct dio_cluster *node, *removed, *prev;
	struct dio_cluster **nodes;
	struct page *page;
	int nr_nodes = 0;
	int nodes_limit = PAGE_SIZE/sizeof(struct dio_cluster *);

	/* If count of nodes lower than a limit no evicting required */
	if (dev->nr_nodes <= dev->nr_max_clus)
		return;

	/* Alloc page that will contains set of pointers to disk map nodes
	 * that are sorted by it's age. So we cant sort more than
	 * PAGE_SIZE/sizeof(void *) nodes.
	 */
	page = alloc_page(GFP_NOIO);
	if (!page) {
		KLOG(KL_ERR, "no memory");
		return;
	}

	nodes = kmap(page);

	/* Lock disk map age mutex, so timer work will not updates age's
	 * while sorting nodes
	 */
	mutex_lock(&dev->age_mutex);
	for (;;) {

		/* Snapshot piece of nodes tree, increase
		 * search start index - first_index to get next piece
		 * next time
		 */
		spin_lock_irq(&dev->clus_lock);
		nr_found = radix_tree_gang_lookup(&dev->clus_root, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			atomic_inc(&node->ref);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock_irq(&dev->clus_lock);

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
				KLOG(KL_ERR, "nr_nodes=%d\
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
			struct dio_cluster *n;
			for (i = 0; i < nr_nodes; i++) {
				n = nodes[i];
				KLOG(KL_ERR, "node=%p, age=%lx, index=%lu",
						n, n->age, n->index);
			}
			BUG_ON(1);
		}
		prev = node;
		/* Evict nodes from disk map with lower age */
		if (dev->nr_clus > dev->nr_max_clus) {
			removed = dio_clu_remove(dev, node->index);
			BUG_ON(removed != node);
			dio_clu_deref(node);
		}
	}

release:
	mutex_unlock(&map->age_mutex);
	for (index = 0; index < nr_nodes; index++) {
		node = nodes[index];
		dio_clu_deref(node);
	}

	kunmap(page);
	put_page(page);
}

/* Dump disk map nodes info by printk. For debug. */
static void dio_clus_dump(struct amap *map)
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
			KLOG(KL_INF, "n=%p i=%lu age=%lx refs=%d",
					node, node->index, node->age, atomic_read(&node->refs));
			amap_node_deref(node);
		}
	}
}

/* Compact disk map by evicting dirties LRU nodes */
static void dio_clus_shrink(struct dio_dev *dev)
{
	for (;;) {
		if (dev->nr_nodes > dev->nr_max_nodes) {
			dio_clus_lru_frees(map);
		} else {
			break;
		}
	}
}

/* Find and reference map node by it's index */
static int dio_clu_get(struct dio_dev *dev, unsigned long index,
		struct dio_cluster **pnode)
{
	struct dio_cluster *node;
	int err;

	*pnode = NULL;
	node = dio_clu_lookup(map, index);
	if (node) {
		*pnode = node;
		return 0;
	}

	node = dio_clu_alloc(dev);
	if (!node) {
		KLOG(KL_ERR, "cant alloc map node");
		return -ENOMEM;
	}
	node->index = index;

	err = dio_clu_insert(dev, node);
	if (err) {
		dio_clu_deref(node);
		KLOG(KL_ERR, "cant insert dev clu node, err=%d", err);
		return err;
	}
	atomic_inc(&node->ref);
	*pnode = node;
	return 0;
}

static void dio_clu_end_bio(struct bio *bio, int err)
{
	struct dio_cluster *cluster = bio->bi_private;
 
	dio_clu_deref(cluster);
	bio_put(bio);
}

static int dio_clu_submit_io(int rw, struct dio_cluster *cluster, unsigned long bio_flags)
{
	struct bio *bio;
	int i;
	int err = 0;

	bio = bio_alloc(GFP_NOIO, cluster->nr_pages);
	if (!bio)
		return -ENOMEM;

	bio->bi_iter.bi_sector = cluster->index*(cluster->clu_size >> 9);
	bio->bi_bdev = cluster->dev->bdev;

	for (i = 0; i < cluster->nr_pages; i++) {
		bio->bi_io_vec[i].bv_page = cluster->pages[i];
		bio->bi_io_vec[i].bv_len = PAGE_SIZE;
		bio->bi_io_vec[i].bv_offset = 0;
	}

	bio->bi_vcnt = cluster->nr_pages;
	bio->bi_iter.bi_size = cluster->clu_size;

	bio->bi_end_io = dio_clu_end_bio;
	bio->bi_private = cluster;
	bio->bi_flags |= bio_flags;

	atomic_inc(&cluster->ref);
	bio->bi_private = cluster;

	bio_get(bio);
	submit_bio(rw, bio);
	bio_put(bio);

	return err;
}

struct dio_cluster *dio_clu_read(struct dio_dev *dev, u64 index)
{
	int err;
	struct dio_cluster *cluster;

	err = dio_clu_get(dev, index, &cluster);
	if (err)
		return NULL;
	dio_clu_submit_io(READ, 
}

int dio_clu_write(struct dio_cluster *cluster)
{

}

int dio_clu_sync(struct dio_cluster *cluster)
{

}


static void dio_clu_free(struct dio_cluster *cluster)
{
	u32 i;
	for (i = 0; i < cluster->nr_pages; i++)
		put_page(cluster->pages[i]);

	kmem_cache_free(dio_clu_cachep, cluster);
}

static void dio_clu_release(struct dio_cluster *cluster)
{
	set_bit(DIO_CLU_RELS, &cluster->flags);	
	if (test_bit(DIO_CLU_DIRTY, &cluster->flags))
		dio_clu_sync(cluster);
	dio_clu_free(cluster);
}

void dio_clu_ref(struct dio_cluster *cluster)
{
	atomic_ref(&cluster->ref);
}

void dio_clu_deref(struct dio_cluster *cluster)
{
	if (atomic_dec_and_test(&cluster->ref))
		dio_clu_release(cluster);
}
