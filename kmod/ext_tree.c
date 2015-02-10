#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ext_tree"

struct ext_need {
	u64 size;
	struct btree_node *node;
	int index;
};

static struct kmem_cache *ext_tree_cachep;

static struct ext_tree *ext_tree_alloc(void)
{
	struct ext_tree *tree;

	tree = kmem_cache_alloc(ext_tree_cachep, GFP_NOIO);
	if (!tree)
		return NULL;
	memset(tree, 0, sizeof(*tree));
	atomic_set(&tree->ref, 1);
	return tree;
}

static void ext_tree_free(struct ext_tree *tree)
{
	kmem_cache_free(ext_tree_cachep, tree);
}

static void ext_tree_release(struct ext_tree *tree)
{
	btree_deref(tree->btree);
	ext_tree_free(tree);
}

void ext_tree_deref(struct ext_tree *tree)
{
	if (atomic_dec_and_test(&tree->ref))
		ext_tree_release(tree);	
}

struct ext_tree *ext_tree_create(struct ds_sb *sb, u64 block)
{
	struct ext_tree *tree;
	
	tree = ext_tree_alloc();
	if (!tree)
		return NULL;

	tree->sb = sb;
	tree->btree = btree_create(sb, block);
	if (!tree->btree)
		goto fail;

	return tree;
fail:
	ext_tree_free(tree);
	return NULL;
}

void ext_tree_stop(struct ext_tree *tree)
{
	tree->releasing = 1;
	btree_stop(tree->btree);
}

int ext_tree_init(void)
{
	int err;

	ext_tree_cachep = kmem_cache_create("ext_tree_cachep",
			sizeof(struct ext_tree), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!ext_tree_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}
	err = 0;
out:
	return err;
}

void ext_tree_finit(void)
{
	kmem_cache_destroy(ext_tree_cachep);
}

void ext_to_btree_key_value(struct ext *ext,
		struct btree_key *key, struct btree_value *value)
{
	btree_key_by_u64(ext->start, key);
	btree_value_by_u64(ext->size, value);
}

void ext_by_btree_key_value(struct btree_key *key,
		struct btree_value *value, struct ext *ext)
{
	ext->start = btree_key_to_u64(key);
	ext->size = btree_value_to_u64(value);
}

int ext_find_fit_clb(void *ctx, struct btree_node *node, int index)
{
	struct btree_key *key;
	struct btree_value *value;
	struct ext_need *need = (struct ext_need *)ctx;
	struct ext ext;

	key = btree_node_key(node, index);
	value = btree_node_value(node, index);
	ext_by_btree_key_value(key, value, &ext);
	if (ext.size >= need->size) {
		BTREE_NODE_REF(node);
		need->node = node;
		need->index = index;
		return 1;
	}

	return 0;
}

int ext_tree_ext_alloc(struct ext_tree *tree, u64 size, struct ext *pext)
{
	struct ext ext;
	struct ext_need need;
	struct btree_key *pkey;
	struct btree_value *pvalue;
	int err;

	memset(pext, 0, sizeof(*pext));

restart:
	memset(&need, 0, sizeof(need));
	need.size = size;
	btree_read_lock(tree->btree);
	if (!btree_enum_tree(tree->btree, ext_find_fit_clb, &need)) {
		err = -ENOENT;
		btree_read_unlock(tree->btree);
		goto out;
	}
	btree_read_unlock(tree->btree);
	btree_write_lock(tree->btree);
	if (!need.node->block || (need.index >= need.node->nr_keys)) {
		btree_write_unlock(tree->btree);
		BTREE_NODE_DEREF(need.node);
		goto restart;
	}

	pkey = btree_node_key(need.node, need.index);
	pvalue = btree_node_value(need.node, need.index);
	ext_by_btree_key_value(pkey, pvalue, &ext);
	if (ext.size < size) {
		btree_write_unlock(tree->btree);
		BTREE_NODE_DEREF(need.node);
		goto restart;
	}

	if ((ext.size - size) == 0) {
		err = btree_node_delete_key(tree->btree->root, pkey);
		if (err)
			goto out;
		*pext = ext;
	} else {
		btree_value_by_u64(ext.size - size, pvalue);
		err = btree_node_write(need.node);
		if (err)
			goto out;
		pext->start = ext.start + (ext.size - size);
		pext->size = size;
	}
	btree_write_unlock(tree->btree);
	BTREE_NODE_DEREF(need.node);
out:
	return err;
}

void ext_tree_ext_free(struct ext_tree *tree, struct ext *ext)
{
	struct btree_key key;
	struct btree_value value;

	ext_to_btree_key_value(ext, &key, &value);

	if (!btree_insert_key(tree->btree, &key, &value, 0))
		BUG();
}
