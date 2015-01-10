#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "inode"

struct kmem_cache *ds_inode_cachep;

static struct ds_inode *ds_inode_alloc(void)
{
	struct ds_inode *inode;

	inode = kmem_cache_alloc(ds_inode_cachep, GFP_NOFS);
	if (!inode) {
		KLOG(KL_ERR, "cant alloc inode");
		return NULL;
	}
	memset(inode, 0, sizeof(*inode));
	atomic_set(&inode->ref, 1);
	return inode;
}

static void ds_inode_free(struct ds_inode *inode)
{
	if (inode->blocks_tree)
		btree_deref(inode->blocks_tree);
	if (inode->blocks_sum_tree)
		btree_deref(inode->blocks_sum_tree);

	kmem_cache_free(ds_inode_cachep, inode);
}

void ds_inode_ref(struct ds_inode *inode)
{
	BUG_ON(atomic_read(&inode->ref) <= 0);
	atomic_inc(&inode->ref);
}

void ds_inode_deref(struct ds_inode *inode)
{
	BUG_ON(atomic_read(&inode->ref) <= 0);	
	if (atomic_dec_and_test(&inode->ref))
		ds_inode_free(inode);	
}

static struct ds_inode *__ds_inodes_lookup(struct ds_sb *sb, u64 block)
{
	struct rb_node *n = sb->inodes.rb_node;
	struct ds_inode *found = NULL;

	while (n) {
		struct ds_inode *inode;
		inode = rb_entry(n, struct ds_inode, inodes_link);
		if (block < inode->block) {
			n = n->rb_left;
		} else if (block > inode->block) {
			n = n->rb_right;
		} else {
			found = inode;
			break;
		}
	}
	return found;
}

static struct ds_inode * ds_inodes_lookup(struct ds_sb *sb, u64 block)
{	
	struct ds_inode *inode;
	read_lock_irq(&sb->inodes_lock);
	inode = __ds_inodes_lookup(sb, block);
	if (inode != NULL)
		INODE_REF(inode);
	read_unlock_irq(&sb->inodes_lock);
	return inode;
}

static void ds_inodes_remove(struct ds_sb *sb, struct ds_inode *inode)
{
	struct ds_inode *found;

	write_lock_irq(&sb->inodes_lock);
	found = __ds_inodes_lookup(sb, inode->block);
	BUG_ON(found != inode);
	rb_erase(&found->inodes_link, &sb->inodes);
	sb->inodes_active--;
	write_unlock_irq(&sb->inodes_lock);
}

static struct ds_inode *ds_inodes_insert(struct ds_sb *sb,
		struct ds_inode *inode)
{
	struct rb_node **p = &sb->inodes.rb_node;
	struct rb_node *parent = NULL;
	struct ds_inode *inserted = NULL;

	write_lock_irq(&sb->inodes_lock);
	while (*p) {
		struct ds_inode *found;
		parent = *p;
		found = rb_entry(parent, struct ds_inode, inodes_link);
		if (inode->block < found->block) {
			p = &(*p)->rb_left;
		} else if (inode->block > found->block) {
			p = &(*p)->rb_right;
		} else {
			inserted = found;
			break;
		}
	}
	if (!inserted) {
		rb_link_node(&inode->inodes_link, parent, p);
		rb_insert_color(&inode->inodes_link, &sb->inodes);
		sb->inodes_active++;
		inserted = inode;
	}
	INODE_REF(inserted);
	write_unlock_irq(&sb->inodes_lock);
	return inserted;
}

static void ds_inode_on_disk_sum(struct ds_inode_disk *on_disk,
		struct sha256_sum *sum)
{
	sha256((const unsigned char *)on_disk,
		offsetof(struct ds_inode_disk, sum), sum, 0); 
}

static void ds_inode_to_on_disk(struct ds_inode *inode,
	struct ds_inode_disk *on_disk)
{
	ds_obj_id_copy(&on_disk->ino, &inode->ino);
	on_disk->size = cpu_to_be64(inode->size);	
	on_disk->blocks_tree_block = cpu_to_be64(inode->blocks_tree_block);
	on_disk->blocks_sum_tree_block = 
		cpu_to_be64(inode->blocks_sum_tree_block);
	on_disk->sig1 = DS_INODE_SIG1;
	on_disk->sig2 = DS_INODE_SIG2;
	ds_inode_on_disk_sum(on_disk, &on_disk->sum);
}

static int ds_inode_parse_on_disk(struct ds_inode *inode,
	struct ds_inode_disk *on_disk)
{
	struct sha256_sum sum;
	if (be32_to_cpu(on_disk->sig1) != DS_INODE_SIG1 ||
		be32_to_cpu(on_disk->sig2) != DS_INODE_SIG2) {
		KLOG(KL_ERR, "invalid inode %llu sig",
			inode->block);
		return -EINVAL;
	}
	ds_inode_on_disk_sum(on_disk, &sum);
	if (0 != memcmp(&sum, &on_disk->sum, sizeof(sum))) {
		KLOG(KL_ERR, "invalid inode %llu sum",
			inode->block);
		return -EINVAL;
	}

	memcpy(&inode->ino, &on_disk->ino, sizeof(on_disk->ino));
	inode->size = be64_to_cpu(on_disk->size);
	inode->blocks_tree_block = be64_to_cpu(on_disk->blocks_tree_block);
	inode->blocks_sum_tree_block =
			be64_to_cpu(on_disk->blocks_sum_tree_block);

	return 0;
}

struct ds_inode *ds_inode_read(struct ds_sb *sb, u64 block)
{
	struct ds_inode *inode, *inserted;
	int err;

	inode =	ds_inodes_lookup(sb, block);
	if (inode) {
		return inode;	
	} else {
		struct buffer_head *bh;
		inode = ds_inode_alloc();
		if (!inode) {
			KLOG(KL_ERR, "no memory");
			return NULL;
		}

		inode->block = block;
		inode->sb = sb;
		bh = __bread(sb->bdev, inode->block, sb->bsize);
		if (!bh) {
			KLOG(KL_ERR, "cant read block %llu",
				inode->block);
			ds_inode_free(inode);
			return NULL;
		}

		err = ds_inode_parse_on_disk(inode,
			(struct ds_inode_disk *)bh->b_data);
		if (err) {
			KLOG(KL_ERR, "parse inode %llu err %d",
				inode->block, err);
			ds_inode_free(inode);
			brelse(bh);
			return NULL;
		}
		inode->blocks_tree = btree_create(sb,
			inode->blocks_tree_block);
		if (!inode->blocks_tree) {
			KLOG(KL_ERR, "cant create blocks tree inode %llu",
				inode->block);
			ds_inode_free(inode);
			brelse(bh);
			return NULL;
		}

		inode->blocks_sum_tree = btree_create(sb,
			inode->blocks_sum_tree_block);
		if (!inode->blocks_sum_tree) {
			KLOG(KL_ERR, "cant create blocks sum tree inode %llu",
				inode->block);
			ds_inode_free(inode);
			brelse(bh);
			return NULL;
		}

		inserted = ds_inodes_insert(sb, inode);
		if (inserted != inode) {
			ds_inode_free(inode);
			brelse(bh);
			return inserted;
		}
		brelse(bh);
		return inode;
	}
}

void ds_inode_delete(struct ds_inode *inode)
{
	if (inode->blocks_tree)
		btree_erase(inode->blocks_tree);
	if (inode->blocks_sum_tree)
		btree_erase(inode->blocks_sum_tree);

	ds_balloc_block_free(inode->sb, inode->block);
	ds_inodes_remove(inode->sb, inode);
	inode->block = 0;
}

static int ds_inode_write(struct ds_inode *inode)
{
	struct buffer_head *bh = NULL;
	int err;

	BUG_ON(!inode->block);
	BUG_ON(!inode->sb);

	bh = __bread(inode->sb->bdev, inode->block, inode->sb->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read inode %llu",
			inode->block);
		return -EIO;
	}
	ds_inode_to_on_disk(inode, (struct ds_inode_disk *)bh->b_data);
	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "cant sync inode %llu",
			inode->block);
	}
	brelse(bh);
	return err;
}

struct ds_inode *ds_inode_create(struct ds_sb *sb, struct ds_obj_id *ino)
{
	struct ds_inode *inode;
	struct ds_inode *inserted;
	int err;

	inode = ds_inode_alloc();
	if (!inode) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}
	inode->sb = sb;
	err = ds_balloc_block_alloc(sb, &inode->block);
	if (err) {
		KLOG(KL_ERR, "cant alloc block");
		goto ifree;
	}
	ds_obj_id_copy(&inode->ino, ino);

	inode->blocks_tree = btree_create(sb, 0);
	if (!inode->blocks_tree) {
		KLOG(KL_ERR, "cant create blocks tree inode %llu",
			inode->block);
		goto idelete;
	}

	inode->blocks_sum_tree = btree_create(sb, 0);
	if (!inode->blocks_sum_tree) {
		KLOG(KL_ERR, "cant create blocks sum tree inode %llu",
			inode->block);
		goto idelete;
	}

	inode->blocks_tree_block =
		btree_root_block(inode->blocks_tree);
	inode->blocks_sum_tree_block =
		btree_root_block(inode->blocks_sum_tree);

	inserted = ds_inodes_insert(sb, inode);
	BUG_ON(inserted != inode);
	if (inserted != inode) {
		KLOG(KL_ERR, "inode %p %llu exitst vs new %p %llu",
			inserted, inserted->block, inode, inode->block)			
		goto idelete;
	}

	err = ds_inode_write(inode);
	if (err) {
		KLOG(KL_ERR, "cant write inode %llu", inode->block);
		goto idelete;
	}

	return inode;

idelete:
	ds_inode_delete(inode);
ifree:
	ds_inode_free(inode);
	return NULL;
}

int ds_inode_read_buf(struct ds_inode *inode, u64 off, void *buf, u32 len)
{
	BUG();
	return -EINVAL;
}

int ds_inode_write_buf(struct ds_inode *inode, u64 off, void *buf, u32 len)
{
	BUG();
	return -EINVAL;
}

int ds_inode_init(void)
{
	ds_inode_cachep = kmem_cache_create("ds_inode_cache",
		sizeof(struct ds_inode), 0, SLAB_MEM_SPREAD, NULL);
	if (!ds_inode_cachep) {
		KLOG(KL_ERR, "cant create cache");
		return -ENOMEM;
	}

	return 0;
}

void ds_inode_finit(void)
{
	kmem_cache_destroy(ds_inode_cachep);
}
