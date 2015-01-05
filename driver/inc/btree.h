#pragma once

#include <linux/types.h>

#define BTREE_T 64

#pragma pack(push, 1)

struct btree_key {
	u8 bytes[16];
};
/* size = 16 */

#define BTREE_NODE_PAD 8

struct btree_node_disk {
	struct btree_key	keys[2*BTREE_T-1];
	__be64			values[2*BTREE_T-1];
	__be64			childs[2*BTREE_T];
	__be32			leaf;
	__be32			nr_keys;
	__be8			pad[BTREE_NODE_PAD];
};

struct btree;

struct btree_node {
	struct btree		*tree;
	u64			block;
	struct btree_key	keys[2*BTREE_T-1];
	u64			values[2*BTREE_T-1];
	u64			childs[2*BTREE_T];
	u32			leaf;
	u32			nr_keys;
};

/* size = (2T-1)*16 + (2T-1)*8 + 2T*8 + 4*4 + pad */

struct btree {
	struct btree_node	*root;
	struct ds_sb		*sb;
};

struct btree_info {
	u64 nr_keys;
	u64 nr_nodes;
};

#pragma pack(pop)

_Static_assert(sizeof(struct __btree_node) <= 4096, "size is not correct");

struct btree *btree_create(struct ds_sb *sb, u64 begin);

void btree_delete(struct btree *tree, int from_disk);

int btree_insert_key(struct btree *tree, struct btree_key *key,
	u64 value, int replace);

int btree_find_key(struct btree *tree,
	struct btree_key *key,
	struct u64 *pvalue);

int btree_delete_key(struct btree *tree,
	struct btree_key *key);

struct btree_key *btree_gen_key(void);
struct u64 btree_gen_value(void);

char *btree_key_hex(struct btree_key *key);
char *btree_value_hex(u64 value);

void btree_log(struct btree *tree, int llevel);

void btree_stats(struct btree *tree, struct btree_info *info);

int btree_check(struct btree *tree);

int btree_test(int num_keys);

