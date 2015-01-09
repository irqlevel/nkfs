#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "super"

static DEFINE_RWLOCK(sb_list_lock);
static LIST_HEAD(sb_list);

static struct kmem_cache *ds_sb_cachep;
static int ds_sb_sync(struct ds_sb *sb);

static void ds_sb_release(struct ds_sb *sb)
{
	KLOG(KL_DBG, "sb %p obj tree %p", sb, sb->obj_tree);
	if (sb->obj_tree)
		btree_deref(sb->obj_tree);

	KLOG(KL_DBG, "sb %p released, obj tree %p",
		sb, sb->obj_tree);
}

static void ds_sb_delete(struct ds_sb *sb)
{
	ds_sb_release(sb);
	kmem_cache_free(ds_sb_cachep, sb);
}

void ds_sb_stop(struct ds_sb *sb)
{
	KLOG(KL_DBG, "sb %p dev %s stopping",
			sb, sb->dev->dev_name);

	write_lock(&sb_list_lock);
	list_del_init(&sb->list);
	write_unlock(&sb_list_lock);

	if (sb->obj_tree)
		btree_stop(sb->obj_tree);

	ds_sb_sync(sb);	
}

void ds_sb_ref(struct ds_sb *sb)
{
	BUG_ON(atomic_read(&sb->refs) <= 0);
	atomic_inc(&sb->refs);
}

void ds_sb_deref(struct ds_sb *sb)
{
	BUG_ON(atomic_read(&sb->refs) <= 0);
	if (atomic_dec_and_test(&sb->refs))
		ds_sb_delete(sb);
}

struct ds_sb *ds_sb_lookup(struct ds_obj_id *id)
{
	struct ds_sb *sb, *found = NULL;

	read_lock(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		if (0 == ds_obj_id_cmp(&sb->id, id)) {
			ds_sb_ref(sb);
			found = sb;
			break;
		}	
	}
	read_unlock(&sb_list_lock);
	return found;
}

int ds_sb_insert(struct ds_sb *cand)
{
	struct ds_sb *sb;
	int err;

	write_lock(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		if (0 == ds_obj_id_cmp(&sb->id, &cand->id)) {
			err = -EEXIST;
			break;
		}
	}
	list_add_tail(&cand->list, &sb_list);
	err = 0;
	write_unlock(&sb_list_lock);
	return err;
}

static int ds_sb_gen_header(struct ds_sb *sb,
	u64 size,
	u32 bsize)
{
	int err;
	u64 bm_blocks;	

	if (ds_mod(size, bsize) || (size <= bsize)) {
		KLOG(KL_ERR, "size %lld bsize %u", size, bsize);
		return -EINVAL;
	}

	bm_blocks = ds_div_round_up(ds_div_round_up(ds_div(size, bsize), 8), bsize);
	if (size <= (DS_IMAGE_BM_BLOCK + bm_blocks)*bsize) {
		KLOG(KL_ERR, "size %lld to small bsize %u bm_blocks %u",
			size, bsize, bm_blocks);
		return -EINVAL;
	}

	err = ds_obj_id_gen(&sb->id);
	if (err) {
		KLOG(KL_ERR, "cant gen obj id err %d", err);
		return err;
	}


	sb->magic = DS_IMAGE_MAGIC;
	sb->version = DS_IMAGE_VER_1;
	sb->size = size;
	sb->bsize = bsize;

	sb->bm_block = DS_IMAGE_BM_BLOCK;
	sb->bm_blocks = bm_blocks;

	return 0;
}

static void ds_sb_parse_header(struct ds_sb *sb,
	struct ds_image_header *header)
{
	sb->magic = be32_to_cpu(header->magic);
	sb->version = be32_to_cpu(header->version);
	sb->size = be64_to_cpu(header->size);
	sb->bsize = be32_to_cpu(header->bsize);
	sb->bm_block = be64_to_cpu(header->bm_block);
	sb->bm_blocks = be64_to_cpu(header->bm_blocks);
	sb->obj_tree_block = be64_to_cpu(header->obj_tree_block);

	memcpy(&sb->id, &header->id, sizeof(header->id));	
}

static void ds_sb_fill_header(struct ds_sb *sb,
	struct ds_image_header *header)
{
	memset(header, 0, sizeof(*header));

	header->magic = cpu_to_be32(sb->magic);
	header->version = cpu_to_be32(sb->version);
	header->size = cpu_to_be64(sb->size);
	header->bsize = cpu_to_be32(sb->bsize);
	header->bm_block = cpu_to_be64(sb->bm_block);
	header->bm_blocks = cpu_to_be64(sb->bm_blocks);
	header->obj_tree_block = cpu_to_be64(sb->obj_tree_block);
	memcpy(&header->id, &sb->id, sizeof(sb->id));
}

static int ds_sb_sync(struct ds_sb *sb)
{
	struct buffer_head *bh;
	int err;

	bh = __bread(sb->bdev, 0, sb->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read 0block");
		return -EIO;
	}
	ds_sb_fill_header(sb, (struct ds_image_header *)bh->b_data);
	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "cant sync 0block");
		goto out;
	}

out:
	brelse(bh);
	return err;
}

static int ds_sb_check(struct ds_sb *sb)
{
	int err;

	if (sb->magic != DS_IMAGE_MAGIC) {
		KLOG(KL_ERR, "sb %p invalid magic %x", sb,
			sb->magic);
		err = -EINVAL;
		goto out;
	}

	if (sb->version != DS_IMAGE_VER_1) {
		KLOG(KL_ERR, "sb %p invalid version %x", sb,
				sb->version);
		err = -EINVAL;
		goto out;
	}

	if ((sb->size == 0) || (sb->bsize == 0) ||
		(sb->size <= sb->bsize) ||
		(ds_mod(sb->size, sb->bsize))) {
		KLOG(KL_ERR, "sb %p invalid size %llu bsize %u",
			sb, sb->size, sb->bsize);
		err = -EINVAL;
		goto out;
	}

	if (sb->bm_block != DS_IMAGE_BM_BLOCK) {
		KLOG(KL_ERR, "sb %p invalid bm_block %u",
			sb, sb->bm_block);
		err = -EINVAL;
		goto out;
	}

	if (sb->size <= (sb->bm_blocks + DS_IMAGE_BM_BLOCK)) {
		KLOG(KL_ERR, "sb %p invalid size %llu bm_blocks %llu",
			sb, sb->size, sb->bm_blocks);
		err = -EINVAL;
		goto out;
	}

	err = 0;
out:
	return err;
}


static int ds_sb_create(struct ds_dev *dev,
		struct ds_image_header *header,
		struct ds_sb **psb)
{
	int err;
	struct ds_sb *sb;

	sb = kmem_cache_alloc(ds_sb_cachep, GFP_NOFS);
	if (!sb) {
		KLOG(KL_ERR, "cant alloc sb");
		return -ENOMEM;
	}
	memset(sb, 0, sizeof(*sb));
	init_rwsem(&sb->rw_lock);
	INIT_LIST_HEAD(&sb->list);
	atomic_set(&sb->refs, 1);

	if (!header) {
		err = ds_sb_gen_header(sb, i_size_read(dev->bdev->bd_inode),
			dev->bsize);
		if (err) {
			KLOG(KL_ERR, "can gen header");
			goto free_sb;
		}
	} else {
		ds_sb_parse_header(sb, header);
	}

	err = ds_sb_check(sb);
	if (err) {
		KLOG(KL_ERR, "invalid sb");
		goto free_sb;
	}

	sb->nr_blocks = ds_div(sb->size, sb->bsize);
	sb->dev = dev;
	sb->bdev = dev->bdev;

	*psb = sb;
	return 0;

free_sb:
	kmem_cache_free(ds_sb_cachep, sb);
	return err;
}

void ds_sb_log(struct ds_sb *sb)
{
	KLOG(KL_DBG, "sb %p blocks %llu obj tree %llu bm %llu bm_blocks %llu",
		sb, sb->nr_blocks, sb->obj_tree_block, sb->bm_block,
		sb->bm_blocks);
}

int ds_sb_format(struct ds_dev *dev, struct ds_sb **psb)
{
	struct buffer_head *bh;
	int err;
	struct ds_sb *sb = NULL;
	u64 i;

	bh = __bread(dev->bdev, 0, dev->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;	
	}

	err = ds_sb_create(dev, NULL, &sb);
	if (err) {
		KLOG(KL_ERR, "cant create sb");
		goto out;
	}

	err = ds_balloc_bm_clear(sb);
	if (err) {
		KLOG(KL_ERR, "cant clear balloc bm err %d", err);
		goto del_sb;
	}

	for (i = 0; i < sb->bm_block + sb->bm_blocks; i++) {
		err = ds_balloc_block_mark(sb, i, 1);
		if (err) {
			KLOG(KL_ERR, "cant mark block %llu as used err %d",
				i, err);
			goto del_sb;
		}
	}
	
	sb->obj_tree = btree_create(sb, 0);
	if (!sb->obj_tree) {
		KLOG(KL_ERR, "cant create obj btree");
		goto del_sb;
	}

	sb->obj_tree_block = btree_get_root_block(sb->obj_tree);
	memset(bh->b_data, 0, dev->bsize);
	ds_sb_fill_header(sb, (struct ds_image_header *)bh->b_data);

	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "sync 0block err %d", err);
		goto del_sb;
	}

	ds_sb_log(sb);
	*psb = sb;
	err = 0;
	goto free_bh;

del_sb:
	ds_sb_delete(sb);
free_bh:
	brelse(bh);
out:
	return err;
}

int ds_sb_load(struct ds_dev *dev, struct ds_sb **psb)
{
	struct buffer_head *bh;
	struct ds_sb *sb = NULL;
	int err;
	
	bh = __bread(dev->bdev, 0, dev->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;	
	}

	err = ds_sb_create(dev, (struct ds_image_header *)bh->b_data,
		&sb);
	if (err) {
		KLOG(KL_ERR, "cant create sb");
		goto free_bh;
	}

	if (sb->size > i_size_read(dev->bdev->bd_inode)) {
		KLOG(KL_ERR, "dev %p invalid size %llu sb %p", dev,
				sb->size, sb);
		err = -EINVAL;
		goto free_sb;
	}

	if (sb->obj_tree_block >= sb->nr_blocks) {
		KLOG(KL_ERR, "sb %p obj tree %llu nr_blocks %llud",
			sb, sb->obj_tree_block, sb->nr_blocks);
		err = -EINVAL;
		goto free_sb;
	}
	sb->obj_tree = btree_create(sb, sb->obj_tree_block);
	if (!sb->obj_tree) {
		KLOG(KL_ERR, "sb %p cant load obj tree %llu",
			sb, sb->obj_tree_block);
		err = -EINVAL;
		goto free_sb;
	}

	ds_sb_log(sb);
	*psb = sb;
	err = 0;
	goto free_bh;

free_sb:
	ds_sb_delete(sb);
free_bh:
	brelse(bh);
out:
	return err;
}

int ds_sb_insert_obj(struct ds_sb *sb, struct ds_obj_id *obj_id,
	u64 value, int replace)
{
	BUG_ON(!sb->obj_tree);
	BUG_ON(sizeof(struct ds_obj_id) != sizeof(struct btree_key));
	BUG_ON(sb->obj_tree->sig1 != BTREE_SIG1);

	return btree_insert_key(sb->obj_tree, (struct btree_key *)obj_id,
		value, replace);
}

int ds_sb_find_obj(struct ds_sb *sb, struct ds_obj_id *obj_id,
	u64 *pvalue)
{
	BUG_ON(!sb->obj_tree);
	BUG_ON(sizeof(struct ds_obj_id) != sizeof(struct btree_key));
	BUG_ON(sb->obj_tree->sig1 != BTREE_SIG1);

	return btree_find_key(sb->obj_tree, (struct btree_key *)obj_id,
		pvalue);
}

int ds_sb_delete_obj(struct ds_sb *sb, struct ds_obj_id *obj_id)
{
	BUG_ON(!sb->obj_tree);
	BUG_ON(sizeof(struct ds_obj_id) != sizeof(struct btree_key));
	BUG_ON(sb->obj_tree->sig1 != BTREE_SIG1);

	return btree_delete_key(sb->obj_tree, (struct btree_key *)obj_id);
}

int ds_sb_init(void)
{
	int err;
	
	ds_sb_cachep = kmem_cache_create("ds_sb_cache", sizeof(struct ds_sb), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!ds_sb_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	return 0;
out:
	return err;
}

void ds_sb_finit(void)
{
	kmem_cache_destroy(ds_sb_cachep);
}
