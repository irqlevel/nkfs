#pragma once

struct dio_dev {
	atomic_t		ref;
	spinlock_t		clus_lock;
	struct radix_tree_root	clus_root;	/* Set of map nodes */

	int			clu_size;
	int			nr_clus;	/* Count of map nodes */
	int			nr_max_clus;	/* Limit of map nodes */

	/* Link to global list of different disk maps */
	struct list_head	list;
	/* Mutex to protect nodes ages update */
	struct mutex		age_mutex;
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
	struct dio_dev 		*dev;
	u64			index;
	unsigned long		flags;
	struct dio_pages	pages;
	int			clu_size;
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
	struct dio_pages	pages;
};

int dio_init(void);
void dio_finit(void);

struct dio_cluster * dio_clu_get(struct dio_dev *dev, u64 index);

void dio_clu_put(struct dio_cluster *cluster);

int dio_clu_read(struct dio_cluster *cluster,
	void *buf, u32 len, u32 off);

int dio_clu_write(struct dio_cluster *cluster,
	void *buf, u32 len, u32 off);

int dio_clu_zero(struct dio_cluster *cluster);

int dio_clu_sync(struct dio_cluster *cluster);

void dio_dev_ref(struct dio_dev *dev);
void dio_dev_deref(struct dio_dev *dev);

struct dio_dev *dio_dev_create(struct block_device *bdev,
	int clu_size, int nr_max_clus);
