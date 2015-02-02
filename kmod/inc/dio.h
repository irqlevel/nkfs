#pragma once

struct dio_dev {
	atomic_t		ref;
	spinlock_t		clus_lock;
	struct radix_tree_root	clus_root;	/* Set of map nodes */

	u32			clu_size;
	u32			nr_clus;	/* Count of map nodes */
	u32			nr_max_clus;	/* Limit of map nodes */

	/* Link to global list of different disk maps */
	struct list_head	list;
	/* Mutex to protect nodes ages update */
	struct mutex		age_mutex;
	struct bdev		*bdev;
};

enum {
	DIO_CLU_INV,
	DIO_CLU_DIRTY,
	DIO_CLU_READ,
	DIO_CLU_RELS,
};

#define DIO_CLU_MAX_PAGES 16

struct dio_cluster {
	atomic_t	ref;
	struct dio_dev 	*dev;
	u64		index;
	unsigned long	flags;	
	struct page	pages[DIO_CLU_MAX_PAGES];
	u32		nr_pages;
	u32		clu_size;
	u64		age;
};
