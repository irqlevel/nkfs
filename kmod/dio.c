#include <inc/ds_priv.h>

static struct kmem_cache *dio_dev_cachep;
static struct kmem_cache *dio_clu_cachep;
static struct kmem_cache *dio_io_cachep;

static DEFINE_MUTEX(dio_dev_list_lock);
static LIST_HEAD(dio_dev_list);

#define __SUBCOMPONENT__ "dio"

#define DIO_TIMER_TIMEOUT_MSECS 1000

static struct timer_list dio_timer;
static struct workqueue_struct *dio_wq;

static void dio_clu_ref(struct dio_cluster *cluster);
static void dio_clu_deref(struct dio_cluster *cluster);

static void dio_pages_zero(struct dio_pages *pages)
{
	memset(pages, 0, sizeof(*pages));
}

static void dio_pages_sum(struct dio_pages *pages, struct csum *sum)
{
	struct csum_ctx ctx;
	int i;

	csum_reset(&ctx);

	for (i = 0; i < pages->nr_pages; i++)
		csum_update(&ctx, page_address(pages->pages[i]), PAGE_SIZE);

	csum_digest(&ctx, sum);
}

static void
dio_pages_io(struct dio_pages *pages, void *buf,
		unsigned long off, unsigned long len, int write)
{
	unsigned long read = 0, step;
	unsigned long pg_off;
	unsigned long pg_idx;

	BUG_ON((off + len) > (pages->nr_pages*PAGE_SIZE));

	pg_idx = off >> PAGE_SHIFT;
	pg_off = off & (PAGE_SIZE - 1);
	while (read < len) {
		BUG_ON(pg_idx >= pages->nr_pages);
		step = len - read;
		if (step > (PAGE_SIZE - pg_off)) 
			step = PAGE_SIZE - pg_off;

		if (!write) 
			memcpy(
			(char *)buf + read,
			(char *)page_address(pages->pages[pg_idx]) + pg_off,
			step);
		else
			memcpy(
			(char *)page_address(pages->pages[pg_idx]) + pg_off,
			(char *)buf + read,
			step);

		read+= step;
		pg_off = 0;
		pg_idx+= 1;
	}
}

static int dio_pages_alloc(struct dio_pages *buf, int nr_pages)
{
	int i;

	memset(buf, 0, sizeof(*buf));
	if (nr_pages <= 0 || nr_pages > ARRAY_SIZE(buf->pages))
		return -EINVAL;

	for (i = 0; i < nr_pages; i++) {
		buf->pages[i] = alloc_page(GFP_KERNEL);
		if (!buf->pages[i]) {
			int j;
			for (j = 0; j < i; j++)
				put_page(buf->pages[j]);
			dio_pages_zero(buf);
			return -ENOMEM;
		}
	}
	buf->nr_pages = nr_pages;
	return 0;
}

static void dio_pages_free(struct dio_pages *buf)
{
	int i;

	for (i = 0; i < buf->nr_pages; i++)
		put_page(buf->pages[i]);

	dio_pages_zero(buf);
}

static int dio_clu_pinned(struct dio_cluster *cluster)
{
	return (atomic_read(&cluster->pin_count)) ? 1 : 0;
}

static void dio_clu_pin(struct dio_cluster *cluster)
{
	atomic_inc(&cluster->pin_count);
}

static void dio_clu_unpin(struct dio_cluster *cluster)
{
	atomic_dec(&cluster->pin_count);
}

static void dio_dev_init(struct dio_dev *dev,
	struct block_device *bdev, unsigned long clu_size,
	unsigned long nr_max_clus)
{
	memset(dev, 0, sizeof(*dev));
	atomic_set(&dev->ref, 1);
	spin_lock_init(&dev->clus_lock);
	init_rwsem(&dev->age_rw_lock);

	INIT_RADIX_TREE(&dev->clus_root, GFP_NOIO);

	dev->nr_clus = 0;
	dev->nr_max_clus = nr_max_clus;
	dev->clu_size = clu_size;
	dev->bdev = bdev;

	mutex_lock(&dio_dev_list_lock);
	list_add_tail(&dev->list, &dio_dev_list);
	mutex_unlock(&dio_dev_list_lock);
}

struct dio_dev *dio_dev_create(struct block_device *bdev,
	int clu_size, int nr_max_clus)
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

static struct dio_cluster *dio_clu_alloc(struct dio_dev *dev)
{
	struct dio_cluster *cluster;
	u32 nr_pages;
	int err;

	BUG_ON(dev->clu_size & (PAGE_SIZE - 1));
	BUG_ON(dev->clu_size < PAGE_SIZE);
	BUG_ON(dev->clu_size > (DIO_CLU_MAX_PAGES * PAGE_SIZE));

	nr_pages = dev->clu_size >> PAGE_SHIFT;
	cluster = kmem_cache_alloc(dio_clu_cachep, GFP_NOIO);
	if (!cluster)
		return NULL;

	memset(cluster, 0, sizeof(*cluster));
	init_rwsem(&cluster->sync_rw_lock);
	init_rwsem(&cluster->rw_lock);

	set_bit(DIO_CLU_INV, &cluster->flags);
	err = dio_pages_alloc(&cluster->pages, nr_pages);
	if (err) {
		kmem_cache_free(dio_clu_cachep, cluster);
		return NULL;
	}
	
	atomic_set(&cluster->pin_count, 0);
	atomic_set(&cluster->ref, 1);
	init_completion(&cluster->read_comp);
	cluster->dev = dev;
	cluster->clu_size = dev->clu_size;

	return cluster;
}

static struct 
dio_cluster *dio_clu_lookup_create(struct dio_dev *dev, unsigned long index)
{
	struct dio_cluster *cluster;

	spin_lock_irq(&dev->clus_lock);
	cluster = radix_tree_lookup(&dev->clus_root, index);
	if (cluster) {
		dio_clu_ref(cluster);
		dio_clu_pin(cluster);
	}
	spin_unlock_irq(&dev->clus_lock);
	if (!cluster) {
		struct dio_cluster *new;
		new = dio_clu_alloc(dev);
		if (!new)
			return NULL;
		new->index = index;
		if (radix_tree_preload(GFP_NOIO)) {
			dio_clu_deref(new);	
			return NULL;
		}

		spin_lock_irq(&dev->clus_lock);
		if (radix_tree_insert(&dev->clus_root, new->index, new)) {
			cluster = radix_tree_lookup(&dev->clus_root, index);
		} else {
			dio_clu_ref(new);
			cluster = new;
			dev->nr_clus++;
		}

		if (cluster) {
			dio_clu_ref(cluster);
			dio_clu_pin(cluster);
		}
		spin_unlock_irq(&dev->clus_lock);
	
		radix_tree_preload_end();
	
		if (cluster != new)
			dio_clu_deref(new);
	}

	if (cluster) {
		down_read(&dev->age_rw_lock);
		cluster->age |= (1ull << 63);
		up_read(&dev->age_rw_lock);
	}

	return cluster;
}

static struct
dio_cluster *dio_clu_remove(struct dio_dev *dev, unsigned long index)
{
	struct dio_cluster *cluster;

	spin_lock_irq(&dev->clus_lock);

	cluster = radix_tree_lookup(&dev->clus_root, index);
	if (cluster && !dio_clu_pinned(cluster)) {
		cluster = radix_tree_delete(&dev->clus_root, index);
		if (cluster) {
			BUG_ON(cluster->index != index);
			dev->nr_clus--;
		}
	} else 
		cluster = NULL;

	spin_unlock_irq(&dev->clus_lock);

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
			dio_clu_ref(cluster);
		}
		spin_unlock(&dev->clus_lock);
		if (nr_found == 0)
			break;

		for (index = 0; index < nr_found; index++) {
			cluster = batch[index];
			atomic_set(&cluster->pin_count, 0);
			removed = dio_clu_remove(dev, cluster->index);
			BUG_ON(removed != cluster);
			dio_clu_sync(cluster);
			dio_clu_deref(cluster); /* was in tree */
			dio_clu_deref(cluster); /* by alloc */
			dio_clu_deref(cluster); /* by batch */
		}
	}

	BUG_ON(dev->nr_clus);
}

static void dio_clus_dump(struct dio_dev *dev)
{
	struct dio_cluster *batch[16];
	int nr_found;
	unsigned long index, first_index = 0;
	struct dio_cluster *node;

	for (;;) {
		spin_lock(&dev->clus_lock);
		nr_found = radix_tree_gang_lookup(&dev->clus_root, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			dio_clu_ref(node);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock(&dev->clus_lock);
		if (nr_found == 0) {
			break;
		}

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			KLOG(KL_DBG3, "n=%p i=%lu age=%lx ref=%d",
				node, node->index,
				node->age, atomic_read(&node->ref));
			dio_clu_deref(node);
		}
	}
}

static void dio_dev_release(struct dio_dev *dev)
{

	mutex_lock(&dio_dev_list_lock);
	list_del(&dev->list);
	mutex_unlock(&dio_dev_list_lock);

	dio_clus_dump(dev);
	dio_clus_release(dev);
	kmem_cache_free(dio_dev_cachep, dev);
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

static int dio_clus_lru_frees(struct dio_dev *dev)
{
	struct dio_cluster *batch[16];
	int nr_found;
	unsigned long index, first_index = 0;
	struct dio_cluster *node, *removed;
	struct dio_cluster **nodes;
	struct page *page;
	int nr_nodes = 0;
	int nodes_limit = PAGE_SIZE/sizeof(struct dio_cluster *);

	if (dev->nr_clus <= dev->nr_max_clus)
		return 0;

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		KLOG(KL_ERR, "no memory");
		return -ENOMEM;
	}

	nodes = page_address(page);

	while (nr_nodes < nodes_limit) {

		spin_lock_irq(&dev->clus_lock);
		nr_found = radix_tree_gang_lookup(&dev->clus_root,
				(void **)batch, first_index,
				ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			dio_clu_ref(node);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock_irq(&dev->clus_lock);

		if (nr_found == 0)
			break;

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			if (!dio_clu_pinned(node) && (nr_nodes < nodes_limit))
				nodes[nr_nodes++] = node;
			else
				dio_clu_deref(node);
		}
	}

	/* Lock clus->age modification */
	down_write(&dev->age_rw_lock);
	sort(nodes, nr_nodes, sizeof(struct dio_cluster *),
			dio_clu_age_cmp, dio_clu_ptr_swap);

	for (index = 0; index < nr_nodes; index++) {
		node = nodes[index];
		if ((dev->nr_clus > dev->nr_max_clus)) {
			removed = dio_clu_remove(dev, node->index);
			if (removed) {
				BUG_ON(removed != node);
				dio_clu_sync(node);
				KLOG(KL_DBG3, "evicted c=%p a=%lx i=%lu r=%lu",
					node, node->age, node->index,
					atomic_read(&node->ref));
				dio_clu_deref(node);
				dio_clu_deref(node);
			}
		}
	}
	up_write(&dev->age_rw_lock);

	for (index = 0; index < nr_nodes; index++) {
		node = nodes[index];
		dio_clu_deref(node);
	}
	put_page(page);
	return 0;
}

static void dio_clus_shrink(struct dio_dev *dev)
{
	for (;;) {
		if (dev->nr_clus > dev->nr_max_clus) {
			dio_clus_lru_frees(dev);
		} else {
			break;
		}
	}
}

static struct dio_cluster * __dio_clu_get(struct dio_dev *dev, u64 index)
{
	return dio_clu_lookup_create(dev, index);
}

void dio_clu_put(struct dio_cluster *cluster)
{
	dio_clu_unpin(cluster);
	dio_clu_deref(cluster);
}

static void dio_io_release(struct dio_io *io)
{
	if (io->bio)
		bio_put(io->bio);

	if (io->cluster)
		dio_clu_deref(io->cluster);

	kmem_cache_free(dio_io_cachep, io);
}

static void dio_io_deref(struct dio_io *io)
{
	if (atomic_dec_and_test(&io->ref))
		dio_io_release(io);
}


static void dio_io_end_bio(struct bio *bio, int err)
{
	struct dio_io *io = bio->bi_private;

	BUG_ON(io->bio != bio);
 
	io->err = err;
	if (err) {
		KLOG(KL_ERR, "err %d rw %x io %p bio %p sec %llx size %x",
			err, io->rw, io, io->bio, BIO_BI_SECTOR(io->bio),
			BIO_BI_SIZE(io->bio));
	} else {
		KLOG(KL_DBG3, "err %d rw %x io %p bio %p sec %llx size %x",
			err, io->rw, io, io->bio, BIO_BI_SECTOR(io->bio),
			BIO_BI_SIZE(io->bio));
	}

	if (!(io->rw & REQ_WRITE)) { /*it was read */
		if (!err) {
			BUG_ON(test_bit(DIO_CLU_READ, &io->cluster->flags));
			set_bit(DIO_CLU_READ, &io->cluster->flags);
		}
	}

	if (test_bit(DIO_IO_WAIT, &io->flags))
		complete(&io->comp);
	else
		dio_io_deref(io);
}

static struct bio *dio_io_alloc_bio(struct dio_io *io)
{
	struct bio *bio;
	int i;

	bio = bio_alloc(GFP_NOIO, io->cluster->pages.nr_pages);
	if (!bio)
		return NULL;

	BIO_BI_SECTOR(bio) = io->cluster->index*(io->cluster->clu_size >> 9);
	bio->bi_bdev = io->cluster->dev->bdev;

	for (i = 0; i < io->cluster->pages.nr_pages; i++) {
		bio->bi_io_vec[i].bv_page = io->cluster->pages.pages[i];
		bio->bi_io_vec[i].bv_len = PAGE_SIZE;
		bio->bi_io_vec[i].bv_offset = 0;
	}

	bio->bi_vcnt = io->cluster->pages.nr_pages;
	BIO_BI_SIZE(bio) = io->cluster->pages.nr_pages*PAGE_SIZE;

	bio->bi_end_io = dio_io_end_bio;
	bio->bi_private = io;
	bio->bi_flags |= io->bio_flags;

	return bio;
}

struct dio_io *dio_io_alloc(struct dio_cluster *cluster)
{
	struct dio_io *io;

	io = kmem_cache_alloc(dio_io_cachep, GFP_NOIO);
	if (!io)
		return NULL;

	memset(io, 0, sizeof(*io));
	atomic_set(&io->ref, 1);
	INIT_LIST_HEAD(&io->list);
	init_completion(&io->comp);

	dio_clu_ref(cluster);
	io->cluster = cluster;

	io->bio = dio_io_alloc_bio(io);
	if (!io->bio) {
		dio_io_deref(io);
		return NULL;
	}

	return io;
}

static void dio_submit(unsigned long rw, struct dio_io *io)
{
	io->rw |= rw;

	KLOG(KL_DBG3, "rw %x io %p bio %p sec %llx size %x",
			io->rw, io, io->bio, BIO_BI_SECTOR(io->bio),
			BIO_BI_SIZE(io->bio));
	submit_bio(io->rw, io->bio);
}

static int dio_clu_wait_read(struct dio_cluster *cluster)
{
	struct dio_io *io;
	int err;

	if (!test_and_set_bit(DIO_CLU_READ_START, &cluster->flags)) {
		io = dio_io_alloc(cluster);
		if (!io) {
			err = -ENOMEM;
			goto read_comp;
		}

		set_bit(DIO_IO_WAIT, &io->flags); 
		dio_submit(READ, io);
		wait_for_completion(&io->comp);
		err = io->err;
		BUG_ON(!err && !test_bit(DIO_CLU_READ, &cluster->flags));
		dio_io_deref(io);
read_comp:
		cluster->err = err;
		complete(&cluster->read_comp);
	} else {
		err = cluster->err;
		wait_for_completion(&cluster->read_comp);
	}

	return err;
}

void dio_clu_write_lock(struct dio_cluster *cluster)
{
	down_write(&cluster->rw_lock);
}

void dio_clu_write_unlock(struct dio_cluster *cluster)
{
	up_write(&cluster->rw_lock);
}

void dio_clu_read_lock(struct dio_cluster *cluster)
{
	down_read(&cluster->rw_lock);
}

void dio_clu_read_unlock(struct dio_cluster *cluster)
{
	up_read(&cluster->rw_lock);
}

int dio_clu_read(struct dio_cluster *cluster,
	void *buf, unsigned long len, unsigned long off)
{
	int err;

	BUG_ON((off + len) > cluster->clu_size);

	if (!test_bit(DIO_CLU_READ, &cluster->flags)) {
		err = dio_clu_wait_read(cluster);
		if (err)
			goto out;
	}

	down_read(&cluster->rw_lock);
	dio_pages_io(&cluster->pages, buf, off, len, 0);
	up_read(&cluster->rw_lock);

	err = 0;
out:	
	return err;
}

struct dio_cluster * dio_clu_get(struct dio_dev *dev, u64 index)
{
	struct dio_cluster *clu;

	clu  = __dio_clu_get(dev, index);
	if (!clu)
		return NULL;

	if (!test_bit(DIO_CLU_READ, &clu->flags)) {
		int err;
		err = dio_clu_wait_read(clu);
		if (err) {
			dio_clu_put(clu);
			clu = NULL;
		}
	}

	return clu;
}

char *dio_clu_map(struct dio_cluster *cluster, unsigned long off)
{
	unsigned long pg_idx;
	unsigned long pg_off;

	BUG_ON(off > cluster->clu_size);
	pg_idx = off >> PAGE_SHIFT;
	pg_off = off & (PAGE_SIZE - 1);
	BUG_ON(pg_idx >= cluster->pages.nr_pages);
	return ((char *)page_address(cluster->pages.pages[pg_idx]) + pg_off);
}

int dio_clu_write(struct dio_cluster *cluster,
	void *buf, unsigned long len, unsigned long off)
{
	int err;

	down_read(&cluster->sync_rw_lock);

	BUG_ON((off + len) > cluster->clu_size);
	if (!test_bit(DIO_CLU_READ, &cluster->flags)) {
		err = dio_clu_wait_read(cluster);
		if (err)
			goto out;
	}

	BUG_ON(!test_bit(DIO_CLU_READ, &cluster->flags));
	
	down_write(&cluster->rw_lock);
	set_bit(DIO_CLU_DIRTY, &cluster->flags);
	dio_pages_io(&cluster->pages, buf, off, len, 1);
	up_write(&cluster->rw_lock);

	err = 0;

out:
	up_read(&cluster->sync_rw_lock);

	return err;
}

int dio_clu_zero(struct dio_cluster *cluster)
{
	int err;
	struct page *page;
	int i;
	u32 off;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	memset(page_address(page), 0, PAGE_SIZE);
	off = 0;
	for (i = 0; i < cluster->pages.nr_pages; i++) {
		err = dio_clu_write(cluster, page_address(page),
			PAGE_SIZE, off);
		if (err)
			goto out;
		off+= PAGE_SIZE;
	}
	err = 0;

out:
	put_page(page);
	return err;
}

void dio_clu_set_dirty(struct dio_cluster *cluster)
{
	down_read(&cluster->sync_rw_lock);
	set_bit(DIO_CLU_DIRTY, &cluster->flags);
	up_read(&cluster->sync_rw_lock);
}

int dio_clu_sync(struct dio_cluster *cluster)
{
	int err;
	struct dio_io *io;

	if (!test_bit(DIO_CLU_DIRTY, &cluster->flags))
		return 0;
	
	down_write(&cluster->sync_rw_lock);
	if (!test_bit(DIO_CLU_DIRTY, &cluster->flags)) {
		err = 0;
		goto out;
	}

	io = dio_io_alloc(cluster);
	if (!io) {
		err = -ENOMEM;
		goto out;
	}

	set_bit(DIO_IO_WAIT, &io->flags); 
	dio_submit(WRITE, io);
	wait_for_completion(&io->comp);

	err = io->err;
	if (!err)
		clear_bit(DIO_CLU_DIRTY, &cluster->flags);

	dio_io_deref(io);
out:
	up_write(&cluster->sync_rw_lock);

	return err;
}

static void dio_clu_free(struct dio_cluster *cluster)
{
	dio_pages_free(&cluster->pages);
	kmem_cache_free(dio_clu_cachep, cluster);
}

static void dio_clu_release(struct dio_cluster *cluster)
{
	set_bit(DIO_CLU_RELS, &cluster->flags);	
	if (test_bit(DIO_CLU_DIRTY, &cluster->flags))
		dio_clu_sync(cluster);
	dio_clu_free(cluster);
}

static void dio_clu_ref(struct dio_cluster *cluster)
{
	BUG_ON(atomic_read(&cluster->ref) <= 0);
	atomic_inc(&cluster->ref);
}

static void dio_clu_deref(struct dio_cluster *cluster)
{
	BUG_ON(atomic_read(&cluster->ref) <= 0);
	if (atomic_dec_and_test(&cluster->ref))
		dio_clu_release(cluster);
}

void dio_dev_ref(struct dio_dev *dev)
{
	BUG_ON(atomic_read(&dev->ref) <= 0);
	atomic_inc(&dev->ref);
}

void dio_dev_deref(struct dio_dev *dev)
{
	BUG_ON(atomic_read(&dev->ref) <= 0);
	if (atomic_dec_and_test(&dev->ref))
		dio_dev_release(dev);
}

static void dio_clu_age(struct dio_cluster *cluster)
{
	down_read(&cluster->dev->age_rw_lock);
	cluster->age = cluster->age >> 1;
	up_read(&cluster->dev->age_rw_lock);
}

void dio_clu_sum(struct dio_cluster *cluster, struct csum *sum)
{
	dio_pages_sum(&cluster->pages, sum);
}

static void dio_clus_age(struct dio_dev *dev)
{
	struct dio_cluster *batch[16];
	int nr_found;
	unsigned long index, first_index = 0;
	struct dio_cluster *node;

	for (;;) {
		spin_lock(&dev->clus_lock);
		nr_found = radix_tree_gang_lookup(&dev->clus_root, (void **)batch,
				first_index, ARRAY_SIZE(batch));
		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			dio_clu_ref(node);
			if (node->index >= first_index)
				first_index = node->index + 1;
		}
		spin_unlock(&dev->clus_lock);
		if (nr_found == 0) {
			break;
		}

		for (index = 0; index < nr_found; index++) {
			node = batch[index];
			dio_clu_age(node);
			dio_clu_deref(node);
		}
	}
}

static void dio_clus_age_work(struct work_struct *work)
{
	struct dio_dev *dev, *next;

	mutex_lock(&dio_dev_list_lock);
	list_for_each_entry_safe(dev, next, &dio_dev_list, list) {
		dio_clus_age(dev);
	}
	mutex_unlock(&dio_dev_list_lock);
	kfree(work);
}

static void dio_clus_shrink_work(struct work_struct *work)
{
	struct dio_dev *dev, *next;

	mutex_lock(&dio_dev_list_lock);
	list_for_each_entry_safe(dev, next, &dio_dev_list, list) {
		dio_clus_shrink(dev);
	}
	mutex_unlock(&dio_dev_list_lock);
	kfree(work);
}

static int dio_queue_work(work_func_t func)
{
	struct work_struct *work = NULL;

	work = kmalloc(sizeof(struct work_struct), GFP_ATOMIC);
	if (!work) {
		KLOG(KL_ERR, "cant alloc work");
		return -ENOMEM;
	}

	memset(work, 0, sizeof(*work));
	INIT_WORK(work, func);
	if (!queue_work(dio_wq, work)) {
		kfree(work);
		KLOG(KL_ERR, "cant queue work");
		return -ENOMEM;
	}
	return 0;
}

static void dio_timer_callback(unsigned long data)
{
	dio_queue_work(dio_clus_age_work);
	dio_queue_work(dio_clus_shrink_work);

	mod_timer(&dio_timer,
			jiffies +
			msecs_to_jiffies(DIO_TIMER_TIMEOUT_MSECS));
}

int dio_init(void)
{
	int err;

	dio_dev_cachep = kmem_cache_create("dio_dev_cachep",
		sizeof(struct dio_dev), 0, SLAB_MEM_SPREAD, NULL);
	if (!dio_dev_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto fail;
	}

	dio_clu_cachep =  kmem_cache_create("dio_clu_cachep",
		sizeof(struct dio_cluster), 0, SLAB_MEM_SPREAD, NULL);
	if (!dio_clu_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto rel_dev_cache;
	}

	dio_io_cachep =  kmem_cache_create("dio_io_cachep",
		sizeof(struct dio_io), 0, SLAB_MEM_SPREAD, NULL);
	if (!dio_io_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto rel_clu_cache;
	}

	dio_wq = alloc_workqueue("dio_wq", WQ_UNBOUND, 1);
	if (!dio_wq) {
		KLOG(KL_ERR, "cant create wq");
		err = -ENOMEM;
		goto rel_io_cache;
	}

	setup_timer(&dio_timer, dio_timer_callback, 0);
	err = mod_timer(&dio_timer,
			jiffies +
			msecs_to_jiffies(DIO_TIMER_TIMEOUT_MSECS));
	if (err) {
		KLOG(KL_ERR, "mod_timer failed with err=%d", err);
		goto del_wq;
	}

	return 0;

del_wq:
	destroy_workqueue(dio_wq);
rel_io_cache:
	kmem_cache_destroy(dio_io_cachep);
rel_clu_cache:
	kmem_cache_destroy(dio_clu_cachep);
rel_dev_cache:
	kmem_cache_destroy(dio_dev_cachep);
fail:
	return err;
}

void dio_finit(void)
{
	del_timer_sync(&dio_timer);
	destroy_workqueue(dio_wq);
	kmem_cache_destroy(dio_io_cachep);
	kmem_cache_destroy(dio_clu_cachep);
	kmem_cache_destroy(dio_dev_cachep);
}
