#pragma once

struct ext {
	u64	start;
	u64	len;
};

struct ext_tree {
	atomic_t	ref;
	struct btree 	*btree;
	struct ds_sb	*sb;
	int		releasing;
};

struct ext_tree *ext_tree_create(struct ds_sb *sb, u64 block);

int ext_tree_ext_alloc(struct ext_tree *tree, u64 len, struct ext *ext);
void ext_tree_ext_free(struct ext_tree *tree, struct ext *ext);

void ext_tree_stop(struct ext_tree *tree);
void ext_tree_deref(struct ext_tree *tree);

int ext_tree_init(void);
void ext_tree_finit(void);
