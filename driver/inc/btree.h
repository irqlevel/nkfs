#pragma once

#include <linux/types.h>

#define BTREE_T 1024

#pragma pack(push, 1)

struct btree_key {
	char bytes[16];
};

struct btree_link {
	union {
		__be64 off;
		struct btree_node *ptr;
	};
};

struct btree_node {
	struct	btree_key keys[2*BTREE_T-1];
	struct	btree_link childs[2*BTREE_T];
	struct	btree_link data;
};

#pragma pack(pop)
