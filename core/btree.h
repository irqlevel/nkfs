#ifndef __NKFS_BTREE_H__
#define __NKFS_BTREE_H__

#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/atomic.h>

#include <include/nkfs_image.h>

#pragma pack(push, 1)

struct nkfs_btree;

struct nkfs_btree_node {
	u32			sig1;
	u32			t;
	u64			block;
	struct nkfs_btree	*tree;
	atomic_t		ref;
	struct rb_node		nodes_link;
	u32			leaf;
	u32			nr_keys;
	struct page		*header;
	struct page		*keys[NKFS_BTREE_KEY_PAGES];
	struct page		*values[NKFS_BTREE_VALUE_PAGES];
	struct page		*children[NKFS_BTREE_CHILD_PAGES];
	u32			sig2;
};

struct nkfs_btree {
	struct nkfs_btree_node	*root;
	struct nkfs_sb		*sb;
	struct rw_semaphore	rw_lock;
	rwlock_t		nodes_lock;
	struct rb_root		nodes;
	atomic_t		ref;
	int			releasing;
	u32			nodes_active;
	u32			sig1;
};

struct nkfs_btree_info {
	u64 nr_keys;
	u64 nr_nodes;
};

#pragma pack(pop)

struct nkfs_btree *nkfs_btree_create(struct nkfs_sb *sb, u64 begin);

u64 nkfs_btree_root_block(struct nkfs_btree *tree);

void nkfs_btree_ref(struct nkfs_btree *tree);
void nkfs_btree_deref(struct nkfs_btree *tree);

int nkfs_btree_insert_key(struct nkfs_btree *tree, struct nkfs_btree_key *key,
	struct nkfs_btree_value *value, int replace);

int nkfs_btree_find_key(struct nkfs_btree *tree,
	struct nkfs_btree_key *key,
	struct nkfs_btree_value *pvalue);

int nkfs_btree_delete_key(struct nkfs_btree *tree,
	struct nkfs_btree_key *key);

void nkfs_btree_stop(struct nkfs_btree *tree);

void nkfs_btree_read_lock(struct nkfs_btree *tree);
void nkfs_btree_read_unlock(struct nkfs_btree *tree);

void nkfs_btree_write_lock(struct nkfs_btree *tree);
void nkfs_btree_write_unlock(struct nkfs_btree *tree);

typedef void (*nkfs_btree_key_erase_clb_t)(struct nkfs_btree_key *key,
					   struct nkfs_btree_value *value,
					   void *ctx);

void nkfs_btree_erase(struct nkfs_btree *tree,
	nkfs_btree_key_erase_clb_t key_erase_clb,
	void *ctx);

void nkfs_btree_value_by_u64(u64 val, struct nkfs_btree_value *value);
u64 nkfs_btree_value_to_u64(struct nkfs_btree_value *value);

void nkfs_btree_key_by_u64(u64 val, struct nkfs_btree_key *key);
u64 nkfs_btree_key_to_u64(struct nkfs_btree_key *key);

char *nkfs_btree_key_hex(struct nkfs_btree_key *key);
char *nkfs_btree_value_hex(struct nkfs_btree_value *value);

typedef int (*nkfs_btree_enum_clb_t)(void *ctx, struct nkfs_btree_node *node,
				     int index);
int nkfs_btree_enum_tree(struct nkfs_btree *tree, nkfs_btree_enum_clb_t clb,
			 void *ctx);

int nkfs_btree_node_delete_key(struct nkfs_btree_node *first,
		struct nkfs_btree_key *key);

struct nkfs_btree_key *nkfs_btree_node_key(struct nkfs_btree_node *node,
					   int index);
struct nkfs_btree_value *nkfs_btree_node_value(struct nkfs_btree_node *node,
					       int index);

void nkfs_btree_log(struct nkfs_btree *tree, int llevel);

void nkfs_btree_stats(struct nkfs_btree *tree, struct nkfs_btree_info *info);

int nkfs_btree_check(struct nkfs_btree *tree);

int nkfs_btree_test(int num_keys);

int nkfs_btree_init(void);
void nkfs_btree_finit(void);

int nkfs_btree_node_write(struct nkfs_btree_node *node);

void nkfs_btree_node_ref(struct nkfs_btree_node *node);
void nkfs_btree_node_deref(struct nkfs_btree_node *node);

#define NKFS_BTREE_NODE_REF(n)						\
{									\
	nkfs_btree_node_ref((n));					\
	KLOG(KL_DBG3, "NREF %p now %d", (n), atomic_read(&(n)->ref));	\
}

#define NKFS_BTREE_NODE_DEREF(n)					\
{									\
	KLOG(KL_DBG3, "NDEREF %p was %d", (n), atomic_read(&(n)->ref));	\
	nkfs_btree_node_deref((n));					\
}

#endif
