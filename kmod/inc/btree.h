#pragma once

#include <linux/types.h>

#pragma pack(push, 1)

struct btree;

struct btree_node {
	u32			sig1;
	u32			t;
	u64			block;
	struct btree		*tree;
	atomic_t		ref;
	struct rb_node		nodes_link;
	u32			leaf;
	u32			nr_keys;
	struct page		*header;
	struct page		*keys[BTREE_KEY_PAGES];
	struct page		*values[BTREE_VALUE_PAGES];
	struct page		*childs[BTREE_CHILD_PAGES];
	u32			sig2;
};

struct btree {
	struct btree_node	*root;
	struct ds_sb		*sb;
	struct rw_semaphore	rw_lock;
	rwlock_t		nodes_lock;
	struct rb_root		nodes;
	atomic_t		ref;
	int			releasing;
	u32			nodes_active;
	u32			sig1;	
};

struct btree_info {
	u64 nr_keys;
	u64 nr_nodes;
};

#pragma pack(pop)

struct btree *btree_create(struct ds_sb *sb, u64 begin);

u64 btree_root_block(struct btree *tree);

void btree_ref(struct btree *tree);
void btree_deref(struct btree *tree);

int btree_insert_key(struct btree *tree, struct btree_key *key,
	struct btree_value *value, int replace);

int btree_find_key(struct btree *tree,
	struct btree_key *key,
	struct btree_value *pvalue);

int btree_delete_key(struct btree *tree,
	struct btree_key *key);

void btree_stop(struct btree *tree);

void btree_read_lock(struct btree *tree);
void btree_read_unlock(struct btree *tree);

void btree_write_lock(struct btree *tree);
void btree_write_unlock(struct btree *tree);

typedef void (*btree_key_erase_clb_t)(struct btree_key *key, struct btree_value *value,
	void *ctx);

void btree_erase(struct btree *tree,
	btree_key_erase_clb_t key_erase_clb,
	void *ctx);

void btree_value_by_u64(u64 val, struct btree_value *value);
u64 btree_value_to_u64(struct btree_value *value);

void btree_key_by_u64(u64 val, struct btree_key *key);
u64 btree_key_to_u64(struct btree_key *key);

char *btree_key_hex(struct btree_key *key);
char *btree_value_hex(struct btree_value *value);

typedef int (*btree_enum_clb_t)(void *ctx, struct btree_node *node, int index);
int btree_enum_tree(struct btree *tree, btree_enum_clb_t clb, void *ctx);

int btree_node_delete_key(struct btree_node *first,
		struct btree_key *key);

struct btree_key *btree_node_key(struct btree_node *node, int index);
struct btree_value *btree_node_value(struct btree_node *node, int index);

void btree_log(struct btree *tree, int llevel);

void btree_stats(struct btree *tree, struct btree_info *info);

int btree_check(struct btree *tree);

int btree_test(int num_keys);

int btree_init(void);
void btree_finit(void);

int btree_node_write(struct btree_node *node);

void btree_node_ref(struct btree_node *node);
void btree_node_deref(struct btree_node *node);

#define BTREE_NODE_REF(n)						\
{									\
	btree_node_ref((n));						\
	KLOG(KL_DBG3, "NREF %p now %d", (n), atomic_read(&(n)->ref));	\
}

#define BTREE_NODE_DEREF(n)						\
{									\
	KLOG(KL_DBG3, "NDEREF %p was %d", (n), atomic_read(&(n)->ref));	\
	btree_node_deref((n));						\
}
