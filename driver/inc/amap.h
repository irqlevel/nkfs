#pragma once

typedef void (*io_complete_t)(void *context,	int err, int rw, struct page *page, u64 off);
typedef void (*io_op_t)(void *io, int rw, struct page *page, u64 off, void *context, io_complete_t complete);

struct amap {

	/* Lock for updating .nodes tree */
	spinlock_t		nodes_lock;
	struct radix_tree_root	nodes;		/* Set of map nodes */
	spinlock_t		lru_list_lock;
	struct list_head	lru_list;

	int			nr_nodes;	/* Count of map nodes */
	int			nr_max_nodes;	/* Limit of map nodes */
	void			*io;		/* io provider */
	io_op_t			io_op;

	/* Link to global list of different disk maps */
	struct list_head	maps_list;
	atomic_t		refs;		/* Reference counter */

	/* Mutex to protect nodes ages update */
	struct mutex		age_mutex;
};

enum {
	AMAP_NODE_S_INVALID,
	AMAP_NODE_S_READY,
	AMAP_NODE_S_LOCKED
};

struct amap_req {
	void			*context;
	struct page		*page;
	struct list_head	req_list;
	u64			off;
	int			rw;
	int			err;
	io_complete_t		complete;
};

struct amap_node {
	struct list_head 	lru_list;	/* Link to disk map lru list */

	/* index = nodes data offset/nodes data size,
	 * index used as unique id of node to link and search inside amap
	 * radix_tree
	 */
	unsigned long		index;
	struct page 		*page;

	atomic_t		refs;		/* Reference counter */
	unsigned long		age;		/* Node's age */

	struct amap		*owner;		/* Link to parent disk map */
	spinlock_t		lock;
	int			state;
	struct	list_head	req_list;
	struct	amap_req	*req;
};

int amap_init(struct amap *map, void *io, int nr_max_nodes, io_op_t io_op);

void amap_release(struct amap *map);

int amap_read(struct amap *map,	struct page *page, u64 off, void *context, io_complete_t complete);
int amap_write(struct amap *map, struct page *page, u64 off, void *context, io_complete_t complete);

/* Init/release of disk map subsystem */
int amap_sys_init(void);
void amap_sys_release(void);
