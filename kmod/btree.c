#include <inc/nkfs_priv.h>

#define __SUBCOMPONENT__ "nkfs_btree"

static struct kmem_cache *nkfs_btree_node_cachep;
static struct kmem_cache *nkfs_btree_cachep;

static void nkfs_btree_nodes_remove(struct nkfs_btree *tree,
	struct nkfs_btree_node *node);

static void __nkfs_btree_node_free(struct nkfs_btree_node *node)
{
	int i;

	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);

	for (i = 0; i < ARRAY_SIZE(node->keys); i++) {
		if (node->keys[i])
			put_page(node->keys[i]);
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		if (node->childs[i])
			put_page(node->childs[i]);
	}

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		if (node->values[i])
			put_page(node->values[i]);
	}

	if (node->header)
		put_page(node->header);

	kmem_cache_free(nkfs_btree_node_cachep, node);
}

struct nkfs_btree_key *nkfs_btree_node_key(struct nkfs_btree_node *node, int index)
{
	int pg_idx;
	int pg_off;

	BUG_ON(index < 0);

	pg_idx = (index*sizeof(struct nkfs_btree_key))/sizeof(struct nkfs_btree_key_page);
	pg_off = (index*sizeof(struct nkfs_btree_key)) &
			(sizeof(struct nkfs_btree_key_page) - 1);

	BUG_ON(pg_idx >= ARRAY_SIZE(node->keys));
	return (struct nkfs_btree_key *)((char *)page_address(node->keys[pg_idx])
		+ pg_off);
}

static struct nkfs_btree_child *nkfs_btree_node_child(struct nkfs_btree_node *node, int index)
{
	int pg_idx;
	int pg_off;

	BUG_ON(index < 0);

	pg_idx = (index*sizeof(struct nkfs_btree_child))/sizeof(struct nkfs_btree_child_page);
	pg_off = (index*sizeof(struct nkfs_btree_child)) &
			(sizeof(struct nkfs_btree_child_page) - 1);

	BUG_ON(pg_idx >= ARRAY_SIZE(node->childs));
	return (struct nkfs_btree_child *)((char *)page_address(node->childs[pg_idx])
		+ pg_off);
}

struct nkfs_btree_value *nkfs_btree_node_value(struct nkfs_btree_node *node, int index)
{
	int pg_idx;
	int pg_off;

	BUG_ON(index < 0);

	pg_idx = (index*sizeof(struct nkfs_btree_value))/sizeof(struct nkfs_btree_value_page);
	pg_off = (index*sizeof(struct nkfs_btree_value)) &
			(sizeof(struct nkfs_btree_value_page) - 1);

	BUG_ON(pg_idx >= ARRAY_SIZE(node->values));
	return (struct nkfs_btree_value *)((char *)page_address(node->values[pg_idx])
			+ pg_off);
}

static void nkfs_btree_node_zero_pages(struct nkfs_btree_node *node)
{
	int i;

	memset(page_address(node->header), 0, PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(node->keys); i++)
		memset(page_address(node->keys[i]), 0, PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(node->childs); i++)
		memset(page_address(node->childs[i]), 0, PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(node->values); i++)
		memset(page_address(node->values[i]), 0, PAGE_SIZE);
}

static struct nkfs_btree_node *nkfs_btree_node_alloc(int zero_pages)
{
	struct nkfs_btree_node *node;
	int i;

	node = kmem_cache_alloc(nkfs_btree_node_cachep, GFP_NOIO);
	if (!node) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}

	memset(node, 0, sizeof(*node));	

	node->header = alloc_page(GFP_KERNEL);
	if (!node->header)
		goto fail;

	for (i = 0; i < ARRAY_SIZE(node->keys); i++) {
		node->keys[i] = alloc_page(GFP_KERNEL);
		if (!node->keys[i])
			goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		node->childs[i] = alloc_page(GFP_KERNEL);
		if (!node->childs[i])
			goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		node->values[i] = alloc_page(GFP_KERNEL);
		if (!node->values[i])
			goto fail;
	}

	if (zero_pages)
		nkfs_btree_node_zero_pages(node);

	node->t = NKFS_BTREE_T;
	node->sig1 = NKFS_BTREE_SIG1;
	node->sig2 = NKFS_BTREE_SIG2;

	atomic_set(&node->ref, 1);

	KLOG(KL_DBG1, "node %p", node);
	return node;
fail:
	__nkfs_btree_node_free(node);
	return NULL;
}

static void __nkfs_btree_node_release(struct nkfs_btree_node *node)
{
	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d",
		node, node->leaf, node->nr_keys);	

	nkfs_btree_nodes_remove(node->tree, node);
	__nkfs_btree_node_free(node);
}

void nkfs_btree_node_ref(struct nkfs_btree_node *node)
{
	BUG_ON(atomic_read(&node->ref) <= 0);
	atomic_inc(&node->ref);
}

void nkfs_btree_node_deref(struct nkfs_btree_node *node)
{
	BUG_ON(atomic_read(&node->ref) <= 0);
	if (atomic_dec_and_test(&node->ref))
		__nkfs_btree_node_release(node);
}

static struct nkfs_btree_node *__nkfs_btree_nodes_lookup(struct nkfs_btree *tree,
	u64 block)
{
	struct rb_node *n = tree->nodes.rb_node;
	struct nkfs_btree_node *found = NULL;

	while (n) {
		struct nkfs_btree_node *node;
		node = rb_entry(n, struct nkfs_btree_node, nodes_link);
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

static struct nkfs_btree_node * nkfs_btree_nodes_lookup(struct nkfs_btree *tree,
	u64 block)
{	
	struct nkfs_btree_node *node;
	read_lock_irq(&tree->nodes_lock);
	node = __nkfs_btree_nodes_lookup(tree, block);
	if (node)
		NKFS_BTREE_NODE_REF(node);
	read_unlock_irq(&tree->nodes_lock);
	return node;
}

static void nkfs_btree_nodes_remove(struct nkfs_btree *tree,
	struct nkfs_btree_node *node)
{
	struct nkfs_btree_node *found;
	if (!node->block)
		return;

	write_lock_irq(&tree->nodes_lock);
	found = __nkfs_btree_nodes_lookup(tree, node->block);
	if (found) {
		BUG_ON(found != node);
		rb_erase(&found->nodes_link, &tree->nodes);
		tree->nodes_active--;
	}
	write_unlock_irq(&tree->nodes_lock);
}

static struct nkfs_btree_node *nkfs_btree_nodes_insert(struct nkfs_btree *tree,
	struct nkfs_btree_node *node)
{
	struct rb_node **p = &tree->nodes.rb_node;
	struct rb_node *parent = NULL;
	struct nkfs_btree_node *inserted = NULL;

	write_lock_irq(&tree->nodes_lock);
	while (*p) {
		struct nkfs_btree_node *found;
		parent = *p;
		found = rb_entry(parent, struct nkfs_btree_node, nodes_link);
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
	NKFS_BTREE_NODE_REF(inserted);
	write_unlock_irq(&tree->nodes_lock);
	return inserted;
}

static void nkfs_btree_node_child_page_by_ondisk(
	struct nkfs_btree_child_page	*dst,
	struct nkfs_btree_child_page *src)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(src->childs); i++)
		dst->childs[i].val = be64_to_cpu(src->childs[i].val_be);
}

static void nkfs_btree_node_value_page_by_ondisk(
	struct nkfs_btree_value_page	*dst,
	struct nkfs_btree_value_page *src)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(src->values); i++)
		dst->values[i].val = be64_to_cpu(src->values[i].val_be);
}

static void nkfs_btree_node_child_page_to_ondisk(
	struct nkfs_btree_child_page	*dst,
	struct nkfs_btree_child_page *src)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(src->childs); i++)
		dst->childs[i].val_be = cpu_to_be64(src->childs[i].val);
}

static void nkfs_btree_node_value_page_to_ondisk(
	struct nkfs_btree_value_page	*dst,
	struct nkfs_btree_value_page *src)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(src->values); i++)
		dst->values[i].val_be = cpu_to_be64(src->values[i].val);
}

static void nkfs_btree_node_header_page_to_ondisk(
	struct nkfs_btree_header_page *dst,
	struct nkfs_btree_header_page *src)
{
	memcpy(dst, src, sizeof(*src));
	dst->sig1 = cpu_to_be32(src->sig1);
	dst->leaf = cpu_to_be32(src->leaf);
	dst->nr_keys = cpu_to_be32(src->nr_keys);
	dst->sig2 = cpu_to_be32(src->sig2);
}

static void nkfs_btree_node_header_page_by_ondisk(
	struct nkfs_btree_header_page *dst,
	struct nkfs_btree_header_page *src)
{
	memcpy(dst, src, sizeof(*src));
	dst->sig1 = be32_to_cpu(src->sig1);
	dst->leaf = be32_to_cpu(src->leaf);
	dst->nr_keys = be32_to_cpu(src->nr_keys);
	dst->sig2 = be32_to_cpu(src->sig2);
}

static void nkfs_btree_node_key_page_copy(
	struct nkfs_btree_key_page *dst,
	struct nkfs_btree_key_page *src)
{
	memcpy(dst, src, sizeof(*src));
}

static void nkfs_btree_node_by_ondisk(struct nkfs_btree_node *node,
			struct dio_cluster *clu)
{
	int i;

	nkfs_btree_node_header_page_by_ondisk(
		page_address(node->header),
		(struct nkfs_btree_header_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->header));

	for (i = 0; i < ARRAY_SIZE(node->keys); i++) {
		nkfs_btree_node_key_page_copy(
		(struct nkfs_btree_key_page *)page_address(node->keys[i]),
		(struct nkfs_btree_key_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->keys[i]));
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		nkfs_btree_node_child_page_by_ondisk(
		(struct nkfs_btree_child_page *)page_address(node->childs[i]),
		(struct nkfs_btree_child_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->childs[i]));
	}

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		nkfs_btree_node_value_page_by_ondisk(
		(struct nkfs_btree_value_page *)page_address(node->values[i]),
		(struct nkfs_btree_value_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->values[i]));
	}
}

static void nkfs_btree_node_to_ondisk(struct nkfs_btree_node *node,
			struct dio_cluster *clu)
{
	int i;

	nkfs_btree_node_header_page_to_ondisk(
		(struct nkfs_btree_header_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->header),
		(struct nkfs_btree_header_page *)page_address(node->header));

	for (i = 0; i < ARRAY_SIZE(node->keys); i++) {
		nkfs_btree_node_key_page_copy(
		(struct nkfs_btree_key_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->keys[i]),
		(struct nkfs_btree_key_page *)page_address(node->keys[i]));
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		nkfs_btree_node_child_page_to_ondisk(
		(struct nkfs_btree_child_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->childs[i]),
		(struct nkfs_btree_child_page *)page_address(node->childs[i]));
	}

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		nkfs_btree_node_value_page_to_ondisk(
		(struct nkfs_btree_value_page *)dio_clu_map(clu,
			(unsigned long)&((struct nkfs_btree_node_disk *)0)->values[i]),
		(struct nkfs_btree_value_page *)page_address(node->values[i]));
	}
}

static struct csum *nkfs_btree_node_map_sum(struct nkfs_btree_node *node)
{
	struct nkfs_btree_header_page *header;
	header = page_address(node->header);
	return &header->sum;
}

static void nkfs_btree_node_calc_sum(struct nkfs_btree_node *node,
	struct csum *sum, int write)
{
	struct csum_ctx ctx;
	struct nkfs_btree_header_page *header;
	int i;

	header = page_address(node->header);
	KLOG(KL_DBG3, "node %llu leaf %d nr_keys %d sig1 %x sig2 %x",
		node->block, header->leaf, header->nr_keys,
		header->sig1, header->sig2);
	
	csum_reset(&ctx);

	KLOG_BUF_SUM(KL_DBG3, page_address(node->header),
		offsetof(struct nkfs_btree_header_page, sum));

	csum_update(&ctx, page_address(node->header),
		offsetof(struct nkfs_btree_header_page, sum));

	for (i = 0; i < ARRAY_SIZE(node->keys); i++) {
		csum_update(&ctx, page_address(node->keys[i]), PAGE_SIZE);
		KLOG_BUF_SUM(KL_DBG3, page_address(node->keys[i]), PAGE_SIZE);
	}

	for (i = 0; i < ARRAY_SIZE(node->childs); i++) {
		csum_update(&ctx, page_address(node->childs[i]), PAGE_SIZE);
		KLOG_BUF_SUM(KL_DBG3, page_address(node->childs[i]), PAGE_SIZE);
	}

	for (i = 0; i < ARRAY_SIZE(node->values); i++) {
		csum_update(&ctx, page_address(node->values[i]), PAGE_SIZE);
		KLOG_BUF_SUM(KL_DBG3, page_address(node->values[i]), PAGE_SIZE);
	}

	csum_digest(&ctx, sum);
	KLOG(KL_DBG3, "node block %llu sum %llx write %d nr_keys %d leaf %d",
			node->block, csum_u64(sum), write,
			node->nr_keys, node->leaf);
}

static struct nkfs_btree_node *nkfs_btree_node_read(struct nkfs_btree *tree, u64 block)
{
	struct nkfs_btree_node *node, *inserted;
	struct dio_cluster *clu;
	struct csum sum;
	struct nkfs_btree_header_page *header;

	BUG_ON(sizeof(struct nkfs_btree_node_disk) > tree->sb->bsize);
	BUG_ON(block == 0 || block >= tree->sb->nr_blocks);

	node = nkfs_btree_nodes_lookup(tree, block);
	if (node)
		return node;

	node = nkfs_btree_node_alloc(0);
	if (!node)
		return NULL;

	node->tree = tree;
	node->block = block;
	
	clu = dio_clu_get(tree->sb->ddev, node->block);
	if (!clu) {
		KLOG(KL_ERR, "cant read block at %llu", block);
		goto free_node;
	}
	nkfs_btree_node_by_ondisk(node, clu);

	header = page_address(node->header);
	node->sig1 = header->sig1;
	node->sig2 = header->sig2;
	node->leaf = header->leaf;
	node->nr_keys = header->nr_keys;
	KLOG(KL_DBG3, "node %llu nr_keys %d",
		node->block, node->nr_keys);

	if (node->sig1 != NKFS_BTREE_SIG1
		|| node->sig2 != NKFS_BTREE_SIG2) {
		KLOG(KL_ERR, "invalid sig of node %p block %llu",
		node, node->block);
		goto put_clu;
	}

	nkfs_btree_node_calc_sum(node, &sum, 0);
	if (0 != memcmp(&sum, nkfs_btree_node_map_sum(node), sizeof(sum))) {
		KLOG(KL_ERR, "node %p block %llu csum %llx vs. sum %llx",
			node, node->block, csum_u64(&sum),
			csum_u64(nkfs_btree_node_map_sum(node)));
		goto put_clu;
	}

	dio_clu_put(clu);

	inserted = nkfs_btree_nodes_insert(tree, node);
	if (node != inserted) {
		__nkfs_btree_node_free(node);
		node = inserted;
	} else {
		NKFS_BTREE_NODE_DEREF(inserted);
	}

	return node;

put_clu:
	dio_clu_put(clu);
free_node:
	__nkfs_btree_node_free(node);

	return NULL;	
}

int nkfs_btree_node_write(struct nkfs_btree_node *node)
{
	struct dio_cluster *clu;
	struct nkfs_btree_header_page *header;
	int err;

	BUG_ON(node->sig1 != NKFS_BTREE_SIG1 || node->sig2 != NKFS_BTREE_SIG2);
	BUG_ON(!node->tree);
	BUG_ON(sizeof(struct nkfs_btree_node_disk) > node->tree->sb->bsize);
	BUG_ON(node->block == 0 || node->block >= node->tree->sb->nr_blocks);

	KLOG(KL_DBG1, "node %p block %llu", node, node->block);

	clu = dio_clu_get(node->tree->sb->ddev, node->block);
	if (!clu) {
		KLOG(KL_ERR, "cant get clu for block %llu", node->block);
		return -EIO;
	}

	header = page_address(node->header);
	header->sig1 = node->sig1;
	header->sig2 = node->sig2;
	header->leaf = node->leaf;
	header->nr_keys = node->nr_keys;

	nkfs_btree_node_calc_sum(node, nkfs_btree_node_map_sum(node), 1);
	KLOG(KL_DBG3, "node block %llu nr_keys %d", node->block, node->nr_keys);
	nkfs_btree_node_to_ondisk(node, clu);

	KLOG_NKFS_BTREE_KEY(KL_DBG3, dio_clu_map(clu, PAGE_SIZE));
	KLOG_NKFS_BTREE_KEY(KL_DBG3, page_address(node->keys[0]));

	dio_clu_set_dirty(clu);
	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "sync err %d", err);
	}

	dio_clu_put(clu);
	return err;
}

static void nkfs_btree_node_delete(struct nkfs_btree_node *node)
{	
	KLOG(KL_DBG1, "node %p leaf %d nr_keys %d block %llu",
		node, node->leaf, node->nr_keys, node->block);

	nkfs_btree_nodes_remove(node->tree, node);
	nkfs_balloc_block_free(node->tree->sb, node->block);
	node->block = 0;
}

void nkfs_btree_key_by_u64(u64 val, struct nkfs_btree_key *key)
{
	BUG_ON(sizeof(val) > sizeof(*key));
	memset(key, 0, sizeof(*key));
	memcpy(key, &val, sizeof(val));
}

u64 nkfs_btree_key_to_u64(struct nkfs_btree_key *key)
{
	u64 val;
	memcpy(&val, key, sizeof(val));
	return val;
}

void nkfs_btree_value_by_u64(u64 val, struct nkfs_btree_value *value)
{
	BUG_ON(sizeof(val) > sizeof(value));
	memset(value, 0, sizeof(*value));
	value->val = val;
}

u64 nkfs_btree_value_to_u64(struct nkfs_btree_value *value)
{
	return value->val;
}

static struct nkfs_btree_node *nkfs_btree_node_create(struct nkfs_btree *tree)
{
	struct nkfs_btree_node *node, *inserted;
	int err;

	node = nkfs_btree_node_alloc(1);
	if (!node)
		return NULL;

	err = nkfs_balloc_block_alloc(tree->sb, &node->block);
	if (err) {
		KLOG(KL_ERR, "cant alloc block, err=%d", err);
		__nkfs_btree_node_free(node);
		return NULL;
	}
	node->tree = tree;
	err = nkfs_btree_node_write(node);
	if (err) {
		KLOG(KL_ERR, "cant write node at %llu, err=%d",
			node->block, err);
		nkfs_btree_node_delete(node);
		__nkfs_btree_node_free(node);
		return NULL;
	}

	inserted = nkfs_btree_nodes_insert(tree, node);
	if (inserted != node) {
		nkfs_btree_node_delete(node);
		__nkfs_btree_node_free(node);	
		node = inserted;
		KLOG(KL_DBG1, "node %p found block %llu", node, node->block);	
	} else {
		NKFS_BTREE_NODE_DEREF(inserted);
		KLOG(KL_DBG1, "node %p created block %llu", node, node->block);
	}

	return node;	
}

char *nkfs_btree_key_hex(struct nkfs_btree_key *key)
{
	return bytes_hex((char *)key, sizeof(*key));
}

char *nkfs_btree_value_hex(struct nkfs_btree_value *value)
{
	return bytes_hex((char *)value, sizeof(*value));
}

static void nkfs_btree_release(struct nkfs_btree *tree);

void nkfs_btree_ref(struct nkfs_btree *tree)
{
	atomic_inc(&tree->ref);
}

void nkfs_btree_deref(struct nkfs_btree *tree)
{
	BUG_ON(atomic_read(&tree->ref) <= 0);
	if (atomic_dec_and_test(&tree->ref)) {
		nkfs_btree_release(tree);
	}
}

u64 nkfs_btree_root_block(struct nkfs_btree *tree)
{
	u64 block;

	down_write(&tree->rw_lock);
	block = tree->root->block;
	up_write(&tree->rw_lock);

	return block;
}

struct nkfs_btree *nkfs_btree_create(struct nkfs_sb *sb, u64 root_block)
{
	struct nkfs_btree *tree;
	int err;

	tree = kmem_cache_alloc(nkfs_btree_cachep, GFP_NOIO);
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
	tree->sig1 = NKFS_BTREE_SIG1;

	if (root_block)
		tree->root = nkfs_btree_node_read(tree, root_block);
	else
		tree->root = nkfs_btree_node_create(tree);

	if (!tree->root)
		goto fail;

	if (!root_block) {
		tree->root->leaf = 1;
		err = nkfs_btree_node_write(tree->root);
		if (err)
			goto fail;
	}

	KLOG(KL_DBG1, "tree %p created root %p ref=%d",
		tree, tree->root, atomic_read(&tree->root->ref));

	return tree;
fail:
	nkfs_btree_deref(tree);
	return NULL;
}

void nkfs_btree_stop(struct nkfs_btree *tree)
{
	tree->releasing = 1;
	down_write(&tree->rw_lock);
	up_write(&tree->rw_lock);
}

static void nkfs_btree_release(struct nkfs_btree *tree)
{
	nkfs_btree_stop(tree);
	if (tree->root)
		NKFS_BTREE_NODE_DEREF(tree->root);

	KLOG(KL_DBG1, "tree %p nodes_active %d root %p",
		tree, tree->nodes_active,
		rb_entry(tree->nodes.rb_node, struct nkfs_btree_node, nodes_link));

	NKFS_BUG_ON(tree->nodes_active);

	kmem_cache_free(nkfs_btree_cachep, tree);
	KLOG(KL_DBG1, "tree %p deleted", tree);
}

static int nkfs_btree_cmp_key(
	struct nkfs_btree_key *key1,
	struct nkfs_btree_key *key2)
{
	return memcmp(key1, key2, sizeof(*key1));
}

static void nkfs_btree_copy_key(
	struct nkfs_btree_key *dst,
	struct nkfs_btree_key *src)
{
	memcpy(dst, src, sizeof(*src));
}

static void nkfs_btree_zero_key(struct nkfs_btree_key *key)
{
	memset(key, 0, sizeof(*key));
}

static void nkfs_btree_zero_value(struct nkfs_btree_value *value)
{
	memset(value, 0, sizeof(*value));
}

static void nkfs_btree_copy_value(
	struct nkfs_btree_value *dst,
	struct nkfs_btree_value *src)
{
	dst->val = src->val;
}

static void nkfs_btree_copy_child(
	struct nkfs_btree_child *dst,
	struct nkfs_btree_child *src)
{
	dst->val = src->val;
}

static void nkfs_btree_set_child_val(struct nkfs_btree_child *dst, u64 val)
{
	dst->val = val;
}

static u64 nkfs_btree_get_child_val(struct nkfs_btree_child *src)
{
	return src->val;
}

static int nkfs_btree_node_is_full(struct nkfs_btree_node *node)
{
	return ((2*NKFS_BTREE_T - 1) == node->nr_keys) ? 1 : 0;
}

static void nkfs_btree_node_copy_key_value(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_key *key, struct nkfs_btree_value *value)
{
	nkfs_btree_copy_key(nkfs_btree_node_key(dst, dst_index), key);
	nkfs_btree_copy_value(nkfs_btree_node_value(dst, dst_index), value);
}

static void nkfs_btree_node_copy_kv(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_node *src, int src_index)
{
	nkfs_btree_copy_key(nkfs_btree_node_key(dst, dst_index),
			nkfs_btree_node_key(src, src_index));
	nkfs_btree_copy_value(nkfs_btree_node_value(dst, dst_index),
			nkfs_btree_node_value(src, src_index));
}

static void nkfs_btree_node_copy_child(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_node *src, int src_index)
{
	nkfs_btree_copy_child(nkfs_btree_node_child(dst, dst_index),
				nkfs_btree_node_child(src, src_index));
}

static void nkfs_btree_node_set_child_val(struct nkfs_btree_node *dst, int dst_index,
	u64 val)
{
	nkfs_btree_set_child_val(nkfs_btree_node_child(dst, dst_index), val);
}

static u64 nkfs_btree_node_get_child_val(struct nkfs_btree_node *src, int src_index)
{
	return nkfs_btree_get_child_val(nkfs_btree_node_child(src, src_index));
}

static void nkfs_btree_node_put_child_val(struct nkfs_btree_node *dst, int dst_index,
	u64 val)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys; i >= dst_index; i--) {
		nkfs_btree_node_copy_child(dst, i + 1, dst, i);
	}

	nkfs_btree_node_set_child_val(dst, dst_index, val);
}


static void nkfs_btree_node_put_child(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_node *src, int src_index)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys; i >= dst_index; i--) {
		nkfs_btree_node_copy_child(dst, i + 1, dst, i);
	}

	nkfs_btree_node_copy_child(dst, dst_index, src, src_index);
}

static void nkfs_btree_node_put_key_value(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_key *key, struct nkfs_btree_value *value)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys - 1; i >= dst_index; i--) {
		nkfs_btree_node_copy_kv(dst, i + 1, dst, i);
	}

	nkfs_btree_node_copy_key_value(dst, dst_index, key, value);
}

static void nkfs_btree_node_put_kv(struct nkfs_btree_node *dst, int dst_index,
	struct nkfs_btree_node *src, int src_index)
{
	int i;

	/* free space for dst_index */
	for (i = dst->nr_keys - 1; i >= dst_index; i--) {
		nkfs_btree_node_copy_kv(dst, i + 1, dst, i);
	}

	nkfs_btree_node_copy_kv(dst, dst_index, src, src_index);
}

static void nkfs_btree_node_zero_kv(struct nkfs_btree_node *dst, int dst_index)
{
	nkfs_btree_zero_key(nkfs_btree_node_key(dst, dst_index));
	nkfs_btree_zero_value(nkfs_btree_node_value(dst, dst_index));
}

static void nkfs_btree_node_split_child(struct nkfs_btree_node *node,
		struct nkfs_btree_node *child,
		int child_index, struct nkfs_btree_node *new)
{
	int i;

	BUG_ON(child_index < 0 || child_index > node->nr_keys);
	BUG_ON(!child || !nkfs_btree_node_is_full(child));	
	BUG_ON(!new);

	KLOG(KL_DBG1, "Splitting node [%p %d] child[%d]=[%p %d]",
		node, node->nr_keys, child_index, child, child->nr_keys);

	new->leaf = child->leaf;
	/* copy T-1 keys from child to new */
	for (i = 0; i < new->t - 1; i++)
		nkfs_btree_node_copy_kv(new, i, child, i + new->t);
	new->nr_keys = new->t - 1;

	/* copy T childs from child to new */
	if (!child->leaf) {
		for (i = 0; i < new->t; i++) {
			nkfs_btree_node_copy_child(new, i, child, i + new->t);
		}
	}

	/* shift node childs to the right by one */
	child->nr_keys = new->t - 1;

	/* setup node new child */
	nkfs_btree_node_put_child_val(node, child_index + 1, new->block);

	/* move mid key from child to node */
	nkfs_btree_node_put_kv(node, child_index, child, new->t - 1);
	node->nr_keys++;

	KLOG(KL_DBG1, "Splitted node [%p %d] child[%d]=[%p %d] new [%p %d]",
		node, node->nr_keys, child_index, child, child->nr_keys,
		new, new->nr_keys);
}

static int nkfs_btree_node_key_probably_inside(struct nkfs_btree_node *node, struct nkfs_btree_key *key)
{
	if (0 == node->nr_keys)
		return 0;
	else if ((nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, node->nr_keys-1)) > 0)
		|| (nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, 0)) < 0))
		return 0;
	return 1;
}

static int nkfs_btree_node_has_key(struct nkfs_btree_node *node,
	struct nkfs_btree_key *key)
{
	u32 start = 0;
	u32 end = node->nr_keys;
	u32 mid;
	int cmp;

	if (!nkfs_btree_node_key_probably_inside(node, key));
		return -1;

	while (start < end) {
		mid = (start + end) / 2;

		cmp = nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, mid));

		if (!cmp)
			return mid;
		else if (cmp < 0)
			end = mid;
		else
			start = mid + 1;
	}

	return -1;
}

static int nkfs_btree_node_find_key_index(struct nkfs_btree_node *node,
	struct nkfs_btree_key *key)
{
	u32 start = 0;
	u32 end = node->nr_keys;
	u32 mid;
	int cmp;

	if (0 == node->nr_keys)
		return 0; 
	else if (nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, end-1)) > 0)
		return end;
	else if (nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, start)) < 0)
		return 0;

	while (start < end) {
		mid = (start + end) / 2;

		cmp = nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, mid));

		if (!cmp)
			return mid;
		else if (cmp < 0)
			end = mid;
		else
			start = mid + 1;
	}

	return end;
}

static int nkfs_btree_node_insert_nonfull(
	struct nkfs_btree_node *first,
	struct nkfs_btree_key *key,
	struct nkfs_btree_value *value,
	int replace)
{
	int i;
	struct nkfs_btree_node *node = first;
	
	while (1) {
		KLOG(KL_DBG1, "node [%p %d] leaf %d",
			node, node->nr_keys, node->leaf);
		/* if key exists replace value */
		i = nkfs_btree_node_has_key(node, key);
		if (i >= 0) {
			if (replace) {
				nkfs_btree_copy_value(
						nkfs_btree_node_value(node, i),
						value);
				nkfs_btree_node_write(node);
				if (node != first)
					NKFS_BTREE_NODE_DEREF(node);
				return 0;
			} else {
				if (node != first)
					NKFS_BTREE_NODE_DEREF(node);
				return -EEXIST;
			}
		}

		if (node->leaf) {
			/* key doesnt exists so place key value in sorted
			 * order
			 */
			i = nkfs_btree_node_find_key_index(node, key);
			nkfs_btree_node_put_key_value(node, i, key, value);
			node->nr_keys++;
			KLOG(KL_DBG1, "inserted key into node=%p nr_keys=%d",
					node, node->nr_keys);
			nkfs_btree_node_write(node);
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			return 0;
		} else {
			struct nkfs_btree_node *child;
			i = nkfs_btree_node_find_key_index(node, key);
			child = nkfs_btree_node_read(node->tree,
				nkfs_btree_node_get_child_val(node, i));
			if (!child) {
				if (node != first)
					NKFS_BTREE_NODE_DEREF(node);
				return -EIO;
			}

			if (nkfs_btree_node_is_full(child)) {
				struct nkfs_btree_node *new;
				new = nkfs_btree_node_create(node->tree);
				if (!new) {
					if (node != first)
						NKFS_BTREE_NODE_DEREF(node);
					NKFS_BTREE_NODE_DEREF(child);
					return -EIO;
				}
				nkfs_btree_node_split_child(node, child, i, new);
				
				nkfs_btree_node_write(node);
				nkfs_btree_node_write(child);
				nkfs_btree_node_write(new);
				
				NKFS_BTREE_NODE_DEREF(new);
				NKFS_BTREE_NODE_DEREF(child);
				continue; /* restart */
			}
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			node = child;
		}
	}
}

static void nkfs_btree_node_copy(struct nkfs_btree_node *dst, struct nkfs_btree_node *src);

int nkfs_btree_insert_key(struct nkfs_btree *tree, struct nkfs_btree_key *key,
	struct nkfs_btree_value *value,
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

	KLOG_NKFS_BTREE_KEY(KL_DBG3, key);
	if (nkfs_btree_node_is_full(tree->root)) {
		struct nkfs_btree_node *new, *new2, *root = tree->root, *clone;

		clone = nkfs_btree_node_create(tree);
		if (clone == NULL) {
			rc = -ENOMEM;
			goto out;
		}

		new = nkfs_btree_node_alloc(1);
		if (new == NULL) {
			nkfs_btree_node_delete(clone);
			NKFS_BTREE_NODE_DEREF(clone);
			rc = -ENOMEM;
			goto out;
		}

		new2 = nkfs_btree_node_create(tree);
		if (new2 == NULL) {
			nkfs_btree_node_delete(clone);
			NKFS_BTREE_NODE_DEREF(clone);
			__nkfs_btree_node_free(new);
			rc = -ENOMEM;
			goto out;
		}

		nkfs_btree_node_copy(clone, root);

		new->leaf = 0;
		new->nr_keys = 0;
		nkfs_btree_node_set_child_val(new, 0, clone->block);
		nkfs_btree_node_split_child(new, clone, 0, new2);

		nkfs_btree_node_copy(root, new);
		
		nkfs_btree_node_write(clone);
		nkfs_btree_node_write(new2);
		nkfs_btree_node_write(root);
	
		NKFS_BTREE_NODE_DEREF(clone);
		NKFS_BTREE_NODE_DEREF(new2);
		__nkfs_btree_node_free(new);
	}

	rc = nkfs_btree_node_insert_nonfull(tree->root, key, value, replace);
out:
	up_write(&tree->rw_lock);
	return rc;
}

static
struct nkfs_btree_node *nkfs_btree_node_find_key(struct nkfs_btree_node *first,
		struct nkfs_btree_key *key, int *pindex)
{
	int i;
	struct nkfs_btree_node *node = first;
	
	while (1) {
		if (node->nr_keys == 0) {
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			return NULL;
		}

		i = nkfs_btree_node_find_key_index(node, key);
		if (i < node->nr_keys &&
				nkfs_btree_cmp_key(key, nkfs_btree_node_key(node, i)) == 0) {
			*pindex = i;
			if (node == first)
				NKFS_BTREE_NODE_REF(node);
			return node;
		} else if (node->leaf) {
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			return NULL;
		} else {
			struct nkfs_btree_node *prev = node;
			node = nkfs_btree_node_read(node->tree,
				nkfs_btree_node_get_child_val(node, i));
			BUG_ON(!node);
			if (prev != first)
				NKFS_BTREE_NODE_DEREF(prev);	
		}
	}
}

int nkfs_btree_find_key(struct nkfs_btree *tree,
	struct nkfs_btree_key *key,
	struct nkfs_btree_value *pvalue)
{
	struct nkfs_btree_node *found;
	int index;

	if (tree->releasing)
		return -EAGAIN;

	down_read(&tree->rw_lock);
	if (tree->releasing) {
		up_read(&tree->rw_lock);
		return -EAGAIN;
	}

	found = nkfs_btree_node_find_key(tree->root, key, &index);
	if (found == NULL) {
		up_read(&tree->rw_lock);
		return -ENOENT;
	}

	nkfs_btree_copy_value(pvalue, nkfs_btree_node_value(found, index));
	NKFS_BTREE_NODE_DEREF(found);
	up_read(&tree->rw_lock);

	return 0;
}

static void __nkfs_btree_node_delete_child_index(struct nkfs_btree_node *node,
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

static void __nkfs_btree_node_delete_key_index(struct nkfs_btree_node *node,
		int index)
{
	int i;

	BUG_ON(node->nr_keys < 1);
	/* do shift to the left by one */
	for (i = (index + 1); i < node->nr_keys; i++) {
		nkfs_btree_node_copy_kv(node, i-1, node, i);	
	}
	/* zero last slot */
	nkfs_btree_node_zero_kv(node, i-1);
}

static void nkfs_btree_node_leaf_delete_key(struct nkfs_btree_node *node,
		struct nkfs_btree_key *key)
{
	int index;
	BUG_ON(!node->leaf);
	index = nkfs_btree_node_has_key(node, key);
	BUG_ON(index < 0 || index >= node->nr_keys);
	__nkfs_btree_node_delete_key_index(node, index);
	node->nr_keys--;
}

static struct nkfs_btree_node *
nkfs_btree_node_child_balance(struct nkfs_btree_node *node,
	int child_index);

static struct nkfs_btree_node *
nkfs_btree_node_find_left_most(struct nkfs_btree_node *node, int *pindex)
{
	struct nkfs_btree_node *curr, *next;

	BUG_ON(node->nr_keys == 0);
	if (node->leaf) {
		*pindex = node->nr_keys - 1;
		NKFS_BTREE_NODE_REF(node);
		return node;
	}

	curr = nkfs_btree_node_child_balance(node, node->nr_keys);
	BUG_ON(!curr);
	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = curr->nr_keys - 1;
			return curr;
		}
		next = nkfs_btree_node_child_balance(curr, curr->nr_keys);
		NKFS_BTREE_NODE_DEREF(curr);
		curr = next;
		BUG_ON(!curr);
	}
}

static struct nkfs_btree_node *
nkfs_btree_node_find_right_most(struct nkfs_btree_node *node, int *pindex)
{
	struct nkfs_btree_node *curr, *next;

	BUG_ON(node->nr_keys == 0);
	if (node->leaf) {
		*pindex = 0;
		NKFS_BTREE_NODE_REF(node);
		return node;
	}

	curr = nkfs_btree_node_child_balance(node, 0);
	BUG_ON(!curr);
	while (1) {
		BUG_ON(curr->nr_keys == 0);
		if (curr->leaf) {
			*pindex = 0;
			return curr;
		}
		next = nkfs_btree_node_child_balance(curr, 0);
		NKFS_BTREE_NODE_DEREF(curr);
		curr = next;
	}
}

static void nkfs_btree_node_merge(struct nkfs_btree_node *dst,
	struct nkfs_btree_node *src, struct nkfs_btree_key *key,
	struct nkfs_btree_value *value)
{
	int i, pos;

	KLOG(KL_DBG1, "Merging %p %d -> %p %d",
		src, src->nr_keys, dst, dst->nr_keys);
	/* copy mid key and value */
	nkfs_btree_copy_key(nkfs_btree_node_key(dst, dst->nr_keys), key);
	nkfs_btree_copy_value(nkfs_btree_node_value(dst, dst->nr_keys), value);

	pos = dst->nr_keys + 1;
	for (i = 0; i < src->nr_keys; i++, pos++) {
		/* copy key-values */
		nkfs_btree_node_copy_kv(dst, pos, src, i);
		/* copy childs */
		nkfs_btree_node_copy_child(dst, pos, src, i);
	}
	/* copy last child */
	nkfs_btree_node_copy_child(dst, pos, src, i);
	/* update keys num */
	dst->nr_keys = dst->nr_keys + 1 + src->nr_keys;

	KLOG(KL_DBG1, "Merged %p -> %p nr_keys %d",
		src, dst, dst->nr_keys);	
}

static void nkfs_btree_node_copy(struct nkfs_btree_node *dst, struct nkfs_btree_node *src)
{
	int i;
	for (i = 0; i < src->nr_keys; i++) {
		nkfs_btree_node_copy_kv(dst, i, src, i);
		nkfs_btree_node_copy_child(dst, i, src, i);
	}
	nkfs_btree_node_copy_child(dst, i, src, i);
	dst->nr_keys = src->nr_keys;
	dst->leaf = src->leaf;
}

static void nkfs_btree_node_child_give_key(struct nkfs_btree_node *node,
	struct nkfs_btree_node *child, int child_index,
	struct nkfs_btree_node *sib, int left)
{
	/* give child an extra key by moving a key from node
	* down into child, moving a key from child's
	* immediate left or right sibling up into node,
	* and moving the appropriate child pointer from the
	* sibling into child
	*/

	if (!left) {
		nkfs_btree_node_copy_kv(child, child->nr_keys, node, child_index);
		nkfs_btree_node_copy_kv(node, child_index, sib, 0);
		nkfs_btree_node_copy_child(child, child->nr_keys + 1, sib, 0);

		__nkfs_btree_node_delete_key_index(sib, 0);
		__nkfs_btree_node_delete_child_index(sib, 0);
	} else {
		nkfs_btree_node_put_kv(child, 0, node, child_index-1);
		nkfs_btree_node_copy_kv(node, child_index-1, sib, sib->nr_keys-1);
		nkfs_btree_node_put_child(child, 0, sib, sib->nr_keys);

		__nkfs_btree_node_delete_key_index(sib, sib->nr_keys-1);
		__nkfs_btree_node_delete_child_index(sib, sib->nr_keys);
	}
	child->nr_keys++;
	sib->nr_keys--;
	nkfs_btree_node_write(sib);
	nkfs_btree_node_write(child);
	nkfs_btree_node_write(node);
}

static struct nkfs_btree_node * nkfs_btree_node_child_merge(struct nkfs_btree_node *node,
	struct nkfs_btree_node *child, int child_index,
	struct nkfs_btree_node *sib, int left)
{
	struct nkfs_btree_node *merged;
	/* merge child with left, which involves
	* moving a key from node down into the new 
	* merged node to become the median key for
	* that node.
	*/

	/* k0 k1 k2	k0 k2
	* c0 c1 c2 c3  c0 c1c2 c3
	*/

	if (left) {
		nkfs_btree_node_merge(sib, child, nkfs_btree_node_key(node, child_index-1),
			nkfs_btree_node_value(node, child_index-1));
		__nkfs_btree_node_delete_key_index(node, child_index-1);
		__nkfs_btree_node_delete_child_index(node, child_index);
		node->nr_keys--;
		nkfs_btree_node_write(sib);
		nkfs_btree_node_write(node);
		nkfs_btree_node_delete(child);
		merged = sib;
	} else {
		nkfs_btree_node_merge(child, sib, nkfs_btree_node_key(node, child_index),
			nkfs_btree_node_value(node, child_index));
		__nkfs_btree_node_delete_key_index(node, child_index);
		__nkfs_btree_node_delete_child_index(node, child_index+1);
		node->nr_keys--;
		nkfs_btree_node_write(child);
		nkfs_btree_node_write(node);
		nkfs_btree_node_delete(sib);
		merged = child;
	}

	if (node->nr_keys == 0) {
		nkfs_btree_node_copy(node, merged);
		nkfs_btree_node_write(node);
		nkfs_btree_node_delete(merged);
		return node;
	} else {
		return merged;
	}
}

static struct nkfs_btree_node *
nkfs_btree_node_child_balance(struct nkfs_btree_node *node,
	int child_index)
{
	struct nkfs_btree_node *child, *next;

	BUG_ON(node->leaf);
	child = nkfs_btree_node_read(node->tree,
		nkfs_btree_node_get_child_val(node, child_index));
	if (!child) {
		KLOG(KL_ERR, "cant read child");
		return NULL;
	}

	KLOG(KL_DBG1, "child %p nr_keys %d t %d",
		child, child->nr_keys, child->t);

	if (child->nr_keys < child->t) {
		struct nkfs_btree_node *left = (child_index > 0) ?
			nkfs_btree_node_read(node->tree,
				nkfs_btree_node_get_child_val(node, child_index-1)) :
				NULL;
		struct nkfs_btree_node *right = (child_index < node->nr_keys) ?
			nkfs_btree_node_read(node->tree,
				nkfs_btree_node_get_child_val(node, child_index+1)) :
				NULL;

		if (left && left->nr_keys >= left->t) {
			nkfs_btree_node_child_give_key(node, child,
				child_index, left, 1);
			next = child;
		} else if (right && right->nr_keys >= right->t) {
			nkfs_btree_node_child_give_key(node, child,
				child_index, right, 0);
			next = child;
		} else if (left && left->nr_keys < left->t) {
			next = nkfs_btree_node_child_merge(node, child,
				child_index, left, 1);
		} else if (right->nr_keys < right->t) {
			next = nkfs_btree_node_child_merge(node, child,
				child_index, right, 0);
		} else {
			KLOG(KL_ERR, "no way to add key to child=%p",
				child);
			BUG();
			next = NULL;
		}

		if (next != child) {
			NKFS_BTREE_NODE_DEREF(child);
			NKFS_BTREE_NODE_REF(next);
		}

		if (right)
			NKFS_BTREE_NODE_DEREF(right);
		if (left)
			NKFS_BTREE_NODE_DEREF(left);

		child = next;
	}

	if (child == node)
		NKFS_BTREE_NODE_DEREF(child);

	return child;
}

int nkfs_btree_node_delete_key(struct nkfs_btree_node *first,
		struct nkfs_btree_key *key)
{
	struct nkfs_btree_key key_copy;
	struct nkfs_btree_node *node = first;
	int i;

restart:
	nkfs_btree_copy_key(&key_copy, key);
	key = &key_copy;

	if ((i = nkfs_btree_node_has_key(node, key)) >= 0) {
		if (node->leaf) {
			nkfs_btree_node_leaf_delete_key(node, key);
			nkfs_btree_node_write(node);
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			return 0;
		} else {
			struct nkfs_btree_node *pre_child = NULL;
			struct nkfs_btree_node *suc_child = NULL;
			int index;

			index = nkfs_btree_node_has_key(node, key);
			BUG_ON(index < 0);

			pre_child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, index));
			suc_child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, index+1));
			BUG_ON(!pre_child);
			BUG_ON(!suc_child);

			if (pre_child->nr_keys >= pre_child->t) {
				struct nkfs_btree_node *pre;
				int pre_index;
	
				pre = nkfs_btree_node_find_left_most(pre_child,
					&pre_index);
				nkfs_btree_node_copy_kv(node, index,
					pre, pre_index);
				nkfs_btree_node_write(node);
				if (node != first)
					NKFS_BTREE_NODE_DEREF(node);
				NKFS_BTREE_NODE_DEREF(pre_child);
				NKFS_BTREE_NODE_DEREF(suc_child);
				node = pre;
				key = nkfs_btree_node_key(pre, pre_index);
				goto restart;
			} else if (suc_child->nr_keys >= suc_child->t) {
				struct nkfs_btree_node *suc;
				int suc_index;

				suc = nkfs_btree_node_find_right_most(suc_child,
					&suc_index);
				nkfs_btree_node_copy_kv(node, index,
					suc, suc_index);
				nkfs_btree_node_write(node);
				if (node != first)
					NKFS_BTREE_NODE_DEREF(node);
				NKFS_BTREE_NODE_DEREF(suc_child);
				NKFS_BTREE_NODE_DEREF(pre_child);
				node = suc;
				key = nkfs_btree_node_key(suc, suc_index);
				goto restart;
			} else {
				/* merge key and all of suc_child
				* into pre_child
				* node loses key and pointer to suc_child
				*/
				int key_index = pre_child->nr_keys;

				nkfs_btree_node_merge(pre_child, suc_child,
					nkfs_btree_node_key(node, index),
					nkfs_btree_node_value(node, index));
				/* delete key from node */
				__nkfs_btree_node_delete_key_index(node,
						index);
				__nkfs_btree_node_delete_child_index(node,
						index+1);
				node->nr_keys--;
				nkfs_btree_node_write(node);
				nkfs_btree_node_write(pre_child);
				/* delete suc_child */
				nkfs_btree_node_delete(suc_child);
				if (node->nr_keys == 0) {
					nkfs_btree_node_copy(node, pre_child);
					nkfs_btree_node_delete(pre_child);
					nkfs_btree_node_write(node);

					NKFS_BTREE_NODE_DEREF(suc_child);
					NKFS_BTREE_NODE_DEREF(pre_child);
					pre_child = node;
				} else {
					if (node != first)
						NKFS_BTREE_NODE_DEREF(node);
					NKFS_BTREE_NODE_DEREF(suc_child);
					node = pre_child;
				}
				key = nkfs_btree_node_key(pre_child, key_index);
				goto restart;
			}
		}
	} else {
		if (node->leaf) {
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			return -ENOENT;
		} else {
			struct nkfs_btree_node *child;
			BUG_ON(nkfs_btree_node_has_key(node, key) >= 0);
			i = nkfs_btree_node_find_key_index(node, key);
			child = nkfs_btree_node_child_balance(node, i);
			if (node != first)
				NKFS_BTREE_NODE_DEREF(node);
			node = child;
			goto restart;
		}
	}
}

int nkfs_btree_delete_key(struct nkfs_btree *tree, struct nkfs_btree_key *key)
{
	int rc;

	if (tree->releasing)
		return -EAGAIN;

	down_write(&tree->rw_lock);
	if (tree->releasing) {
		up_write(&tree->rw_lock);
		return -EAGAIN;
	}
	rc = nkfs_btree_node_delete_key(tree->root, key);
	up_write(&tree->rw_lock);
	return rc;
}

static void nkfs_btree_log_node(struct nkfs_btree_node *first, u32 height, int llevel)
{
	struct nkfs_btree_node *child;
	struct nkfs_btree_node *node = first;
	int i;

	KLOG(llevel, "node %p nr_keys %d leaf %d height %u",
			node, node->nr_keys, node->leaf, height);

	if (node->nr_keys) {
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, i));
				BUG_ON(!child);
				nkfs_btree_log_node(child, height+1, llevel);
				NKFS_BTREE_NODE_DEREF(child);
			}
		}
	}
}

static int nkfs_btree_enum_node(struct nkfs_btree_node *first, nkfs_btree_enum_clb_t clb,
		void *ctx)
{
	struct nkfs_btree_node *child;
	struct nkfs_btree_node *node = first;
	int i;
	int rc;

	if (node->nr_keys) {
		for (i = 0; i < node->nr_keys; i++) {
			if (clb(ctx, node, i))
				return 1;
		}

		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, i));
				BUG_ON(!child);
				rc = nkfs_btree_enum_node(child, clb, ctx);
				NKFS_BTREE_NODE_DEREF(child);
				if (rc)
					return rc;
			}
		}
	}
	return 0;
}

int nkfs_btree_enum_tree(struct nkfs_btree *tree, nkfs_btree_enum_clb_t clb, void *ctx)
{
	return nkfs_btree_enum_node(tree->root, clb, ctx);
}

static void nkfs_btree_node_stats(struct nkfs_btree_node *node,
	struct nkfs_btree_info *info)
{
	struct nkfs_btree_node *child;
	int i;

	if (node->nr_keys) {
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, i));
				BUG_ON(!child);
				nkfs_btree_node_stats(child, info);
				NKFS_BTREE_NODE_DEREF(child);
			}
		}
	}
	info->nr_nodes++;
	info->nr_keys += node->nr_keys;
}

void nkfs_btree_stats(struct nkfs_btree *tree, struct nkfs_btree_info *info)
{
	memset(info, 0, sizeof(*info));
	if (tree->releasing)
		return;

	down_read(&tree->rw_lock);
	if (!tree->releasing)
		nkfs_btree_node_stats(tree->root, info);
	up_read(&tree->rw_lock);
}

void nkfs_btree_log(struct nkfs_btree *tree, int llevel)
{
	if (tree->releasing)
		return;

	down_read(&tree->rw_lock);
	if (!tree->releasing)
		nkfs_btree_log_node(tree->root, 1, llevel);
	up_read(&tree->rw_lock);
}

static void nkfs_btree_erase_node(struct nkfs_btree_node *root,
	nkfs_btree_key_erase_clb_t key_erase_clb,
	void *ctx)
{
	struct nkfs_btree_node *child;
	struct nkfs_btree_node *node = root;
	int i;

	if (node->nr_keys) {
		if (!node->leaf) {
			for (i = 0; i < node->nr_keys + 1; i++) {
				child = nkfs_btree_node_read(node->tree,
					nkfs_btree_node_get_child_val(node, i));
				BUG_ON(!child);
				nkfs_btree_erase_node(child, key_erase_clb, ctx);
				NKFS_BTREE_NODE_DEREF(child);
			}
		}
		if (key_erase_clb) {
			for (i = 0; i < node->nr_keys; i++) {
				key_erase_clb(nkfs_btree_node_key(node, i),
					nkfs_btree_node_value(node, i),
					ctx);
			}
		}
	}
	nkfs_btree_node_delete(node);
}

void nkfs_btree_erase(struct nkfs_btree *tree,
	nkfs_btree_key_erase_clb_t key_erase_clb,
	void *ctx)
{
	down_write(&tree->rw_lock);
	tree->releasing = 1;
	nkfs_btree_erase_node(tree->root, key_erase_clb, ctx);
	up_write(&tree->rw_lock);
}

static int nkfs_btree_node_check(struct nkfs_btree_node *first, int root)
{
	int i;
	int errs = 0;
	struct nkfs_btree_key *prev_key;
	struct nkfs_btree_node *node = first;

	if (node->sig1 != NKFS_BTREE_SIG1 || node->sig2 != NKFS_BTREE_SIG2) {
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
		if (prev_key && (nkfs_btree_cmp_key(prev_key,
			nkfs_btree_node_key(node, i)) >= 0)) {
			KLOG(KL_ERR, "node %p key %d not sorted",
				node, i);
			errs++;
		}
		prev_key = nkfs_btree_node_key(node, i);
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
			struct nkfs_btree_node *child;
			child = nkfs_btree_node_read(node->tree,
				nkfs_btree_node_get_child_val(node, i));
			BUG_ON(!child);
			errs+= nkfs_btree_node_check(child, 0);
			NKFS_BTREE_NODE_DEREF(child);
		}
	}

	return errs;
}

int nkfs_btree_check(struct nkfs_btree *tree)
{
	int rc;
	if (tree->releasing)
		return -EAGAIN;

	down_read(&tree->rw_lock);
	if (!tree->releasing) {
		rc = nkfs_btree_node_check(tree->root, 1);
		KLOG(KL_INF, "tree %p check rc %d", tree, rc);
	} else
		rc = -EAGAIN;
	up_read(&tree->rw_lock);
	return rc;
}

void nkfs_btree_read_lock(struct nkfs_btree *tree)
{
	down_read(&tree->rw_lock);
}

void nkfs_btree_read_unlock(struct nkfs_btree *tree)
{
	up_read(&tree->rw_lock);
}

void nkfs_btree_write_lock(struct nkfs_btree *tree)
{
	down_write(&tree->rw_lock);
}

void nkfs_btree_write_unlock(struct nkfs_btree *tree)
{
	up_write(&tree->rw_lock);
}

int nkfs_btree_init(void)
{
	int err;

	nkfs_btree_node_cachep = kmem_cache_create("nkfs_btree_node_cache",
			sizeof(struct nkfs_btree_node), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!nkfs_btree_node_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	nkfs_btree_cachep = kmem_cache_create("nkfs_btree_cache",
			sizeof(struct nkfs_btree), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!nkfs_btree_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_node_cache;
	}

	return 0;

del_node_cache:
	kmem_cache_destroy(nkfs_btree_node_cachep);
out:
	return err;
}

void nkfs_btree_finit(void)
{
	kmem_cache_destroy(nkfs_btree_node_cachep);
	kmem_cache_destroy(nkfs_btree_cachep);
}
