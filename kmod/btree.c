#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "btree"

static struct kmem_cache *btree_node_cachep;
static struct kmem_cache *btree_cachep;
static struct kmem_cache *btree_key_cachep;
static struct kmem_cache *btree_node_disk_cachep;

static struct btree_node *btree_node_alloc(void) 
{
	struct btree_node *node;

	node = kmem_cache_alloc(btree_node_cachep, GFP_NOIO);
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

	KLOG(KL_DBG1, "node %p", node);
	return node;
}

static void btree_nodes_remove(struct btree *tree,
	struct btree_node *node);

static void __btree_node_free(struct btree_node *node)
{
	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);
	kmem_cache_free(btree_node_cachep, node);
}

static void __btree_node_release(struct btree_node *node)
{
	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);	

	btree_nodes_remove(node->tree, node);
	__btree_node_free(node);
}

static void btree_node_ref(struct btree_node *node)
{
	BUG_ON(atomic_read(&node->ref) <= 0);
	atomic_inc(&node->ref);
}

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
	if (node)
		BTREE_NODE_REF(node);
	read_unlock_irq(&tree->nodes_lock);
	return node;
}

static void btree_nodes_remove(struct btree *tree,
	struct btree_node *node)
{
	struct btree_node *found;
	if (!node->block)
		return;

	write_lock_irq(&tree->nodes_lock);
	found = __btree_nodes_lookup(tree, node->block);
	if (found) {
		BUG_ON(found != node);
		rb_erase(&found->nodes_link, &tree->nodes);
		tree->nodes_active--;
	}
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
	BTREE_NODE_REF(inserted);
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

static void btree_node_on_disk_sum(struct btree_node_disk *on_disk,
	struct sha256_sum *sum)
{
	sha256(on_disk,
		offsetof(struct btree_node_disk, sum), sum, 0);
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
	struct dio_cluster *clu;
	struct sha256_sum sum;
	struct btree_node_disk *on_disk;
	int err;

	BUG_ON(sizeof(struct btree_node_disk) > tree->sb->bsize);
	BUG_ON(block == 0 || block >= tree->sb->nr_blocks);

	node = btree_nodes_lookup(tree, block);
	if (node)
		return node;

	node = btree_node_alloc();
	if (!node)
		return NULL;

	node->tree = tree;
	node->block = block;
	
	clu = dio_clu_get(tree->sb->ddev, node->block);
	if (!clu) {
		KLOG(KL_ERR, "cant read block at %llu", block);
		goto free_node;
	}

	on_disk = kmem_cache_alloc(btree_node_disk_cachep, GFP_NOIO);
	if (!on_disk) {
		KLOG(KL_ERR, "cant alloc on disk");
		goto put_clu;
	}
		
	err = dio_clu_read(clu, on_disk, sizeof(*on_disk), 0);
	if (err) {
		KLOG(KL_ERR, "cant read on_disk err %d", err);
		goto free_ondisk;
	}

	if (be32_to_cpu(on_disk->sig1) != BTREE_SIG1
		|| be32_to_cpu(on_disk->sig2) != BTREE_SIG2) {
		KLOG(KL_ERR, "invalid sig of node %p block %llu",
		node, node->block);
		goto free_ondisk;
	}

	btree_node_on_disk_sum(on_disk, &sum);
	if (0 != memcmp(&sum, &on_disk->sum, sizeof(sum))) {
		KLOG(KL_ERR, "invalid sha256 sum of node %p block %llu",
			node, node->block);
		goto free_ondisk;
	}

	btree_node_by_ondisk(node,
		on_disk);

	kmem_cache_free(btree_node_disk_cachep, on_disk);
	dio_clu_put(clu);

	BUG_ON(btree_node_check_sigs(node));

	inserted = btree_nodes_insert(tree, node);
	if (node != inserted) {
		__btree_node_free(node);
		node = inserted;
	} else {
		BTREE_NODE_DEREF(inserted);
	}

	return node;

free_ondisk:
	kmem_cache_free(btree_node_disk_cachep, on_disk);
put_clu:
	dio_clu_put(clu);
free_node:
	__btree_node_free(node);

	return NULL;	
}

static int btree_node_write(struct btree_node *node)
{
	struct dio_cluster *clu;
	int err;
	struct btree_node_disk *on_disk;

	BUG_ON(btree_node_check_sigs(node));
	BUG_ON(!node->tree);
	BUG_ON(sizeof(struct btree_node_disk) > node->tree->sb->bsize);
	BUG_ON(node->block == 0 || node->block >= node->tree->sb->nr_blocks);

	KLOG(KL_DBG1, "node %p block %llu", node, node->block);

	clu = dio_clu_get(node->tree->sb->ddev, node->block);
	if (!clu) {
		KLOG(KL_ERR, "cant get clu for block %llu", node->block);
		return -EIO;
	}

	on_disk = kmem_cache_alloc(btree_node_disk_cachep, GFP_NOIO);
	if (!on_disk) {
		err = -ENOMEM;
		goto clu_put;
	}

	btree_node_to_ondisk(node, on_disk);
	btree_node_on_disk_sum(on_disk, &on_disk->sum);

	err = dio_clu_write(clu, on_disk, sizeof(*on_disk), 0);
	if (err) {
		KLOG(KL_ERR, "cant write err %d", err);
		goto free_on_disk;
	}

	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "sync err %d", err);
		goto free_on_disk;
	}

free_on_disk:
	kmem_cache_free(btree_node_disk_cachep, on_disk);
clu_put:
	dio_clu_put(clu);

	return err;
}

static void btree_node_delete(struct btree_node *node)
{	
	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d block %llu",
		node, node->leaf, node->nr_keys, node->block);

	btree_nodes_remove(node->tree, node);
	ds_balloc_block_free(node->tree->sb, node->block);
	node->block = 0;
}

void btree_key_by_u64(u64 val, struct ds_obj_id *key)
{
	BUG_ON(sizeof(val) > sizeof(*key));
	memset(key, 0, sizeof(*key));
	memcpy(key, &val, sizeof(val));
}

u64 btree_key_to_u64(struct ds_obj_id *key)
{
	u64 val;
	memcpy(&val, key, sizeof(val));
	return val;
}

static struct btree_node *btree_node_create(struct btree *tree)
{
	struct btree_node *node, *inserted;
	int err;

	node = btree_node_alloc();
	if (!node)
		return NULL;
		
	err = ds_balloc_block_alloc(tree->sb, &node->block);
	if (err) {
		KLOG(KL_ERR, "cant alloc block, err=%d", err);
		__btree_node_free(node);
		return NULL;
	}
	node->tree = tree;
	err = btree_node_write(node);
	if (err) {
		KLOG(KL_ERR, "cant write node at %llu, err=%d",
			node->block, err);
		btree_node_delete(node);
		__btree_node_free(node);
		return NULL;
	}

	inserted = btree_nodes_insert(tree, node);
	if (inserted != node) {
		btree_node_delete(node);
		__btree_node_free(node);	
		node = inserted;
		KLOG(KL_DBG1, "node %p found block %llu", node, node->block);	
	} else {
		BTREE_NODE_DEREF(inserted);
		KLOG(KL_DBG1, "node %p created block %llu", node, node->block);
	}

	return node;	
}

void btree_key_fee(struct ds_obj_id *key)
{
	kmem_cache_free(btree_key_cachep, key);
}

struct ds_obj_id *btree_gen_key(void)
{
	struct ds_obj_id *key;
	key = kmem_cache_alloc(btree_key_cachep, GFP_NOIO);
	if (!key)
		return NULL;

	if (crt_random_buf(key, sizeof(*key))) {
		kmem_cache_free(btree_key_cachep, key);
		return NULL;
	}
	return key;
}

u64 btree_gen_value(void)
{
	u64 value;

	if (crt_random_buf(&value, sizeof(value))) {
		return -1;
	}

	return value;
}

char *btree_key_hex(struct ds_obj_id *key)
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

u64 btree_root_block(struct btree *tree)
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

	tree = kmem_cache_alloc(btree_cachep, GFP_NOIO);
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

	KLOG(KL_DBG1, "tree %p created root %p ref=%d",
		tree, tree->root, atomic_read(&tree->root->ref));

	return tree;
fail:
	btree_deref(tree);
	return NULL;
}

void btree_stop(struct btree *tree)
{
	tree->releasing = 1;
	down_write(&tree->rw_lock);
	up_write(&tree->rw_lock);
}

static void btree_release(struct btree *tree)
{
	btree_stop(tree);
	if (tree->root)
		BTREE_NODE_DEREF(tree->root);

	KLOG(KL_DBG1, "tree %p nodes_active %d root %p",
		tree, tree->nodes_active,
		rb_entry(tree->nodes.rb_node, struct btree_node, nodes_link));

	DS_BUG_ON(tree->nodes_active);

	kmem_cache_free(btree_cachep, tree);
	KLOG(KL_DBG1, "tree %p deleted", tree);
}

static int btree_cmp_key(
	struct ds_obj_id *key1,
	struct ds_obj_id *key2)
{
	return ds_obj_id_cmp(key1, key2);
}

static void btree_copy_key(
	struct ds_obj_id *dst,
	struct ds_obj_id *src)
{
	ds_obj_id_copy(dst, src);
}

static void btree_copy_value(
	u64 *dst,
	u64 *src)
{
	memcpy(dst, src, sizeof(*dst));
}

static int btree_node_is_full(struct btree_node *node)
{
	return (ARRAY_SIZE(node->keys) == node->nr_keys) ? 1 : 0;
}

static void btree_node_copy_key_value(struct btree_node *dst, int dst_index,
	struct ds_obj_id *key, u64 *value)
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
	struct ds_obj_id *key, u64 *value)
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

	memset(&dst->keys[dst_index], 0, sizeof(struct ds_obj_id));
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

	KLOG(KL_DBG1, "Splitting node [%p %d] child[%d]=[%p %d]",
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

	KLOG(KL_DBG1, "Splitted node [%p %d] child[%d]=[%p %d] new [%p %d]",
		node, node->nr_keys, child_index, child, child->nr_keys,
		new, new->nr_keys);
}

static int btree_node_key_probably_inside(struct btree_node *node, struct ds_obj_id *key)
{
	if (0 == node->nr_keys)
		return 0;
	else if ((btree_cmp_key(key, &node->keys[node->nr_keys-1]) > 0)
		|| (btree_cmp_key(key, &node->keys[0]) < 0))
		return 0;
	return 1;
}

static int btree_node_has_key(struct btree_node *node,
	struct ds_obj_id *key)
{
	u32 start = 0;
	u32 end = node->nr_keys;
	u32 mid;
	int cmp;

	if (!btree_node_key_probably_inside(node, key));
		return -1;

	while (start < end) {
		mid = (start + end) / 2;

		cmp = btree_cmp_key(key, &node->keys[mid]);

		if (!cmp)
			return mid;
		else if (cmp < 0)
			end = mid;
		else
			start = mid + 1;
	}

	return -1;
}

static int btree_node_find_key_index(struct btree_node *node,
	struct ds_obj_id *key)
{
	u32 start = 0;
	u32 end = node->nr_keys;
	u32 mid;
	int cmp;

	if (0 == node->nr_keys)
		return 0; 
	else if (btree_cmp_key(key, &node->keys[end-1]) > 0)
		return end;
	else if (btree_cmp_key(key, &node->keys[start]) < 0)
		return 0;

	while (start < end) {
		mid = (start + end) / 2;

		cmp = btree_cmp_key(key, &node->keys[mid]);

		if (!cmp)
			return mid;
		else if (cmp < 0)
			end = mid;
		else
			start = mid + 1;
	}

	return end;
}

static int btree_node_insert_nonfull(
	struct btree_node *first,
	struct ds_obj_id *key,
	u64 *value,
	int replace)
{
	int i;
	struct btree_node *node = first;
	
	while (1) {
		KLOG(KL_DBG1, "node [%p %d] leaf %d",
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
					BTREE_NODE_DEREF(node);
				return 0;
			} else {
				if (node != first)
					BTREE_NODE_DEREF(node);
				return -EEXIST;
			}
		}

		if (node->leaf) {
			/* key doesnt exists so place key value in sorted
			 * order
			 */
			i = btree_node_find_key_index(node, key);
			btree_node_put_key_value(node, i, key, value);
			node->nr_keys++;
			KLOG(KL_DBG1, "inserted key into node=%p nr_keys=%d",
					node, node->nr_keys);
			btree_node_write(node);
			if (node != first)
				BTREE_NODE_DEREF(node);
			return 0;
		} else {
			struct btree_node *child;
			i = btree_node_find_key_index(node, key);
			child = btree_node_read(node->tree, node->childs[i]);
			if (!child) {
				if (node != first)
					BTREE_NODE_DEREF(node);
				return -EIO;
			}

			if (btree_node_is_full(child)) {
				struct btree_node *new;
				new = btree_node_create(node->tree);
				if (!new) {
					if (node != first)
						BTREE_NODE_DEREF(node);
					BTREE_NODE_DEREF(child);
					return -EIO;
				}
				btree_node_split_child(node, child, i, new);
				
				btree_node_write(node);
				btree_node_write(child);
				btree_node_write(new);
				
				BTREE_NODE_DEREF(new);
				BTREE_NODE_DEREF(child);
				continue; /* restart */
			}
			if (node != first)
				BTREE_NODE_DEREF(node);
			node = child;
		}
	}
}

static void btree_node_copy(struct btree_node *dst, struct btree_node *src);

int btree_insert_key(struct btree *tree, struct ds_obj_id *key,
	u64 value,
	int replace)
{
	int rc;

	if (tree->releasing)
		return -EAGAIN;

	down_write(&tree->rw_lock);
	if (tree->releasing) {
		up_write(&tree->rw_lock);
		return -EAGAIN;
	}

	if (btree_node_is_full(tree->root)) {
		struct btree_node *new, *new2, *root = tree->root, *clone;

		clone = btree_node_create(tree);
		if (clone == NULL) {
			rc = -ENOMEM;
			goto out;
		}

		new = btree_node_alloc();
		if (new == NULL) {
			btree_node_delete(clone);
			BTREE_NODE_DEREF(clone);
			rc = -ENOMEM;
			goto out;
		}

		new2 = btree_node_create(tree);
		if (new2 == NULL) {
			btree_node_delete(clone);
			BTREE_NODE_DEREF(clone);
			__btree_node_free(new);
			rc = -ENOMEM;
			goto out;
		}

		btree_node_copy(clone, root);

		new->leaf = 0;
		new->nr_keys = 0;
		new->childs[0] = clone->block;
		btree_node_split_child(new, clone, 0, new2);
		
		btree_node_copy(root, new);
		
		btree_node_write(clone);
		btree_node_write(new2);
		btree_node_write(root);
	
		BTREE_NODE_DEREF(clone);
		BTREE_NODE_DEREF(new2);
		__btree_node_free(new);
	}

	rc = btree_node_insert_nonfull(tree->root, key, &value, replace);
out:
	up_write(&tree->rw_lock);
	return rc;
}

static struct btree_node *btree_node_find_key(struct btree_node *first,
		struct ds_obj_id *key, int *pindex)
{
	int i;
	struct btree_node *node = first;
	
	while (1) {
		if (node->nr_keys == 0) {
			if (node != first)
				BTREE_NODE_DEREF(node);
			return NULL;
		}

		i = btree_node_find_key_index(node, key);
		if (i < node->nr_keys &&
				btree_cmp_key(key, &node->keys[i]) == 0) {
			*pindex = i;
			if (node == first)
				BTREE_NODE_REF(node);
			return node;
		} else if (node->leaf) {
			if (node != first)
				BTREE_NODE_DEREF(node);
			return NULL;
		} else {
			struct btree_node *prev = node;
			node = btree_node_read(node->tree, node->childs[i]);
			BUG_ON(!node);
			if (prev != first)
				BTREE_NODE_DEREF(prev);	
		}
	}
}

int btree_find_key(struct btree *tree,
	struct ds_obj_id *key,
	u64 *pvalue)
{
	struct btree_node *found;
	int index;

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
		return -ENOENT;
	}

	btree_copy_value(pvalue, &found->values[index]);
	BTREE_NODE_DEREF(found);
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
		struct ds_obj_id *key)
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
		BTREE_NODE_REF(node);
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
		BTREE_NODE_DEREF(curr);
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
		BTREE_NODE_REF(node);
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
		BTREE_NODE_DEREF(curr);
		curr = next;
	}
}

static void btree_node_merge(struct btree_node *dst,
	struct btree_node *src, struct ds_obj_id *key,
	u64 *value)
{
	int i, pos;

	KLOG(KL_DBG1, "Merging %p %d -> %p %d",
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

	KLOG(KL_DBG1, "Merged %p -> %p nr_keys %d",
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

	KLOG(KL_DBG1, "child %p nr_keys %d t %d",
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
			BTREE_NODE_DEREF(child);
			BTREE_NODE_REF(next);
		}

		if (right)
			BTREE_NODE_DEREF(right);
		if (left)
			BTREE_NODE_DEREF(left);

		child = next;
	}

	if (child == node)
		BTREE_NODE_DEREF(child);

	return child;
}

static int btree_node_delete_key(struct btree_node *first,
		struct ds_obj_id *key)
{
	struct ds_obj_id key_copy;
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
				BTREE_NODE_DEREF(node);
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
					BTREE_NODE_DEREF(node);
				BTREE_NODE_DEREF(pre_child);
				BTREE_NODE_DEREF(suc_child);
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
					BTREE_NODE_DEREF(node);
				BTREE_NODE_DEREF(suc_child);
				BTREE_NODE_DEREF(pre_child);
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

					BTREE_NODE_DEREF(suc_child);
					BTREE_NODE_DEREF(pre_child);
					pre_child = node;
				} else {
					if (node != first)
						BTREE_NODE_DEREF(node);
					BTREE_NODE_DEREF(suc_child);
					node = pre_child;
				}
				key = &pre_child->keys[key_index];
				goto restart;
			}
		}
	} else {
		if (node->leaf) {
			if (node != first)
				BTREE_NODE_DEREF(node);
			return -ENOENT;
		} else {
			struct btree_node *child;
			BUG_ON(btree_node_has_key(node, key) >= 0);
			i = btree_node_find_key_index(node, key);
			child = btree_node_child_balance(node, i);
			if (node != first)
				BTREE_NODE_DEREF(node);
			node = child;
			goto restart;
		}
	}
}

int btree_delete_key(struct btree *tree, struct ds_obj_id *key)
{
	int rc;

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
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = btree_node_read(node->tree,
					node->childs[i]);
				BUG_ON(!child);
				btree_log_node(child, height+1, llevel);
				BTREE_NODE_DEREF(child);
			}
		}
	}
}

static void btree_node_stats(struct btree_node *node,
	struct btree_info *info)
{
	struct btree_node *child;
	int i;

	if (node->nr_keys) {
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = btree_node_read(node->tree,
					node->childs[i]);
				BUG_ON(!child);
				btree_node_stats(child, info);
				BTREE_NODE_DEREF(child);
			}
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

static void btree_erase_node(struct btree_node *root,
	btree_key_erase_clb_t key_erase_clb,
	void *ctx)
{
	struct btree_node *child;
	struct btree_node *node = root;
	int i;

	if (node->nr_keys) {
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = btree_node_read(node->tree,
					node->childs[i]);
				BUG_ON(!child);
				btree_erase_node(child, key_erase_clb, ctx);
				BTREE_NODE_DEREF(child);
			}
		}
		if (key_erase_clb) {
			for (i = 0; i < node->nr_keys; i++) {
				key_erase_clb(&node->keys[i],
					node->values[i],
					ctx);
			}
		}
	}
	btree_node_delete(node);
}

void btree_erase(struct btree *tree,
	btree_key_erase_clb_t key_erase_clb,
	void *ctx)
{
	down_write(&tree->rw_lock);
	tree->releasing = 1;
	btree_erase_node(tree->root, key_erase_clb, ctx);
	up_write(&tree->rw_lock);
}

static int btree_node_check(struct btree_node *first, int root)
{
	int i;
	int errs = 0;
	struct ds_obj_id *prev_key;
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
		if (prev_key && (btree_cmp_key(prev_key,
			&node->keys[i]) >= 0)) {
			KLOG(KL_ERR, "node %p key %d not sorted",
				node, i);
			errs++;
		}
		prev_key = &node->keys[i];
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
			BTREE_NODE_DEREF(child);
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
	if (!tree->releasing) {
		rc = btree_node_check(tree->root, 1);
		KLOG(KL_INF, "tree %p check rc %d", tree, rc);
	} else
		rc = -EAGAIN;
	up_read(&tree->rw_lock);
	return rc;
}

int btree_init(void)
{
	int err;

	btree_node_cachep = kmem_cache_create("btree_node_cache",
			sizeof(struct btree_node), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!btree_node_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	btree_key_cachep = kmem_cache_create("btree_key_cache",
			sizeof(struct ds_obj_id), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!btree_key_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_node_cache;
	}

	btree_cachep = kmem_cache_create("btree_cache",
			sizeof(struct btree), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!btree_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_key_cache;
	}
	
	btree_node_disk_cachep = kmem_cache_create("btree_node_disk_cache",
			sizeof(struct btree_node_disk), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!btree_node_disk_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_btree_cache;
	}

	return 0;

del_btree_cache:
	kmem_cache_destroy(btree_cachep);
del_key_cache:
	kmem_cache_destroy(btree_key_cachep);
del_node_cache:
	kmem_cache_destroy(btree_node_cachep);
out:
	return err;
}

void btree_finit(void)
{
	kmem_cache_destroy(btree_node_disk_cachep);
	kmem_cache_destroy(btree_node_cachep);
	kmem_cache_destroy(btree_key_cachep);
	kmem_cache_destroy(btree_cachep);
}
