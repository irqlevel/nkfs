#pragma once

#include <linux/types.h>

#pragma pack(push, 1)

struct btree;

struct btree_node {
	u64			block;
	struct ds_obj_id	keys[2*BTREE_T-1];
	u64			values[2*BTREE_T-1];
	u64			childs[2*BTREE_T];
	struct btree		*tree;
	atomic_t		ref;
	struct rb_node		nodes_link;
	u32			leaf;
	u32			nr_keys;
	u32			t;
	u32			sig1;
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

int btree_insert_key(struct btree *tree, struct ds_obj_id *key,
	u64 value, int replace);

int btree_find_key(struct btree *tree,
	struct ds_obj_id *key,
	u64 *pvalue);

int btree_delete_key(struct btree *tree,
	struct ds_obj_id *key);

void btree_stop(struct btree *tree);

typedef void (*btree_key_erase_clb_t)(struct ds_obj_id *key, u64 value,
	void *ctx);

void btree_erase(struct btree *tree,
	btree_key_erase_clb_t key_erase_clb,
	void *ctx);

struct ds_obj_id *btree_gen_key(void);
u64 btree_gen_value(void);

void btree_key_by_u64(u64 val, struct ds_obj_id *key);
u64 btree_key_to_u64(struct ds_obj_id *key);

char *btree_key_hex(struct ds_obj_id *key);
char *btree_value_hex(u64 value);

void btree_log(struct btree *tree, int llevel);

void btree_stats(struct btree *tree, struct btree_info *info);

int btree_check(struct btree *tree);

int btree_test(int num_keys);

int btree_init(void);
void btree_finit(void);
