#ifndef __NKFS_DIO_H__
#define __NKFS_DIO_H__

struct dio_dev {
	atomic_t		ref;
	spinlock_t		clus_lock;
	struct radix_tree_root	clus_root;	/* Set of map nodes */

	unsigned long		clu_size;
	unsigned long		nr_clus;	/* Count of map nodes */
	unsigned long		nr_max_clus;	/* Limit of map nodes */

	/* Link to global list of different disk maps */
	struct list_head	list;
	/* Mutex to protect nodes ages update */
	struct rw_semaphore	age_rw_lock;
	struct block_device	*bdev;
};

enum {
	DIO_CLU_INV,
	DIO_CLU_DIRTY,
	DIO_CLU_READ,
	DIO_CLU_READ_START,
	DIO_CLU_RELS,
};

#define DIO_CLU_MAX_PAGES 16

struct dio_pages {
	struct page	*pages[DIO_CLU_MAX_PAGES];
	int		nr_pages;
};

struct dio_cluster {
	atomic_t		ref;
	atomic_t		pin_count;
	struct dio_dev		*dev;
	u64			index;
	unsigned long		flags;
	struct dio_pages	pages;
	int			clu_size;
	struct rw_semaphore	sync_rw_lock;
	struct rw_semaphore	rw_lock;
	u64			age;
	struct completion	read_comp;
	int			err;
};

enum {
	DIO_IO_WAIT,
};

struct dio_io {
	atomic_t		ref;
	struct bio		*bio;
	struct dio_cluster	*cluster;
	struct list_head	list;
	unsigned long		flags;
	unsigned long		rw;
	unsigned long		bio_flags;
	struct completion	comp;
	int			err;
};

int dio_init(void);
void dio_finit(void);

struct dio_cluster *dio_clu_get(struct dio_dev *dev, u64 index);

void dio_clu_put(struct dio_cluster *cluster);

int dio_clu_read(struct dio_cluster *cluster,
	void *buf, unsigned long len, unsigned long off);

int dio_clu_write(struct dio_cluster *cluster,
	void *buf, unsigned long len, unsigned long off);

char *dio_clu_map(struct dio_cluster *cluster, unsigned long off);

int dio_clu_zero(struct dio_cluster *cluster);

int dio_clu_sync(struct dio_cluster *cluster);

void dio_clu_write_lock(struct dio_cluster *cluster);

void dio_clu_write_unlock(struct dio_cluster *cluster);

void dio_clu_read_lock(struct dio_cluster *cluster);

void dio_clu_read_unlock(struct dio_cluster *cluster);

void dio_dev_ref(struct dio_dev *dev);
void dio_dev_deref(struct dio_dev *dev);

void dio_clu_set_dirty(struct dio_cluster *cluster);

void dio_clu_sum(struct dio_cluster *cluster, struct csum *sum);

struct dio_dev *dio_dev_create(struct block_device *bdev,
	int clu_size, int nr_max_clus);

#endif
