#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "btree"

static struct btree_node *btree_node_create(void)
{
	struct btree_node *node;

	node = kmalloc(sizeof(*node), GFP_NOFS);
	if (!node) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}
	memset(node, 0, sizeof(*node));
	node->max_nr_keys = ARRAY_SIZE(node->keys);
	node->leaf = 0;

	BUG_ON((node->max_nr_keys + 1) & 1);
	node->t = (node->max_nr_keys + 1)/2;

	KLOG(KL_DBG, "node %p created", node);
	return node;	
}

static void __btree_node_free(struct btree_node *node)
{
	KLOG(KL_DBG, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);
	kfree(node);
}

static void btree_node_delete(struct btree_node *node)
{
	struct btree_node *child;
	int i;

	if (!node->leaf) {
		for (i = 0; i < node->nr_keys + 1; i++) {
			child = node->childs[i].addr;
			BUG_ON(!child);
			btree_node_delete(child);
		}
	}
	KLOG(KL_DBG, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);
	__btree_node_free(node);
}

struct btree_key *btree_gen_key(void)
{
	struct btree_key *key;
	key = kmalloc(sizeof(*key), GFP_NOFS);
	if (!key)
		return NULL;

	if (ds_random_buf_read(key, sizeof(*key), 1)) {
		kfree(key);
		return NULL;
	}
	return key;
}

struct btree_value *btree_gen_value(void)
{
	struct btree_value *value;
	value = kmalloc(sizeof(*value), GFP_NOFS);
	if (!value)
		return NULL;

	if (ds_random_buf_read(value, sizeof(*value), 1)) {
		kfree(value);
		return NULL;
	}
	return value;
}

char *btree_key_hex(struct btree_key *key)
{
	return bytes_hex((char *)key, sizeof(*key));
}

char *btree_value_hex(struct btree_value *value)
{
	return bytes_hex((char *)value, sizeof(*value));
}

struct btree *btree_create(void)
{
	struct btree *tree;

	tree = kmalloc(sizeof(*tree), GFP_NOFS);
	if (!tree) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}

	memset(tree, 0, sizeof(*tree));
	tree->root = btree_node_create();
	if (!tree->root) {
		goto fail;
	}
	tree->root->leaf = 1;
	KLOG(KL_DBG, "tree %p created", tree);
	return tree;
fail:
	btree_delete(tree);
	return NULL;
}

void btree_delete(struct btree *tree)
{
	if (tree->root)
		btree_node_delete(tree->root);
	kfree(tree);
	KLOG(KL_DBG, "tree %p deleted", tree);
}

static int btree_cmp_key(
	struct btree_key *key1,
	struct btree_key *key2)
{
	return memcmp(key1, key2, sizeof(*key1));
}

static void btree_copy_key(
	struct btree_key *dst,
	struct btree_key *src)
{
	memcpy(dst, src, sizeof(*dst));
}

static void btree_copy_value(
	struct btree_value *dst,
	struct btree_value *src)
{
	memcpy(dst, src, sizeof(*dst));
}

static struct btree_key btree_zero_key = {{0,0,0,0,
					0,0,0,0,
					0,0,0,0,
					0,0,0,0}};

static int btree_key_is_zero(struct btree_key *key)
{
	return (0 == btree_cmp_key(key, &btree_zero_key)) ? 1 : 0;
}

static int btree_node_is_full(struct btree_node *node)
{
	return (node->max_nr_keys == node->nr_keys) ? 1 : 0;
}

static void btree_node_copy_key_value(struct btree_node *dst, int dst_index,
	struct btree_key *key, struct btree_value *value)
{
	BUG_ON(dst_index < 0 ||
		dst_index >= ARRAY_SIZE(dst->keys));

	btree_copy_key(&dst->keys[dst_index], key);
	btree_copy_value(&dst->values[dst_index], value);
}

static void btree_node_copy_kv(struct btree_node *dst, int dst_index,
	struct btree_node *src, int src_index)
{
	BUG_ON(dst_index < 0 || src_index < 0 ||
		dst_index >= ARRAY_SIZE(dst->keys) ||
		src_index >= ARRAY_SIZE(src->keys));

	btree_copy_key(&dst->keys[dst_index], &src->keys[src_index]);
	btree_copy_value(&dst->values[dst_index], &src->values[src_index]);
}

static void btree_node_copy_child(struct btree_node *dst, int dst_index,
	struct btree_node *src, int src_index)
{
	BUG_ON(dst_index < 0 || src_index < 0 ||
		dst_index >= ARRAY_SIZE(dst->childs) ||
		src_index >= ARRAY_SIZE(src->childs));

	dst->childs[dst_index].addr = src->childs[src_index].addr;
}

static void btree_node_copy_child_value(struct btree_node *dst, int dst_index,
	struct btree_node *value)
{
	BUG_ON(dst_index < 0 ||
		dst_index >= ARRAY_SIZE(dst->childs));

	dst->childs[dst_index].addr = value;
}

static void btree_node_put_child_value(struct btree_node *dst, int dst_index,
	struct btree_node *value)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys; i >= dst_index; i--) {
		btree_node_copy_child(dst, i + 1, dst, i);
	}

	btree_node_copy_child_value(dst, dst_index, value);
}


static void btree_node_put_child(struct btree_node *dst, int dst_index,
	struct btree_node *src, int src_index)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys; i >= dst_index; i--) {
		btree_node_copy_child(dst, i + 1, dst, i);
	}

	btree_node_copy_child(dst, dst_index, src, src_index);
}

static void btree_node_put_key_value(struct btree_node *dst, int dst_index,
	struct btree_key *key, struct btree_value *value)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys - 1; i >= dst_index; i--) {
		btree_node_copy_kv(dst, i + 1, dst, i);
	}

	btree_node_copy_key_value(dst, dst_index, key, value);
}

static void btree_node_put_kv(struct btree_node *dst, int dst_index,
	struct btree_node *src, int src_index)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys - 1; i >= dst_index; i--) {
		btree_node_copy_kv(dst, i + 1, dst, i);
	}

	btree_node_copy_kv(dst, dst_index, src, src_index);
}

static void btree_node_zero_kv(struct btree_node *dst, int dst_index)
{
	BUG_ON(dst_index < 0 || dst_index >= ARRAY_SIZE(dst->keys));

	memset(&dst->keys[dst_index], 0, sizeof(struct btree_key));
	memset(&dst->values[dst_index], 0, sizeof(struct btree_value));
}

static void btree_node_split_child(struct btree_node *node,
		int index, struct btree_node *new)
{
	struct btree_node *child;
	int i;

	BUG_ON(index < 0 || index > node->nr_keys);
	child = node->childs[index].addr;
	BUG_ON(!child || !btree_node_is_full(child));	
	BUG_ON(!new);

	KLOG(KL_DBG, "Splitting node [%p %d] child[%d]=[%p %d]",
		node, node->nr_keys, index, child, child->nr_keys);


	new->leaf = child->leaf;
	/* copy T-1 keys from child to new */
	for (i = 0; i < new->t - 1; i++)
		btree_node_copy_kv(new, i, child, i + new->t);
	new->nr_keys = new->t - 1;

	/* copy T childs from child to new */
	if (!child->leaf) {
		for (i = 0; i < new->t; i++) {
			btree_node_copy_child(new, i, child, i + new->t);
		}
	}

	/* shift node childs to the right by one */
	child->nr_keys = new->t - 1;

	/* setup node new child */
	btree_node_put_child_value(node, index + 1, new);

	/* move mid key from child to node */
	btree_node_put_kv(node, index, child, new->t - 1);
	node->nr_keys++;

	KLOG(KL_DBG, "Splitted node [%p %d] child[%d]=[%p %d] new [%p %d]",
		node, node->nr_keys, index, child, child->nr_keys,
		new, new->nr_keys);
}

static int btree_node_has_key(struct btree_node *node, struct btree_key *key)
{
	int i;
	for (i = 0; i < node->nr_keys; i++) {
		if (0 == btree_cmp_key(&node->keys[i], key))
			return i;
	}
	return -1;
}

static int
btree_node_find_key_index(struct btree_node *node,
	struct btree_key *key)
{
	int i = 0;
	while (i < node->nr_keys && btree_cmp_key(key, &node->keys[i]) > 0)
		i++;
	return i;
}

static int btree_node_insert_nonfull(struct btree_node *node,
	struct btree_key *key,
	struct btree_value *value,
	int replace)
{
	int i;

	while (1) {
		KLOG(KL_DBG, "node [%p %d] leaf %d",
			node, node->nr_keys, node->leaf);
		/* if key exists replace value */
		i = btree_node_has_key(node, key);
		if (i >= 0) {
			if (replace) {
				btree_copy_value(
						&node->values[i],
						value);
				return 0;
			} else {
				return -1;
			}
		}

		if (node->leaf) {
			/* key doesnt exists so place key value in sorted
			 * order
			 */
			i = btree_node_find_key_index(node, key);
			btree_node_put_key_value(node, i, key, value);
			node->nr_keys++;
			KLOG(KL_DBG, "inserted key into node=%p nr_keys=%d",
					node, node->nr_keys);
			return 0;
		} else {
			struct btree_node *child;
			i = btree_node_find_key_index(node, key);
			child = node->childs[i].addr;
			if (btree_node_is_full(child)) {
				struct btree_node *new;
				new = btree_node_create();
				if (!new)
					return -1;
				btree_node_split_child(node, i, new);
				continue; /* restart */
			}
			node = child;
		}
	}
}

int btree_insert_key(struct btree *tree, struct btree_key *key,
	struct btree_value *value,
	int replace)
{
	if (btree_key_is_zero(key)) {
		KLOG(KL_ERR, "key is zero");
		return -1;
	}

	if (btree_node_is_full(tree->root)) {
		struct btree_node *new, *new2;
		new = btree_node_create();
		if (new == NULL)
			return -1;
		new2 = btree_node_create();
		if (new2 == NULL) {
			btree_node_delete(new);
			return -1;
		}

		new->leaf = 0;
		new->nr_keys = 0;
		new->childs[0].addr = tree->root;
		btree_node_split_child(new, 0, new2);
		tree->root = new;
	}

	return btree_node_insert_nonfull(tree->root, key, value, replace);
}

static struct btree_node *btree_node_find_key(struct btree_node *node,
		struct btree_key *key, int *pindex)
{
	int i;

	while (1) {
		if (node->nr_keys == 0)
			return NULL;

		i = btree_node_find_key_index(node, key);
		if (i < node->nr_keys &&
				btree_cmp_key(key, &node->keys[i]) == 0) {
			*pindex = i;
			return node;
		} else if (node->leaf)
			return NULL;
		else
			node = node->childs[i].addr;
	}
}

int btree_find_key(struct btree *tree,
	struct btree_key *key,
	struct btree_value **pvalue)
{
	struct btree_node *node;
	struct btree_value *value;
	int index;

	if (btree_key_is_zero(key)) {
		KLOG(KL_ERR, "key is zero");
		return -1;
	}

	node = btree_node_find_key(tree->root, key, &index);
	if (node == NULL)
		return -1;

	value = kmalloc(sizeof(*value), GFP_NOFS);
	if (!value)
		return -1;

	btree_copy_value(value, &node->values[index]);
	*pvalue = value;

	return 0;
}

static void __btree_node_delete_child_index(struct btree_node *node,
		int index)
{
	int i;
	
	/* do shift to the left by one */
	for (i = (index + 1); i < node->nr_keys + 1; i++) {
		node->childs[i-1].addr = node->childs[i].addr;
	}
	/* zero last slot */
	node->childs[i-1].addr = NULL;	
}

static void __btree_node_delete_key_index(struct btree_node *node,
		int index)
{
	int i;

	BUG_ON(node->nr_keys < 1);
	/* do shift to the left by one */
	for (i = (index + 1); i < node->nr_keys; i++) {
		btree_node_copy_kv(node, i-1, node, i);	
	}
	/* zero last slot */
	btree_node_zero_kv(node, i-1);
}


static void btree_node_leaf_delete_key(struct btree_node *node,
		struct btree_key *key)
{
	int index;
	BUG_ON(!node->leaf);
	index = btree_node_has_key(node, key);
	BUG_ON(index < 0 || index >= node->nr_keys);
	__btree_node_delete_key_index(node, index);
	node->nr_keys--;
}

static struct btree_node *
btree_node_child_balance(struct btree_node *node,
	int child_index);

static struct btree_node *
btree_node_find_left_most(struct btree_node *node, int *pindex)
{
	struct btree_node *curr = node;

	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = curr->nr_keys - 1;
			return curr;
		}
		curr = btree_node_child_balance(curr, curr->nr_keys);
	}
}

static struct btree_node *
btree_node_find_right_most(struct btree_node *node, int *pindex)
{
	struct btree_node *curr = node;
	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = 0;
			return curr;
		}
		curr = btree_node_child_balance(curr, 0);
	}
}

static void btree_node_merge(struct btree_node *dst,
	struct btree_node *src, struct btree_key *key,
	struct btree_value *value)
{
	int i, pos;

	KLOG(KL_DBG, "Merging %p %d -> %p %d",
		src, src->nr_keys, dst, dst->nr_keys);
	/* copy mid key and value */
	btree_copy_key(&dst->keys[dst->nr_keys], key);
	btree_copy_value(&dst->values[dst->nr_keys], value);

	pos = dst->nr_keys + 1;
	for (i = 0; i < src->nr_keys; i++, pos++) {
		/* copy key-values */
		btree_node_copy_kv(dst, pos, src, i);
		/* copy childs */
		btree_node_copy_child(dst, pos, src, i);
	}
	/* copy last child */
	btree_node_copy_child(dst, pos, src, i);
	/* update keys num */
	dst->nr_keys = dst->nr_keys + 1 + src->nr_keys;

	KLOG(KL_DBG, "Merged %p -> %p nr_keys %d",
		src, dst, dst->nr_keys);	
}

static void btree_node_copy(struct btree_node *dst, struct btree_node *src)
{
	int i;
	for (i = 0; i < src->nr_keys; i++) {
		btree_node_copy_kv(dst, i, src, i);
		btree_node_copy_child(dst, i, src, i);
	}
	btree_node_copy_child(dst, i, src, i);
	dst->nr_keys = src->nr_keys;
	dst->leaf = src->leaf;
}

static void btree_node_child_give_key(struct btree_node *node,
	struct btree_node *child, int child_index,
	struct btree_node *sib, int left)
{
	/* give child an extra key by moving a key from node
	* down into child, moving a key from child's
	* immediate left or right sibling up into node,
	* and moving the appropriate child pointer from the
	* sibling into child
	*/

	if (!left) {
		btree_node_copy_kv(child, child->nr_keys, node, child_index);
		btree_node_copy_kv(node, child_index, sib, 0);
		btree_node_copy_child(child, child->nr_keys + 1, sib, 0);

		__btree_node_delete_key_index(sib, 0);
		__btree_node_delete_child_index(sib, 0);
	} else {
		btree_node_put_kv(child, 0, node, child_index-1);
		btree_node_copy_kv(node, child_index-1, sib, sib->nr_keys-1);
		btree_node_put_child(child, 0, sib, sib->nr_keys);

		__btree_node_delete_key_index(sib, sib->nr_keys-1);
		__btree_node_delete_child_index(sib, sib->nr_keys);
	}
	child->nr_keys++;
	sib->nr_keys--;
}

static struct btree_node * btree_node_child_merge(struct btree_node *node,
	struct btree_node *child, int child_index,
	struct btree_node *sib, int left)
{
	struct btree_node *merged;
	/* merge child with left, which involves
	* moving a key from node down into the new 
	* merged node to become the median key for
	* that node.
	*/

	/* k0 k1 k2	k0 k2
	* c0 c1 c2 c3  c0 c1c2 c3
	*/

	if (left) {
		btree_node_merge(sib, child, &node->keys[child_index-1],
			&node->values[child_index-1]);
		__btree_node_delete_key_index(node, child_index-1);
		__btree_node_delete_child_index(node, child_index);
		node->nr_keys--;
		__btree_node_free(child);
		merged = sib;
	} else {
		btree_node_merge(child, sib, &node->keys[child_index],
			&node->values[child_index]);
		__btree_node_delete_key_index(node, child_index);
		__btree_node_delete_child_index(node, child_index+1);
		node->nr_keys--;
		__btree_node_free(sib);
		merged = child;
	}

	if (node->nr_keys == 0) {
		btree_node_copy(node, merged);
		__btree_node_free(merged);
		return node;
	} else {
		return merged;
	}
}

static struct btree_node *
btree_node_child_balance(struct btree_node *node,
	int child_index)
{
	struct btree_node *child;

	BUG_ON(node->leaf);
	child = node->childs[child_index].addr;
	BUG_ON(!child);

	KLOG(KL_DBG, "child %p nr_keys %d t %d",
		child, child->nr_keys, child->t);

	if (child->nr_keys < child->t) {
		struct btree_node *left = (child_index > 0) ?
			node->childs[child_index-1].addr : NULL;
		struct btree_node *right = (child_index < node->nr_keys) ?
			node->childs[child_index+1].addr : NULL;
		
		if (left && left->nr_keys >= left->t) {
			btree_node_child_give_key(node, child,
				child_index, left, 1);
		} else if (right && right->nr_keys >= right->t) {
			btree_node_child_give_key(node, child,
				child_index, right, 0);
		} else if (left && left->nr_keys < left->t) {
			child = btree_node_child_merge(node, child,
				child_index, left, 1);
		} else if (right->nr_keys < right->t) {
			child = btree_node_child_merge(node, child,
				child_index, right, 0);
		} else {
			KLOG(KL_ERR, "no way to add key to child=%p",
				child);
			BUG();
		}
	}

	return child;
}

static int btree_node_delete_key(struct btree_node *node,
		struct btree_key *key)
{
	struct btree_key key_copy;
	int i;

restart:
	btree_copy_key(&key_copy, key);
	key = &key_copy;

	if ((i = btree_node_has_key(node, key)) >= 0) {
		if (node->leaf) {
			btree_node_leaf_delete_key(node, key);
			return 0;
		} else {
			struct btree_node *pre_child = NULL;
			struct btree_node *suc_child = NULL;
			int index;

			index = btree_node_has_key(node, key);
			BUG_ON(index < 0);

			pre_child = node->childs[index].addr;
			suc_child = node->childs[index+1].addr;

			if (pre_child->nr_keys >= pre_child->t) {
				struct btree_node *pre;
				int pre_index;

				pre = btree_node_find_left_most(pre_child,
					&pre_index);
				btree_node_copy_kv(node, index,
					pre, pre_index);
				node = pre;
				key = &pre->keys[pre_index];
				goto restart;
			} else if (suc_child->nr_keys >= suc_child->t) {
				struct btree_node *suc;
				int suc_index;

				suc = btree_node_find_right_most(suc_child,
					&suc_index);
				btree_node_copy_kv(node, index,
					suc, suc_index);
				node = suc;
				key = &suc->keys[suc_index];
				goto restart;
			} else {
				/* merge key and all of suc_child
				* into pre_child
				* node loses key and pointer to suc_child
				*/
				int key_index = pre_child->nr_keys;

				btree_node_merge(pre_child, suc_child,
					&node->keys[index],
					&node->values[index]);
				/* delete key from node */
				__btree_node_delete_key_index(node,
						index);
				__btree_node_delete_child_index(node,
						index+1);
				node->nr_keys--;

				/* delete suc_child */
				__btree_node_free(suc_child);
				if (node->nr_keys == 0) {
					btree_node_copy(node, pre_child);
					__btree_node_free(pre_child);
					pre_child = node;
				}
				node = pre_child;
				key = &pre_child->keys[key_index];
				goto restart;
			}
		}
	} else {
		if (node->leaf) {
			return -1;
		} else {
			struct btree_node *child;
			BUG_ON(btree_node_has_key(node, key) >= 0);
			i = btree_node_find_key_index(node, key);
			child = btree_node_child_balance(node, i);
			node = child;
			goto restart;
		}
	}
}

int btree_delete_key(struct btree *tree, struct btree_key *key)
{
	if (btree_key_is_zero(key))
		return -1;

	return btree_node_delete_key(tree->root, key);
}


static void btree_log_node(struct btree_node *node, u32 height, int llevel)
{
	struct btree_node *child;
	int i;

	KLOG(llevel, "node %p nr_keys %d leaf %d height %u",
			node, node->nr_keys, node->leaf, height);

	if (node->nr_keys) {
		for (i = 0; i < node->nr_keys + 1; i++) {
			child = node->childs[i].addr;
			if (child != NULL)
				btree_log_node(child, height+1, llevel);
		}
	}
}

static void btree_node_stats(struct btree_node *node,
	struct btree_info *info)
{
	struct btree_node *child;
	int i;

	if (node->nr_keys) {
		for (i = 0; i < node->nr_keys + 1; i++) {
			child = node->childs[i].addr;
			if (child != NULL)
				btree_node_stats(child, info);
		}
	}
	info->nr_nodes++;
	info->nr_keys += node->nr_keys;
}

void btree_stats(struct btree *tree, struct btree_info *info)
{
	memset(info, 0, sizeof(*info));
	btree_node_stats(tree->root, info);
}

void btree_log(struct btree *tree, int llevel)
{
	btree_log_node(tree->root, 1, llevel);
}

static int btree_node_check(struct btree_node *node, int root)
{
	int i;
	int errs = 0;
	struct btree_key *prev_key;

	if (root) {
		if (node->nr_keys > (2*node->t - 1)) {
			KLOG(KL_ERR, "node %p contains %d keys",
				node, node->nr_keys);
			errs++;
		}
	} else {
		if (node->nr_keys > (2*node->t - 1)) {
			KLOG(KL_ERR, "node %p contains %d keys",
				node->nr_keys);
			errs++;
		}

		if (node->nr_keys < (node->t - 1)) {
			KLOG(KL_ERR, "node %p contains %d keys",
				node, node->nr_keys);
			errs++;
		}
	}

	prev_key = NULL;
	for (i = 0 ; i < node->nr_keys; i++) {
		if (btree_key_is_zero(&node->keys[i])) {
			KLOG(KL_ERR, "node %p zero key %d found",
				node, i);
			errs++;
		} else {
			if (prev_key && (btree_cmp_key(prev_key,
				&node->keys[i]) >= 0)) {
				KLOG(KL_ERR, "node %p key %d not sorted",
					node, i);
				errs++;
			}
			prev_key = &node->keys[i];
		}
		if (!node->leaf) {
			if (!node->childs[i].addr) {
				KLOG(KL_ERR, "node %p zero child %d found",
					node, i);
				errs++;
			}
		}
	}

	if (!node->leaf) {
		if (!root || (node->nr_keys > 0)) {
			if (!node->childs[i].addr) {
				KLOG(KL_ERR, "node %p zero child %d found",
						node, i);
				errs++;
			}
		}
	}

	if (!node->leaf) {
		for (i = 0; i < node->nr_keys+1; i++) {
			struct btree_node *child;
			child = node->childs[i].addr;
			BUG_ON(!child);
			errs+= btree_node_check(child, 0);
		}
	}
	return errs;
}

int btree_check(struct btree *tree)
{
	return btree_node_check(tree->root, 1);
}

static int btree_test_insert_case(struct btree *t, struct btree_key **keys,
		struct btree_value **values, int num_keys, int replace,
		int fail_ok)
{
	int i;
	int err;
	char *key_hex = NULL, *val_hex = NULL;

	/* insert keys */
	for (i = 0; i < num_keys; i++) {
		key_hex = btree_key_hex(keys[i]);
		if (!key_hex) {
			KLOG(KL_ERR, "cant get key hex");
			err = -ENOMEM;
			goto cleanup;
		}
		val_hex = btree_value_hex(values[i]);
		if (!val_hex) {
			KLOG(KL_ERR, "cant get val hex");
			err = -ENOMEM;
			goto cleanup;
		}

		err = btree_insert_key(t, keys[i], values[i], replace);
		if (err && !fail_ok) {
			KLOG(KL_ERR, "Cant insert key[%d] %s rplc=%d rc=%d",
					i, key_hex, replace, err);
			goto cleanup;
		}

		KLOG(KL_TST, "Insert key[%d] %s -> %s rplc=%d rc=%d",
				i, key_hex, val_hex, replace, err);

		kfree(key_hex);
		key_hex = NULL;
		kfree(val_hex);
		val_hex = NULL;
	}

	err = 0;
cleanup:
	if (key_hex)
		kfree(key_hex);
	if (val_hex)
		kfree(val_hex);
	return err;	
}

static int btree_test_find_case(struct btree *t, struct btree_key **keys,
		struct btree_value **values, int num_keys)
{
	int i;
	int err;
	char *key_hex = NULL, *val_hex = NULL;
	struct btree_value *value = NULL;

	for (i = 0; i < num_keys; i++) {
		key_hex = btree_key_hex(keys[i]);
		if (!key_hex) {
			KLOG(KL_ERR, "cant get key hex");
			err = -ENOMEM;
			goto cleanup;
		}

		err = btree_find_key(t, keys[i], &value);
		if (err) {
			KLOG(KL_ERR, "Cant found key[%d] %s, rc=%d",
					i, key_hex, err);
			goto cleanup;
		}

		val_hex = btree_value_hex(value);
		if (!val_hex) {
			KLOG(KL_ERR, "cant get val hex");
			err = -ENOMEM;
			goto cleanup;
		}

		KLOG(KL_TST, "Found key[%d] %s -> %s, rc=%d",
				i, key_hex, val_hex, err);
		if (0 != memcmp(values[i], value, sizeof(*value))) {
			KLOG(KL_ERR, "value mismatch");
			err = -EINVAL;
			goto cleanup;
		}
		kfree(value);
		value = NULL;
		kfree(key_hex);
		key_hex = NULL;
		kfree(val_hex);
		val_hex = NULL;
	}

	err = 0;
cleanup:
	if (value)
		kfree(value);
	if (key_hex)
		kfree(key_hex);
	if (val_hex)
		kfree(val_hex);
	return err;	
}

static int btree_test_delete_case(struct btree *t, struct btree_key **keys,
		struct btree_value **values, int num_keys)
{
	int i;
	int err;
	char *key_hex = NULL;

	for (i = 0; i < num_keys; i++) {
		key_hex = btree_key_hex(keys[i]);
		if (!key_hex) {
			KLOG(KL_ERR, "cant get key hex");
			err = -ENOMEM;
			goto cleanup;
		}

		err = btree_delete_key(t, keys[i]);
		if (err) {
			KLOG(KL_ERR, "Cant delete key[%d] %s, rc=%d",
					i, key_hex, err);
			goto cleanup;
		} else {
			KLOG(KL_TST, "Deleted key[%d] %s, rc=%d", i, key_hex, err);
		}
		kfree(key_hex);
		key_hex = NULL;
	}

	err = 0;
cleanup:
	if (key_hex)
		kfree(key_hex);
	return err;	
}


static int btree_test_stats_case(struct btree *t)
{
	struct btree_info info;
	/* output some stats */
	btree_log(t, KL_TST);
	btree_stats(t, &info);

	KLOG(KL_TST, "tree %p nr_keys=%llu nr_nodes=%llu",
		t, info.nr_keys, info.nr_nodes);

	return 0;
}

void btree_test_free_kvs(struct btree_key **keys,
	struct btree_value **values, int num_keys)
{
	int i;

	if (values) {
		for (i = 0; i < num_keys; i++)
			if (values[i])
				kfree(values[i]);
		kfree(values);
	}

	if (keys) {
		for (i = 0; i < num_keys; i++)
			if (keys[i])
				kfree(keys[i]);
		kfree(keys);
	}
}

int btree_test_gen_kvs(struct btree_key ***pkeys,
	struct btree_value ***pvalues, int num_keys)
{
	struct btree_key **keys = NULL;
	struct btree_value **values = NULL;
	int i, err;

	*pkeys = NULL;
	*pvalues = NULL;

	keys = kmalloc(num_keys*sizeof(struct btree_key *), GFP_NOFS);
	if (!keys) {
		KLOG(KL_ERR, "cant malloc keys");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(keys, 0, num_keys*sizeof(struct btree_key *));

	values = kmalloc(num_keys*sizeof(struct btree_value *), GFP_NOFS);
	if (!values) {
		KLOG(KL_ERR,"cant malloc values");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(values, 0, num_keys*sizeof(struct btree_value *));

	for (i = 0; i < num_keys; i++) {
		keys[i] = btree_gen_key();
		if (!keys[i]) {
			KLOG(KL_ERR, "cant gen key[%d]", i);
			err = -ENOMEM;
			goto cleanup;
		}
		values[i] = btree_gen_value();
		if (!values[i]) {
			KLOG(KL_ERR, "cant gen value[%d]", i);
			err = -ENOMEM;
			goto cleanup;
		}
	}

	*pkeys = keys;
	*pvalues = values;
	return 0;
cleanup:
	btree_test_free_kvs(keys, values, num_keys);
	return err;
}

int btree_test(int num_keys)
{
	struct btree *t = NULL;
	struct btree_key **keys = NULL;
	struct btree_value **values = NULL;
	int err;

	err = btree_test_gen_kvs(&keys, &values, num_keys);
	if (err)
		goto cleanup;

	t = btree_create();
	if (!t) {
		KLOG(KL_ERR, "cant create tree");
		err = -ENOMEM;
		goto cleanup;
	}
	KLOG(KL_TST, "t=%p, root=%p sz=%zu",
			t, t->root, sizeof(*t->root));

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* insert keys */
	err = btree_test_insert_case(t, keys, values, num_keys, 0, 0);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}


	/* output tree structures and stats */
	err = btree_test_stats_case(t);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* insert keys again */
	err = btree_test_insert_case(t, keys, values, num_keys, 0, 1);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}


	/* output tree structures and stats */
	err = btree_test_stats_case(t);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* replace keys */
	err = btree_test_insert_case(t, keys, values, num_keys, 1, 0);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* output tree structures and stats */
	err = btree_test_stats_case(t);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* find keys */
	err = btree_test_find_case(t, keys, values, num_keys);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* output tree structures and stats */
	err = btree_test_stats_case(t);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* delete keys */
	err = btree_test_delete_case(t, keys, values, num_keys);
	if (err)
		goto cleanup;
	
	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	/* output tree structures and stats */
	err = btree_test_stats_case(t);
	if (err)
		goto cleanup;

	err = btree_check(t);
	if (err) {
		KLOG(KL_ERR, "t=%p check errs=%d", t, err);
		goto cleanup;
	}

	KLOG(KL_TST, "PASSED");

	err = 0;
cleanup:
	if (err)
		KLOG(KL_TST, "FAILED");

	btree_test_free_kvs(keys, values, num_keys);

	if (t)
		btree_delete(t);
	return 0;
}
