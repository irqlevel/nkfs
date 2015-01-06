#pragma once

#include <linux/types.h>

#define BTREE_T 64

#pragma pack(push, 1)

struct btree_key {
	u8 bytes[16];
};
/* size = 16 */

#define BTREE_SIG1 ((u32)0xCBACBADA)
#define BTREE_SIG2 ((u32)0x3EFFEEFE)

#define BTREE_NODE_PAD 8

struct btree_node_disk {
	__be32			sig1;
	struct btree_key	keys[2*BTREE_T-1];
	__be64			values[2*BTREE_T-1];
	__be64			childs[2*BTREE_T];
	__be32			leaf;
	__be32			nr_keys;
	__be32			sig2;
	u8			pad[BTREE_NODE_PAD];
};

struct btree;

struct btree_node {
	u64			block;
	struct btree_key	keys[2*BTREE_T-1];
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

/* size = (2T-1)*16 + (2T-1)*8 + 2T*8 + 4*4 + pad */

struct btree {
	struct btree_node	*root;
	struct ds_sb		*sb;
	struct rw_semaphore	rw_lock;
	spinlock_t		lock;
	struct rb_root		nodes;
	atomic_t		ref;
	int			releasing;
	int			nodes_active;
};

struct btree_info {
	u64 nr_keys;
	u64 nr_nodes;
};

#pragma pack(pop)

_Static_assert(sizeof(struct btree_node_disk) <= 4096, "size is not correct");

struct btree *btree_create(struct ds_sb *sb, u64 begin);

u64 btree_get_root_block(struct btree *tree);

void btree_ref(struct btree *tree);
void btree_deref(struct btree *tree);

int btree_insert_key(struct btree *tree, struct btree_key *key,
	u64 value, int replace);

int btree_find_key(struct btree *tree,
	struct btree_key *key,
	u64 *pvalue);

int btree_delete_key(struct btree *tree,
	struct btree_key *key);

struct btree_key *btree_gen_key(void);
u64 btree_gen_value(void);

char *btree_key_hex(struct btree_key *key);
char *btree_value_hex(u64 value);

void btree_log(struct btree *tree, int llevel);

void btree_stats(struct btree *tree, struct btree_info *info);

int btree_check(struct btree *tree);

int btree_test(int num_keys);

