#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "inode"

struct kmem_cache *ds_inode_cachep;

static void ds_inodes_remove(struct ds_sb *sb, struct ds_inode *inode);

static int __ds_inode_block_alloc(struct ds_inode *inode,
		u64 *pblock)
{
	return ds_balloc_block_alloc(inode->sb, pblock);
}

static int __ds_inode_block_free(struct ds_inode *inode,
		u64 block)
{
	return ds_balloc_block_free(inode->sb, block);
}

static struct ds_inode *ds_inode_alloc(void)
{
	struct ds_inode *inode;

	inode = kmem_cache_alloc(ds_inode_cachep, GFP_NOIO);
	if (!inode) {
		KLOG(KL_ERR, "cant alloc inode");
		return NULL;
	}
	memset(inode, 0, sizeof(*inode));
	atomic_set(&inode->ref, 1);
	init_rwsem(&inode->rw_sem);
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

static void ds_inode_release(struct ds_inode *inode)
{
	ds_inodes_remove(inode->sb, inode);
	ds_inode_free(inode);
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
		ds_inode_release(inode);	
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
	if (found) {
		BUG_ON(found != inode);
		rb_erase(&found->inodes_link, &sb->inodes);
		sb->inodes_active--;
	}
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
	sha256(on_disk,
		offsetof(struct ds_inode_disk, sum), sum, 0); 
}

static void ds_inode_set_size(struct ds_inode *inode,
	u64 size)
{
	if (inode->size != size) {
		inode->size = size;
		inode->dirty = 1;
	}
}

static void ds_inode_to_on_disk(struct ds_inode *inode,
	struct ds_inode_disk *on_disk)
{
	ds_obj_id_copy(&on_disk->ino, &inode->ino);
	on_disk->size = cpu_to_be64(inode->size);	
	on_disk->blocks_tree_block = cpu_to_be64(inode->blocks_tree_block);
	on_disk->blocks_sum_tree_block = 
		cpu_to_be64(inode->blocks_sum_tree_block);
	on_disk->sig1 = cpu_to_be32(DS_INODE_SIG1);
	on_disk->sig2 = cpu_to_be32(DS_INODE_SIG2);
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
	
	inode->sig1 = be32_to_cpu(on_disk->sig1);
	inode->sig2 = be32_to_cpu(on_disk->sig2);

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
		bh = __bread(sb->bdev, block, sb->bsize);
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

		inode->block = block;
		inserted = ds_inodes_insert(sb, inode);
		if (inserted != inode) {
			ds_inode_free(inode);
			brelse(bh);
			return inserted;
		} else {
			INODE_DEREF(inserted);
		}
		brelse(bh);
		return inode;
	}
}

static void inode_block_erase(struct btree_key *key,
	u64 value, void *ctx)
{
	struct ds_inode *inode = (struct ds_inode *)ctx;
	u64 block = value;
	__ds_inode_block_free(inode, block);
}

void ds_inode_delete(struct ds_inode *inode)
{
	down_write(&inode->rw_sem);
	if (inode->blocks_tree)
		btree_erase(inode->blocks_tree,
			inode_block_erase, inode);
	if (inode->blocks_sum_tree)
		btree_erase(inode->blocks_sum_tree,
			inode_block_erase, inode);

	ds_inodes_remove(inode->sb, inode);
	ds_balloc_block_free(inode->sb, inode->block);
	inode->block = 0;
	inode->size = 0;
	up_write(&inode->rw_sem);
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

static int ds_inode_write_dirty(struct ds_inode *inode)
{
	if (!inode->dirty) {
		return 0;
	} else {
		int err;
		err = ds_inode_write(inode);
		inode->dirty = 0;
		return err;
	}
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

	err = ds_inode_write(inode);
	if (err) {
		KLOG(KL_ERR, "cant write inode %llu", inode->block);
		goto idelete;
	}

	inserted = ds_inodes_insert(sb, inode);
	BUG_ON(inserted != inode);
	if (inserted != inode) {
		KLOG(KL_DBG, "inode %p %llu exitst vs new %p %llu",
			inserted, inserted->block, inode, inode->block);
		ds_inode_delete(inode);
		ds_inode_free(inode);
		inode = inserted;
		goto out;
	} else {
		INODE_DEREF(inserted);
	}

out:
	return inode;

idelete:
	ds_inode_delete(inode);
ifree:
	ds_inode_free(inode);
	return NULL;
}

static void ds_inode_block_to_sum_block(u64 block, u32 bsize,
		u64 *pblock, u32 *poff)
{
	u64 nbytes = block*sizeof(struct sha256_sum);
	*pblock = ds_div(nbytes, bsize);
	*poff = ds_mod(nbytes, bsize);
}

static int ds_inode_block_open_bhs(struct ds_inode *inode,
	struct inode_block *ib)
{
	BUG_ON(ib->bh || ib->sum_bh);

	ib->bh = __bread(inode->sb->bdev, ib->block, inode->sb->bsize);
	if (!ib->bh) {
		return -EIO;
	}

	ib->sum_bh = __bread(inode->sb->bdev, ib->sum_block, inode->sb->bsize);
	if (!ib->sum_bh) {
		return -EIO;
	}

	return 0;
}

static void ds_inode_block_relse(struct inode_block *ib)
{
	if (ib->bh)
		brelse(ib->bh);
	if (ib->sum_bh)
		brelse(ib->sum_bh);
}

static void ds_inode_block_zero(struct inode_block *ib)
{
	memset(ib, 0, sizeof(*ib));
}

static void ds_inode_buf_sum(void *buf, u32 len, struct sha256_sum *sum)
{
	sha256(buf, len, sum, 0);
}

static int ds_inode_block_write(struct ds_inode *inode,
		struct inode_block *ib)
{
	int err;

	BUG_ON(!ib->bh || !ib->sum_bh);

	ds_inode_buf_sum(ib->bh->b_data, inode->sb->bsize,
		(struct sha256_sum *)(ib->sum_bh->b_data + ib->sum_off));
	mark_buffer_dirty(ib->bh);
	mark_buffer_dirty(ib->sum_bh);
	err = sync_dirty_buffer(ib->bh);
	if (!err)
		err = sync_dirty_buffer(ib->sum_bh);

	return err;
}

static int ds_inode_block_alloc(struct ds_inode *inode,
	u64 vblock,	
	struct inode_block *pib)
{
	int err;
	struct inode_block ib;
	struct btree_key key;
	int sum_block_allocated = 0;
	int sum_block_inserted = 0;

	ds_inode_block_zero(&ib);
	ib.vblock = vblock;

	err = __ds_inode_block_alloc(inode, &ib.block);
	if (err) {
		return err;
	}

	ds_inode_block_to_sum_block(ib.vblock, inode->sb->bsize,
		&ib.vsum_block, &ib.sum_off);

	btree_key_by_u64(ib.vsum_block, &key);
	err = btree_find_key(inode->blocks_sum_tree, &key, &ib.sum_block);
	if (err) {
		err = __ds_inode_block_alloc(inode, &ib.sum_block);
		if (err)
			goto fail;

		sum_block_allocated = 1;

		btree_key_by_u64(ib.vsum_block, &key);
		err = btree_insert_key(inode->blocks_sum_tree, &key,
			ib.sum_block, 0);
		if (err)
			goto fail;

		sum_block_inserted = 1;
	}

	err = ds_inode_block_open_bhs(inode, &ib);
	if (err)
		goto fail;


	btree_key_by_u64(ib.vblock, &key);	
	err = btree_insert_key(inode->blocks_tree, &key,
		ib.block, 0);
	if (err)
		goto fail;

	memcpy(pib, &ib, sizeof(ib));
	return 0;

fail:
	if (sum_block_inserted) {
		btree_key_by_u64(ib.vsum_block, &key);	
		btree_delete_key(inode->blocks_sum_tree, &key);
	}

	if (sum_block_allocated)
		__ds_inode_block_free(inode, ib.sum_block);

	__ds_inode_block_free(inode, ib.block);
	ds_inode_block_relse(&ib);
	return err;
}

static void
ds_inode_block_erase(struct ds_inode *inode,
	struct inode_block *ib)
{
	struct btree_key key;

	btree_key_by_u64(ib->vblock, &key);
	btree_delete_key(inode->blocks_tree, &key);

	__ds_inode_block_free(inode, ib->block);
}

static int
ds_inode_block_check_sum(struct ds_inode *inode,
	struct inode_block *ib)
{
	struct sha256_sum sum;
	BUG_ON(!ib->bh || !ib->sum_bh);

	ds_inode_buf_sum(ib->bh->b_data, inode->sb->bsize, &sum);
	if (0 != memcmp(ib->sum_bh->b_data + ib->sum_off, &sum, sizeof(sum))) {
		KLOG(KL_ERR, "invalid sum i[%llu] vb%llu b%llu",
			inode->block, ib->vblock, ib->block);
		return -EINVAL;
	}
	return 0;
}

static int
ds_inode_block_read(struct ds_inode *inode, u64 vblock,
	struct inode_block *pib)
{
	int err;
	struct btree_key key;
	struct inode_block ib;
	
	ds_inode_block_zero(&ib);
	ib.vblock = vblock;
	btree_key_by_u64(ib.vblock, &key);	
	err = btree_find_key(inode->blocks_tree, &key, &ib.block);
	if (err) {
		return err;
	}

	ds_inode_block_to_sum_block(ib.vblock, inode->sb->bsize,
		&ib.vsum_block, &ib.sum_off);

	btree_key_by_u64(ib.vsum_block, &key);
	err = btree_find_key(inode->blocks_sum_tree, &key, &ib.sum_block);
	if (err) {
		KLOG(KL_ERR, "cant find vb %llu", ib.vsum_block);
		goto fail;
	}

	err = ds_inode_block_open_bhs(inode, &ib);
	if (err)
		goto fail;

	memcpy(pib, &ib, sizeof(ib));
	return 0;
fail:
	ds_inode_block_relse(&ib);
	return err;
}

static int
ds_inode_block_read_create(struct ds_inode *inode, u64 vblock,
	struct inode_block *pib)
{
	int err;
	struct inode_block ib;
	
	ds_inode_block_zero(&ib);
	err = ds_inode_block_read(inode, vblock, &ib);
	if (err) {
		err = ds_inode_block_alloc(inode, vblock, &ib);		
		if (err)
			goto fail;
		err = ds_inode_block_write(inode, &ib);
		if (err) {
			ds_inode_block_erase(inode, &ib);
			goto fail;		
		}
		goto success;
	}
	err = ds_inode_block_check_sum(inode, &ib);
	if (err)
		goto fail;

success:
	memcpy(pib, &ib, sizeof(ib));
	return 0;
fail:
	ds_inode_block_relse(&ib);	
	return err;
}

static int
ds_inode_read_block_buf(struct ds_inode *inode, u64 vblock,
	u32 off, void *buf, u32 len, u32 *pio_count, u32 *peof)
{
	int err;
	struct inode_block ib;
	u64 data_off;
	u32 llen;

	BUG_ON(((u64)off + (u64)len) > inode->sb->bsize);
	*pio_count = 0;
	*peof = 0;

	down_read(&inode->rw_sem);
	data_off = vblock*inode->sb->bsize + off;
	if (data_off > inode->size) {
		up_read(&inode->rw_sem);
		KLOG(KL_ERR, "inode %p %llu doff %llu isize %llu",
			inode, inode->block, data_off, inode->size);
		return -ERANGE;
	} else if (data_off == inode->size) {
		up_read(&inode->rw_sem);
		*peof = 1;	
		return 0;
	}

	err = ds_inode_block_read(inode, vblock, &ib);
	if (err) {
		KLOG(KL_ERR, "cant read vb %llu err %d", vblock, err);
		return err;
	}

	err = ds_inode_block_check_sum(inode, &ib);
	if (err) {
		KLOG(KL_ERR, "check sum b %llu vb %llu err %d",
			ib.block, ib.vblock, err);
		goto out;
	}

	KLOG(KL_DBG1, "vb %llu off %u len %u b %llu",
		vblock, off, len, ib.block);

	down_read(&inode->rw_sem);
	if ((data_off + len) >= inode->size) {
		llen = inode->size - data_off;
		*peof = 1;
	} else {
		llen = len;
	}
	up_read(&inode->rw_sem);

	memcpy(buf, ib.bh->b_data + off, llen);
	*pio_count = llen;
	err = 0;
out:
	ds_inode_block_relse(&ib);
	return err;
}

static int
ds_inode_write_block_buf(struct ds_inode *inode, u64 vblock,
	u32 off, void *buf, u32 len, u32 *pio_count, int *peof)
{
	int err;
	struct inode_block ib;
	u64 data_end;


	BUG_ON(((u64)off + (u64)len) > inode->sb->bsize);
	*peof = 0;
	*pio_count = 0;

	err = ds_inode_block_read_create(inode, vblock, &ib);
	if (err) {
		KLOG(KL_ERR, "cant read vblock %llu", vblock);
		return err;
	}

	KLOG(KL_DBG1, "vb %llu off %u len %u b %llu",
		vblock, off, len, ib.block);

	memcpy(ib.bh->b_data + off, buf, len);
	err = ds_inode_block_write(inode, &ib);
	if (err) {
		KLOG(KL_ERR, "cant write to b %llu vb %llu",
			ib.block, ib.vblock);
		goto out;
	}

	data_end = vblock*inode->sb->bsize + off + len;
	down_write(&inode->rw_sem);
	if (data_end > inode->size) {
		ds_inode_set_size(inode, data_end);
		err = ds_inode_write_dirty(inode);
		if (err) {
			KLOG(KL_ERR, "cant update inode %llu",
				inode->block);
			up_write(&inode->rw_sem);
			goto out;
		}
	}
	up_write(&inode->rw_sem);

	*pio_count = len;
	err = 0;
out:
	ds_inode_block_relse(&ib);
	return err;
}

static int ds_inode_io_buf(struct ds_inode *inode, u64 off, void *buf, u32 len,
		int write, u32 *pio_count, int *peof)
{
	int err;
	u64 vblock = ds_div(off, inode->sb->bsize);
	u32 loff = ds_mod(off, inode->sb->bsize);
	char *pos = (char *)buf;
	u32 llen, res = len;
	u32 io_count, io_count_sum;
	int eof = 0;

	*peof = 0;
	io_count_sum = 0;
	while (res > 0) {
		llen = ((res + loff) > inode->sb->bsize) ?
			(inode->sb->bsize - loff) : res;
		if (write) {
			err = ds_inode_write_block_buf(inode, vblock, loff,
							pos, llen, &io_count, &eof);
		} else {
			err = ds_inode_read_block_buf(inode, vblock, loff,
							pos, llen, &io_count, &eof);
		}
		if (err)
			goto out;
		io_count_sum+= io_count;
		if (eof)
			break;
		BUG_ON(llen != io_count);
		pos+= llen;
		res-= llen;
		loff = 0;
		vblock++;
	}

	*peof = eof;
	*pio_count = io_count_sum;
	err = 0;
out:
	return err;
}

int ds_inode_io_pages(struct ds_inode *inode, u64 off, u32 pg_off, u32 len,
		struct page **pages, int nr_pages, int write, u32 *pio_count)
{
	int err;
	int i;
	void *buf;
	u32 llen;
	u32 io_count, io_count_sum;
	int eof;

	KLOG(KL_DBG1, "off %llu pg_off %u len %u nr_pages %d write %d",
		off, pg_off, len, nr_pages, write);

	io_count_sum = 0;
	i = 0;
	while (len > 0) {
		if (i >= nr_pages) {
			KLOG(KL_ERR, "overflow pages");
			err = -EINVAL;
			goto fail;
		}
		buf = kmap(pages[i]);
		llen = ((len + pg_off) > PAGE_SIZE) ? (PAGE_SIZE - pg_off) : len;
		err = ds_inode_io_buf(inode, off, (void *)((unsigned long)buf + pg_off),
			llen, write, &io_count, &eof);
		kunmap(pages[i]);
		if (err)
			goto fail;
		io_count_sum+= io_count;
		if (eof)
			break;
		BUG_ON(io_count != llen);
		pg_off = 0;
		off+= llen;
		len-= llen;
		i++;
	}

	*pio_count = io_count_sum;
	return 0;
fail:
	return err;
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
