#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "btree"

static struct btree_node *btree_node_alloc(void) 
{
	struct btree_node *node;

	node = kmalloc(sizeof(*node), GFP_NOFS);
	if (!node) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}
	memset(node, 0, sizeof(*node));	

	BUG_ON((ARRAY_SIZE(node->keys) + 1) & 1);	
	node->t = (ARRAY_SIZE(node->keys) + 1)/2;
	node->sig1 = BTREE_SIG1;
	node->sig2 = BTREE_SIG2;
	atomic_set(&node->ref, 1);

	KLOG(KL_DBG, "node %p", node);
	return node;
}

static void btree_nodes_remove(struct btree *tree,
	struct btree_node *node);

static void __btree_node_free(struct btree_node *node)
{
	KLOG(KL_DBG, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);
	kfree(node);
}

static void __btree_node_release(struct btree_node *node)
{
	KLOG(KL_DBG, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);	
	btree_nodes_remove(node->tree, node);
	__btree_node_free(node);
}

static void btree_node_ref(struct btree_node *node)
{
	BUG_ON(atomic_read(&node->ref) <= 0);
	atomic_inc(&node->ref);
}

static void btree_node_deref(struct btree_node *node)
{
	BUG_ON(atomic_read(&node->ref) <= 0);
	if (atomic_dec_and_test(&node->ref))
		__btree_node_release(node);
}

static struct btree_node *__btree_nodes_lookup(struct btree *tree,
	u64 block)
{
	struct rb_node *n = tree->nodes.rb_node;
	struct btree_node *found = NULL;

	while (n) {
		struct btree_node *node;
		node = rb_entry(n, struct btree_node, nodes_link);
		if (block < node->block) {
			n = n->rb_left;
		} else if (block > node->block) {
			n = n->rb_right;
		} else {
			found = node;
			break;
		}
	}
	return found;
}

static struct btree_node * btree_nodes_lookup(struct btree *tree,
	u64 block)
{	
	struct btree_node *node;
	read_lock_irq(&tree->nodes_lock);
	node = __btree_nodes_lookup(tree, block);
	read_unlock_irq(&tree->nodes_lock);
	return node;
}

static void btree_nodes_remove(struct btree *tree,
	struct btree_node *node)
{
	struct btree_node *found;
	write_lock_irq(&tree->nodes_lock);
	found = __btree_nodes_lookup(tree, node->block);
	if (found != node)
		BUG();
	rb_erase(&found->nodes_link, &tree->nodes);
	tree->nodes_active--;
	write_unlock_irq(&tree->nodes_lock);
}

static struct btree_node *btree_nodes_insert(struct btree *tree,
	struct btree_node *node)
{
	struct rb_node **p = &tree->nodes.rb_node;
	struct rb_node *parent = NULL;
	struct btree_node *inserted = NULL;

	write_lock_irq(&tree->nodes_lock);
	while (*p) {
		struct btree_node *found;
		parent = *p;
		found = rb_entry(parent, struct btree_node, nodes_link);
		if (node->block < found->block) {
			p = &(*p)->rb_left;
		} else if (node->block > found->block) {
			p = &(*p)->rb_right;
		} else {
			inserted = found;
			break;
		}
	}
	if (!inserted) {
		rb_link_node(&node->nodes_link, parent, p);
		rb_insert_color(&node->nodes_link, &tree->nodes);
		tree->nodes_active++;
		inserted = node;
	}
	btree_node_ref(inserted);
	write_unlock_irq(&tree->nodes_lock);
	return inserted;
}

static void btree_node_by_ondisk(struct btree_node *node,
		struct btree_node_disk *ondisk)
{
	int i;

	memcpy(node->keys, ondisk->keys, sizeof(ondisk->keys));

	for (i = 0; i < ARRAY_SIZE(ondisk->values); i++) {
		node->values[i] = be64_to_cpu(ondisk->values[i]);
	}

	for (i = 0; i < ARRAY_SIZE(ondisk->childs); i++) {
		node->childs[i] = be64_to_cpu(ondisk->childs[i]);
	}

	node->leaf = be32_to_cpu(ondisk->leaf);
	node->nr_keys = be32_to_cpu(ondisk->nr_keys);
	node->sig1 = be32_to_cpu(ondisk->sig1);
	node->sig2 = be32_to_cpu(ondisk->sig2);
}

static void btree_node_to_ondisk(struct btree_node *node,
		struct btree_node_disk *ondisk)
{
	int i;

	memcpy(ondisk->keys, node->keys, sizeof(node->keys));

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		ondisk->values[i] = cpu_to_be64(node->values[i]);
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		ondisk->childs[i] = cpu_to_be64(node->childs[i]);
	}

	ondisk->leaf = cpu_to_be32(node->leaf);
	ondisk->nr_keys = cpu_to_be32(node->nr_keys);
	ondisk->sig1 = cpu_to_be32(node->sig1);
	ondisk->sig2 = cpu_to_be32(node->sig2);
}

static int btree_node_check_sigs(struct btree_node *node)
{
	if (node->sig1 != BTREE_SIG1 || node->sig2 != BTREE_SIG2) {
		KLOG(KL_ERR, "node %p block %llu invalid sig1 %x or sig2 %x",
			node, node->block, node->sig1, node->sig2);
		return -EINVAL;
	}
	return 0;
}

static struct btree_node *btree_node_read(struct btree *tree, u64 block)
{
	struct btree_node *node, *inserted;
	struct buffer_head *bh;

	BUG_ON(sizeof(struct btree_node_disk) > tree->sb->bsize);
	BUG_ON(block == 0 || block >= tree->sb->nr_blocks);

	node = btree_nodes_lookup(tree, block);
	if (!node) {
		node = btree_node_alloc();
		if (!node)
			return NULL;
		node->tree = tree;
		node->block = block;
		inserted = btree_nodes_insert(tree, node);
		if (node != inserted) {
			__btree_node_free(node);
			node = inserted;
		} else {
			btree_node_deref(inserted);
		}
	}

	bh = __bread(tree->sb->bdev, node->block, tree->sb->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read block at %llu", block);
		btree_node_deref(node);
		return NULL;
	}
	btree_node_by_ondisk(node,
		(struct btree_node_disk *)bh->b_data);

	if (btree_node_check_sigs(node)) {
		btree_node_deref(node);
		node = NULL;
	}

	brelse(bh);

	return node;
}

static int btree_node_write(struct btree_node *node)
{
	struct buffer_head *bh;
	int err;

	BUG_ON(btree_node_check_sigs(node));
	BUG_ON(!node->tree);
	BUG_ON(sizeof(struct btree_node_disk) > node->tree->sb->bsize);
	BUG_ON(node->block == 0 || node->block >= node->tree->sb->nr_blocks);

	KLOG(KL_DBG, "node %p block %llu", node, node->block);

	bh = __bread(node->tree->sb->bdev,
			node->block,
			node->tree->sb->bsize);
	if (!bh) {
		KLOG(KL_ERR, "__getlbk for block %llu", node->block);
		return -EIO;
	}

	btree_node_to_ondisk(node,
		(struct btree_node_disk *)bh->b_data);

	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "sync err %d", err);
	}

	brelse(bh);

	return err;
}

static struct btree_node *btree_node_create(struct btree *tree)
{
	struct btree_node *node, *inserted;
	u64 block;
	int err;

	node = btree_node_alloc();
	if (!node)
		return NULL;
		
	err = ds_balloc_block_alloc(tree->sb, &block);
	if (err) {
		KLOG(KL_ERR, "cant alloc block, err=%d", err);
		__btree_node_free(node);
		return NULL;
	}
	node->block = block;
	BUG_ON(node->block == 0);

	node->tree = tree;
	inserted = btree_nodes_insert(tree, node);
	BUG_ON(inserted != node);
	btree_node_deref(inserted);
	btree_node_write(node);
	KLOG(KL_DBG, "node %p created block %llu", node, node->block);

	return node;	
}

static void btree_node_delete(struct btree_node *node)
{	
	KLOG(KL_DBG, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);

	ds_balloc_block_free(node->tree->sb, node->block);
	node->block = 0;
	btree_node_deref(node);
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

u64 btree_gen_value(void)
{
	u64 value;

	if (ds_random_buf_read(&value, sizeof(value), 1)) {
		return -1;
	}

	return value;
}

char *btree_key_hex(struct btree_key *key)
{
	return bytes_hex((char *)key, sizeof(*key));
}

char *btree_value_hex(u64 value)
{
	return bytes_hex((char *)&value, sizeof(value));
}

static void btree_release(struct btree *tree);

void btree_ref(struct btree *tree)
{
	atomic_inc(&tree->ref);
}

void btree_deref(struct btree *tree)
{
	BUG_ON(atomic_read(&tree->ref) <= 0);
	if (atomic_dec_and_test(&tree->ref)) {
		btree_release(tree);
	}
}

u64 btree_get_root_block(struct btree *tree)
{
	u64 block;

	down_write(&tree->rw_lock);
	block = tree->root->block;
	up_write(&tree->rw_lock);

	return block;
}

struct btree *btree_create(struct ds_sb *sb, u64 root_block)
{
	struct btree *tree;
	int err;

	tree = kmalloc(sizeof(*tree), GFP_NOFS);
	if (!tree) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}

	memset(tree, 0, sizeof(*tree));
	atomic_set(&tree->ref, 1);
	init_rwsem(&tree->rw_lock);
	rwlock_init(&tree->nodes_lock);
	tree->nodes = RB_ROOT;
	tree->sb = sb;
	tree->sig1 = BTREE_SIG1;

	if (root_block)
		tree->root = btree_node_read(tree, root_block);
	else
		tree->root = btree_node_create(tree);

	if (!tree->root)
		goto fail;

	if (!root_block) {
		tree->root->leaf = 1;
		err = btree_node_write(tree->root);
		if (err)
			goto fail;
	}

	KLOG(KL_DBG, "tree %p created root %p ref=%d",
		tree, tree->root, atomic_read(&tree->root->ref));

	return tree;
fail:
	btree_deref(tree);
	return NULL;
}

static void btree_release(struct btree *tree)
{
	tree->releasing = 1;
	down_write(&tree->rw_lock);
	up_write(&tree->rw_lock);

	if (tree->root)
		btree_node_deref(tree->root);

	BUG_ON(tree->nodes_active);

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
	u64 *dst,
	u64 *src)
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
	return (ARRAY_SIZE(node->keys) == node->nr_keys) ? 1 : 0;
}

static void btree_node_copy_key_value(struct btree_node *dst, int dst_index,
	struct btree_key *key, u64 *value)
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

	dst->childs[dst_index] = src->childs[src_index];
}

static void btree_node_copy_child_value(struct btree_node *dst, int dst_index,
	struct btree_node *value)
{
	BUG_ON(dst_index < 0 ||
		dst_index >= ARRAY_SIZE(dst->childs));

	dst->childs[dst_index] = value->block;
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
	struct btree_key *key, u64 *value)
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
	memset(&dst->values[dst_index], 0, sizeof(u64));
}

static void btree_node_split_child(struct btree_node *node,
		struct btree_node *child,
		int child_index, struct btree_node *new)
{
	int i;

	BUG_ON(child_index < 0 || child_index > node->nr_keys);
	BUG_ON(!child || !btree_node_is_full(child));	
	BUG_ON(!new);

	KLOG(KL_DBG, "Splitting node [%p %d] child[%d]=[%p %d]",
		node, node->nr_keys, child_index, child, child->nr_keys);

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
	btree_node_put_child_value(node, child_index + 1, new);

	/* move mid key from child to node */
	btree_node_put_kv(node, child_index, child, new->t - 1);
	node->nr_keys++;

	KLOG(KL_DBG, "Splitted node [%p %d] child[%d]=[%p %d] new [%p %d]",
		node, node->nr_keys, child_index, child, child->nr_keys,
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

static int btree_node_insert_nonfull(
	struct btree_node *first,
	struct btree_key *key,
	u64 *value,
	int replace)
{
	int i;
	struct btree_node *node = first;
	
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
				btree_node_write(node);
				if (node != first)
					btree_node_deref(node);
				return 0;
			} else {
				if (node != first)
					btree_node_deref(node);
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
			btree_node_write(node);
			if (node != first)
				btree_node_deref(node);
			return 0;
		} else {
			struct btree_node *child;
			i = btree_node_find_key_index(node, key);
			child = btree_node_read(node->tree, node->childs[i]);
			if (!child) {
				if (node != first)
					btree_node_deref(node);
				return -EIO;
			}

			if (btree_node_is_full(child)) {
				struct btree_node *new;
				new = btree_node_create(node->tree);
				if (!new) {
					if (node != first)
						btree_node_deref(node);
					btree_node_deref(child);
					return -EIO;
				}
				btree_node_split_child(node, child, i, new);
				
				btree_node_write(node);
				btree_node_write(child);
				btree_node_write(new);
				
				btree_node_deref(new);
				btree_node_deref(child);
				continue; /* restart */
			}
			if (node != first)
				btree_node_deref(node);
			node = child;
		}
	}
}

int btree_insert_key(struct btree *tree, struct btree_key *key,
	u64 value,
	int replace)
{
	int rc;

	if (btree_key_is_zero(key)) {
		KLOG(KL_ERR, "key is zero");
		return -1;
	}

	if (tree->releasing)
		return -EAGAIN;

	down_write(&tree->rw_lock);
	if (tree->releasing) {
		up_write(&tree->rw_lock);
		return -EAGAIN;
	}

	if (btree_node_is_full(tree->root)) {
		struct btree_node *new, *new2, *root = tree->root;
		new = btree_node_create(tree);
		if (new == NULL) {
			up_write(&tree->rw_lock);
			return -1;
		}
		new2 = btree_node_create(tree);
		if (new2 == NULL) {
			btree_node_delete(new);
			btree_node_deref(new);	
			up_write(&tree->rw_lock);
			return -1;
		}

		new->leaf = 0;
		new->nr_keys = 0;
		new->childs[0] = root->block;
		btree_node_split_child(new, root, 0, new2);

		btree_node_write(root);
		btree_node_write(new);
		btree_node_write(new2);

		btree_node_deref(root);
		btree_node_deref(new2);

		tree->root = new;
	}

	rc = btree_node_insert_nonfull(tree->root, key, &value, replace);
	up_write(&tree->rw_lock);
	return rc;
}

static struct btree_node *btree_node_find_key(struct btree_node *first,
		struct btree_key *key, int *pindex)
{
	int i;
	struct btree_node *node = first;
	
	while (1) {
		if (node->nr_keys == 0) {
			if (node != first)
				btree_node_deref(node);
			return NULL;
		}

		i = btree_node_find_key_index(node, key);
		if (i < node->nr_keys &&
				btree_cmp_key(key, &node->keys[i]) == 0) {
			*pindex = i;
			if (node == first)
				btree_node_ref(node);
			return node;
		} else if (node->leaf) {
			if (node != first)
				btree_node_deref(node);
			return NULL;
		} else {
			struct btree_node *prev = node;
			node = btree_node_read(node->tree, node->childs[i]);
			BUG_ON(!node);
			if (prev != first)
				btree_node_deref(prev);	
		}
	}
}

int btree_find_key(struct btree *tree,
	struct btree_key *key,
	u64 *pvalue)
{
	struct btree_node *found;
	int index;

	if (btree_key_is_zero(key)) {
		KLOG(KL_ERR, "key is zero");
		return -1;
	}

	if (tree->releasing)
		return -EAGAIN;

	down_read(&tree->rw_lock);
	if (tree->releasing) {
		up_read(&tree->rw_lock);
		return -EAGAIN;
	}

	found = btree_node_find_key(tree->root, key, &index);
	if (found == NULL) {
		up_read(&tree->rw_lock);
		return -1;
	}

	btree_copy_value(pvalue, &found->values[index]);
	btree_node_deref(found);
	up_read(&tree->rw_lock);

	return 0;
}

static void __btree_node_delete_child_index(struct btree_node *node,
		int index)
{
	int i;
	
	/* do shift to the left by one */
	for (i = (index + 1); i < node->nr_keys + 1; i++) {
		node->childs[i-1] = node->childs[i];
	}
	/* zero last slot */
	node->childs[i-1] = 0;	
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
	struct btree_node *curr, *next;

	BUG_ON(node->nr_keys == 0);
	if (node->leaf) {
		*pindex = node->nr_keys - 1;
		btree_node_ref(node);
		return node;
	}

	curr = btree_node_child_balance(node, node->nr_keys);
	BUG_ON(!curr);
	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = curr->nr_keys - 1;
			return curr;
		}
		next = btree_node_child_balance(curr, curr->nr_keys);
		btree_node_deref(curr);
		curr = next;
		BUG_ON(!curr);
	}
}

static struct btree_node *
btree_node_find_right_most(struct btree_node *node, int *pindex)
{
	struct btree_node *curr, *next;

	BUG_ON(node->nr_keys == 0);
	if (node->leaf) {
		*pindex = 0;
		btree_node_ref(node);
		return node;
	}

	curr = btree_node_child_balance(node, 0);
	BUG_ON(!curr);
	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = 0;
			return curr;
		}
		next = btree_node_child_balance(curr, 0);
		btree_node_deref(curr);
		curr = next;
	}
}

static void btree_node_merge(struct btree_node *dst,
	struct btree_node *src, struct btree_key *key,
	u64 *value)
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
	btree_node_write(sib);
	btree_node_write(child);
	btree_node_write(node);
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
		btree_node_write(sib);
		btree_node_write(node);
		btree_node_delete(child);
		merged = sib;
	} else {
		btree_node_merge(child, sib, &node->keys[child_index],
			&node->values[child_index]);
		__btree_node_delete_key_index(node, child_index);
		__btree_node_delete_child_index(node, child_index+1);
		node->nr_keys--;
		btree_node_write(child);
		btree_node_write(node);
		btree_node_delete(sib);
		merged = child;
	}

	if (node->nr_keys == 0) {
		btree_node_copy(node, merged);
		btree_node_write(node);
		btree_node_delete(merged);
		return node;
	} else {
		return merged;
	}
}

static struct btree_node *
btree_node_child_balance(struct btree_node *node,
	int child_index)
{
	struct btree_node *child, *next;

	BUG_ON(node->leaf);
	child = btree_node_read(node->tree, node->childs[child_index]);
	if (!child) {
		KLOG(KL_ERR, "cant read child");
		return NULL;
	}

	KLOG(KL_DBG, "child %p nr_keys %d t %d",
		child, child->nr_keys, child->t);

	if (child->nr_keys < child->t) {
		struct btree_node *left = (child_index > 0) ?
			btree_node_read(node->tree,
				node->childs[child_index-1]) : NULL;
		struct btree_node *right = (child_index < node->nr_keys) ?
			btree_node_read(node->tree,
				node->childs[child_index+1]) : NULL;

		if (left && left->nr_keys >= left->t) {
			btree_node_child_give_key(node, child,
				child_index, left, 1);
			next = child;
		} else if (right && right->nr_keys >= right->t) {
			btree_node_child_give_key(node, child,
				child_index, right, 0);
			next = child;
		} else if (left && left->nr_keys < left->t) {
			next = btree_node_child_merge(node, child,
				child_index, left, 1);
		} else if (right->nr_keys < right->t) {
			next = btree_node_child_merge(node, child,
				child_index, right, 0);
		} else {
			KLOG(KL_ERR, "no way to add key to child=%p",
				child);
			BUG();
			next = NULL;
		}

		if (next != child) {
			btree_node_deref(child);
			btree_node_ref(next);
		}

		if (right)
			btree_node_deref(right);
		if (left)
			btree_node_deref(left);

		child = next;
	}

	return child;
}

static int btree_node_delete_key(struct btree_node *first,
		struct btree_key *key)
{
	struct btree_key key_copy;
	struct btree_node *node = first;
	int i;

restart:
	btree_copy_key(&key_copy, key);
	key = &key_copy;

	if ((i = btree_node_has_key(node, key)) >= 0) {
		if (node->leaf) {
			btree_node_leaf_delete_key(node, key);
			btree_node_write(node);
			if (node != first)
				btree_node_deref(node);
			return 0;
		} else {
			struct btree_node *pre_child = NULL;
			struct btree_node *suc_child = NULL;
			int index;

			index = btree_node_has_key(node, key);
			BUG_ON(index < 0);

			pre_child = btree_node_read(node->tree,
					node->childs[index]);
			suc_child = btree_node_read(node->tree,
					node->childs[index+1]);
			BUG_ON(!pre_child);
			BUG_ON(!suc_child);

			if (pre_child->nr_keys >= pre_child->t) {
				struct btree_node *pre;
				int pre_index;
	
				pre = btree_node_find_left_most(pre_child,
					&pre_index);
				btree_node_copy_kv(node, index,
					pre, pre_index);
				btree_node_write(node);
				if (node != first)
					btree_node_deref(node);
				btree_node_deref(pre_child);
				btree_node_deref(suc_child);
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
				btree_node_write(node);
				if (node != first)
					btree_node_deref(node);
				btree_node_deref(suc_child);
				btree_node_deref(pre_child);
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
				btree_node_write(node);
				btree_node_write(pre_child);
				/* delete suc_child */
				btree_node_delete(suc_child);
				if (node->nr_keys == 0) {
					btree_node_copy(node, pre_child);
					btree_node_delete(pre_child);
					btree_node_write(node);

					btree_node_deref(suc_child);
					btree_node_deref(pre_child);
					pre_child = node;
				} else {
					if (node != first)
						btree_node_deref(node);
					btree_node_deref(suc_child);
					node = pre_child;
				}
				key = &pre_child->keys[key_index];
				goto restart;
			}
		}
	} else {
		if (node->leaf) {
			if (node != first)
				btree_node_deref(node);
			return -1;
		} else {
			struct btree_node *child;
			BUG_ON(btree_node_has_key(node, key) >= 0);
			i = btree_node_find_key_index(node, key);
			child = btree_node_child_balance(node, i);
			if (node != first)
				btree_node_deref(node);
			node = child;
			goto restart;
		}
	}
}

int btree_delete_key(struct btree *tree, struct btree_key *key)
{
	int rc;
	if (btree_key_is_zero(key))
		return -1;
	if (tree->releasing)
		return -EAGAIN;

	down_write(&tree->rw_lock);
	if (tree->releasing) {
		up_write(&tree->rw_lock);
		return -EAGAIN;
	}
	rc = btree_node_delete_key(tree->root, key);
	up_write(&tree->rw_lock);
	return rc;
}

static void btree_log_node(struct btree_node *first, u32 height, int llevel)
{
	struct btree_node *child;
	struct btree_node *node = first;
	int i;

	KLOG(llevel, "node %p nr_keys %d leaf %d height %u",
			node, node->nr_keys, node->leaf, height);

	if (node->nr_keys) {
		for (i = 0; i < node->nr_keys + 1; i++) {
			child = btree_node_read(node->tree,
				node->childs[i]);
			BUG_ON(!child);
			btree_log_node(child, height+1, llevel);
			btree_node_deref(child);
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
			child = btree_node_read(node->tree,
				node->childs[i]);
			BUG_ON(!child);
			btree_node_stats(child, info);
			btree_node_deref(child);
		}
	}
	info->nr_nodes++;
	info->nr_keys += node->nr_keys;
}

void btree_stats(struct btree *tree, struct btree_info *info)
{
	memset(info, 0, sizeof(*info));
	if (tree->releasing)
		return;

	down_read(&tree->rw_lock);
	if (!tree->releasing)
		btree_node_stats(tree->root, info);
	up_read(&tree->rw_lock);
}

void btree_log(struct btree *tree, int llevel)
{
	if (tree->releasing)
		return;

	down_read(&tree->rw_lock);
	if (!tree->releasing)
		btree_log_node(tree->root, 1, llevel);
	up_read(&tree->rw_lock);
}

static int btree_node_check(struct btree_node *first, int root)
{
	int i;
	int errs = 0;
	struct btree_key *prev_key;
	struct btree_node *node = first;

	if (btree_node_check_sigs(node)) {
		KLOG(KL_ERR, "node %p invalid sigs");
		errs++;
	}

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
			if (!node->childs[i]) {
				KLOG(KL_ERR, "node %p zero child %d found",
					node, i);
				errs++;
			}
		}
	}

	if (!node->leaf) {
		if (!root || (node->nr_keys > 0)) {
			if (!node->childs[i]) {
				KLOG(KL_ERR, "node %p zero child %d found",
						node, i);
				errs++;
			}
		}
	}

	if (!node->leaf) {
		for (i = 0; i < node->nr_keys+1; i++) {
			struct btree_node *child;
			child = btree_node_read(node->tree,
				node->childs[i]);
			BUG_ON(!child);
			errs+= btree_node_check(child, 0);
			btree_node_deref(child);
		}
	}

	return errs;
}

int btree_check(struct btree *tree)
{
	int rc;
	if (tree->releasing)
		return -EAGAIN;

	down_read(&tree->rw_lock);
	if (!tree->releasing)
		rc = btree_node_check(tree->root, 1);
	else
		rc = -EAGAIN;

	up_read(&tree->rw_lock);
	return rc;
}
