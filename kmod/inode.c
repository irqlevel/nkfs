#define CREATE_TRACE_POINTS

#include "inc/nkfs_priv.h"

#define __SUBCOMPONENT__ "inode"

struct kmem_cache *nkfs_inode_cachep;
struct kmem_cache *nkfs_inode_disk_cachep;

static void nkfs_inodes_remove(struct nkfs_sb *sb, struct nkfs_inode *inode);

static int __nkfs_inode_block_alloc(struct nkfs_inode *inode,
		u64 *pblock)
{
	return nkfs_balloc_block_alloc(inode->sb, pblock);
}

static int __nkfs_inode_block_free(struct nkfs_inode *inode,
		u64 block)
{
	return nkfs_balloc_block_free(inode->sb, block);
}

static struct nkfs_inode *nkfs_inode_alloc(void)
{
	struct nkfs_inode *inode;

	inode = kmem_cache_alloc(nkfs_inode_cachep, GFP_NOIO);
	if (!inode) {
		KLOG(KL_ERR, "cant alloc inode");
		return NULL;
	}
	memset(inode, 0, sizeof(*inode));
	atomic_set(&inode->ref, 1);
	init_rwsem(&inode->rw_sem);
	return inode;
}

static void nkfs_inode_free(struct nkfs_inode *inode)
{
	if (inode->blocks_tree)
		nkfs_btree_deref(inode->blocks_tree);
	if (inode->blocks_sum_tree)
		nkfs_btree_deref(inode->blocks_sum_tree);

	kmem_cache_free(nkfs_inode_cachep, inode);
}

static void nkfs_inode_release(struct nkfs_inode *inode)
{
	nkfs_inodes_remove(inode->sb, inode);
	nkfs_inode_free(inode);
}

void nkfs_inode_ref(struct nkfs_inode *inode)
{
	NKFS_BUG_ON(atomic_read(&inode->ref) <= 0);
	atomic_inc(&inode->ref);
}

void nkfs_inode_deref(struct nkfs_inode *inode)
{
	NKFS_BUG_ON(atomic_read(&inode->ref) <= 0);	
	if (atomic_dec_and_test(&inode->ref))
		nkfs_inode_release(inode);	
}

static struct nkfs_inode *__nkfs_inodes_lookup(struct nkfs_sb *sb, u64 block)
{
	struct rb_node *n = sb->inodes.rb_node;
	struct nkfs_inode *found = NULL;

	while (n) {
		struct nkfs_inode *inode;
		inode = rb_entry(n, struct nkfs_inode, inodes_link);
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

static struct nkfs_inode * nkfs_inodes_lookup(struct nkfs_sb *sb, u64 block)
{	
	struct nkfs_inode *inode;
	read_lock_irq(&sb->inodes_lock);
	inode = __nkfs_inodes_lookup(sb, block);
	if (inode != NULL)
		INODE_REF(inode);
	read_unlock_irq(&sb->inodes_lock);
	return inode;
}

static void nkfs_inodes_remove(struct nkfs_sb *sb, struct nkfs_inode *inode)
{
	struct nkfs_inode *found;

	write_lock_irq(&sb->inodes_lock);
	found = __nkfs_inodes_lookup(sb, inode->block);
	if (found) {
		NKFS_BUG_ON(found != inode);
		rb_erase(&found->inodes_link, &sb->inodes);
		sb->inodes_active--;
	}
	write_unlock_irq(&sb->inodes_lock);
}

static struct nkfs_inode *nkfs_inodes_insert(struct nkfs_sb *sb,
		struct nkfs_inode *inode)
{
	struct rb_node **p = &sb->inodes.rb_node;
	struct rb_node *parent = NULL;
	struct nkfs_inode *inserted = NULL;

	write_lock_irq(&sb->inodes_lock);
	while (*p) {
		struct nkfs_inode *found;
		parent = *p;
		found = rb_entry(parent, struct nkfs_inode, inodes_link);
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

static void nkfs_inode_on_disk_sum(struct nkfs_inode_disk *on_disk,
		struct csum *sum)
{
	struct csum_ctx ctx;

	csum_reset(&ctx);
	csum_update(&ctx, on_disk, offsetof(struct nkfs_inode_disk, sum));
	csum_digest(&ctx, sum);
}

static void nkfs_inode_set_size(struct nkfs_inode *inode,
	u64 size)
{
	if (inode->size != size) {
		inode->size = size;
		inode->dirty = 1;
	}
}

static void nkfs_inode_to_on_disk(struct nkfs_inode *inode,
	struct nkfs_inode_disk *on_disk)
{
	nkfs_obj_id_copy(&on_disk->ino, &inode->ino);
	on_disk->size = cpu_to_be64(inode->size);	
	on_disk->blocks_tree_block = cpu_to_be64(inode->blocks_tree_block);
	on_disk->blocks_sum_tree_block = 
		cpu_to_be64(inode->blocks_sum_tree_block);
	on_disk->sig1 = cpu_to_be32(NKFS_INODE_SIG1);
	on_disk->sig2 = cpu_to_be32(NKFS_INODE_SIG2);
	nkfs_inode_on_disk_sum(on_disk, &on_disk->sum);
}

static int nkfs_inode_parse_on_disk(struct nkfs_inode *inode,
	struct nkfs_inode_disk *on_disk)
{
	struct csum sum;
	if (be32_to_cpu(on_disk->sig1) != NKFS_INODE_SIG1 ||
		be32_to_cpu(on_disk->sig2) != NKFS_INODE_SIG2) {
		KLOG(KL_ERR, "invalid inode %llu sig",
			inode->block);
		return -EINVAL;
	}
	nkfs_inode_on_disk_sum(on_disk, &sum);
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

struct nkfs_inode *nkfs_inode_read(struct nkfs_sb *sb, u64 block)
{
	struct nkfs_inode *inode, *inserted;
	struct dio_cluster *clu;
	struct nkfs_inode_disk *inode_disk;
	int err;

	inode =	nkfs_inodes_lookup(sb, block);
	if (inode) {
		return inode;	
	}
	
	inode = nkfs_inode_alloc();
	if (!inode) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}
	
	inode->block = block;
	inode->sb = sb;

	clu = dio_clu_get(sb->ddev, block);
	if (!clu) {
		KLOG(KL_ERR, "cant read block %llu",
			inode->block);
		goto free_inode;
	}

	inode_disk = kmem_cache_alloc(nkfs_inode_disk_cachep, GFP_NOIO);
	if (!inode_disk) {
		KLOG(KL_ERR, "no mem");
		goto put_clu;
	}

	err = dio_clu_read(clu, inode_disk, sizeof(*inode_disk), 0);
	if (err) {
		KLOG(KL_ERR, "cant read %llu", inode->block);
		goto free_idisk;
	}

	err = nkfs_inode_parse_on_disk(inode, inode_disk);
	if (err) {
		KLOG(KL_ERR, "parse inode %llu err %d",
			inode->block, err);
		goto free_idisk;
	}

	inode->blocks_tree = nkfs_btree_create(sb,
			inode->blocks_tree_block);
	if (!inode->blocks_tree) {
		KLOG(KL_ERR, "cant create blocks tree inode %llu",
			inode->block);
		goto free_idisk;
	}

	inode->blocks_sum_tree = nkfs_btree_create(sb,
		inode->blocks_sum_tree_block);
	if (!inode->blocks_sum_tree) {
		KLOG(KL_ERR, "cant create blocks sum tree inode %llu",
			inode->block);
		goto free_idisk;
	}

	inode->block = block;
	inserted = nkfs_inodes_insert(sb, inode);
	if (inserted != inode) {
		kmem_cache_free(nkfs_inode_disk_cachep, inode_disk);
		dio_clu_put(clu);
		nkfs_inode_free(inode);
		return inserted;
	} else {
		INODE_DEREF(inserted);
	}

	kmem_cache_free(nkfs_inode_disk_cachep, inode_disk);
	dio_clu_put(clu);
	return inode;

free_idisk:
	kmem_cache_free(nkfs_inode_disk_cachep, inode_disk);
put_clu:
	dio_clu_put(clu);
free_inode:
	nkfs_inode_free(inode);
	return NULL;
}

static void inode_block_erase(struct nkfs_btree_key *key,
	struct nkfs_btree_value *value, void *ctx)
{
	struct nkfs_inode *inode = (struct nkfs_inode *)ctx;
	u64 block = value->val;
	__nkfs_inode_block_free(inode, block);
}

void nkfs_inode_delete(struct nkfs_inode *inode)
{
	down_write(&inode->rw_sem);
	if (inode->blocks_tree)
		nkfs_btree_erase(inode->blocks_tree,
			inode_block_erase, inode);
	if (inode->blocks_sum_tree)
		nkfs_btree_erase(inode->blocks_sum_tree,
			inode_block_erase, inode);

	nkfs_inodes_remove(inode->sb, inode);
	nkfs_balloc_block_free(inode->sb, inode->block);
	trace_balloc_block_free(inode);
	inode->block = 0;
	inode->size = 0;
	up_write(&inode->rw_sem);
}

static int nkfs_inode_write(struct nkfs_inode *inode)
{
	struct dio_cluster *clu;	
	struct nkfs_inode_disk *idisk;
	int err;

	NKFS_BUG_ON(!inode->block);
	NKFS_BUG_ON(!inode->sb);

	clu = dio_clu_get(inode->sb->ddev, inode->block);
	if (!clu) {
		KLOG(KL_ERR, "cant read inode %llu",
			inode->block);
		return -EIO;
	}
	idisk = kmem_cache_alloc(nkfs_inode_disk_cachep, GFP_NOIO);
	if (!idisk) {
		KLOG(KL_ERR, "no memory");
		dio_clu_put(clu);
		return -ENOMEM;
	}

	nkfs_inode_to_on_disk(inode, idisk);

	err = dio_clu_write(clu, idisk, sizeof(*idisk), 0);
	if (err) {
		KLOG(KL_ERR, "cant write inode %llu",
			inode->block);
		goto cleanup;	
	}

	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "cant sync inode %llu",
			inode->block);
	}

cleanup:
	kmem_cache_free(nkfs_inode_disk_cachep, idisk);
	dio_clu_put(clu);

	return err;
}

static int nkfs_inode_write_dirty(struct nkfs_inode *inode)
{
	if (!inode->dirty) {
		return 0;
	} else {
		int err;
		err = nkfs_inode_write(inode);
		inode->dirty = 0;
		return err;
	}
}

struct nkfs_inode *nkfs_inode_create(struct nkfs_sb *sb, struct nkfs_obj_id *ino)
{
	struct nkfs_inode *inode;
	struct nkfs_inode *inserted;
	int err;

	inode = nkfs_inode_alloc();
	if (!inode) {
		KLOG(KL_ERR, "no memory");
		return NULL;
	}

	inode->sb = sb;
	err = nkfs_balloc_block_alloc(sb, &inode->block);
	if (err) {
		KLOG(KL_ERR, "cant alloc block");
		goto ifree;
	}
	NKFS_BUG_ON(!inode->block);
	trace_balloc_block_alloc(inode);
	nkfs_obj_id_copy(&inode->ino, ino);

	inode->blocks_tree = nkfs_btree_create(sb, 0);
	if (!inode->blocks_tree) {
		KLOG(KL_ERR, "cant create blocks tree inode %llu",
			inode->block);
		goto idelete;
	}

	inode->blocks_sum_tree = nkfs_btree_create(sb, 0);
	if (!inode->blocks_sum_tree) {
		KLOG(KL_ERR, "cant create blocks sum tree inode %llu",
			inode->block);
		goto idelete;
	}

	inode->blocks_tree_block =
		nkfs_btree_root_block(inode->blocks_tree);
	inode->blocks_sum_tree_block =
		nkfs_btree_root_block(inode->blocks_sum_tree);
	trace_inode_create(inode);
	err = nkfs_inode_write(inode);
	if (err) {
		KLOG(KL_ERR, "cant write inode %llu", inode->block);
		goto idelete;
	}

	inserted = nkfs_inodes_insert(sb, inode);
	NKFS_BUG_ON(inserted != inode);
	if (inserted != inode) {
		KLOG(KL_DBG, "inode %p %llu exitst vs new %p %llu",
			inserted, inserted->block, inode, inode->block);
		nkfs_inode_delete(inode);
		nkfs_inode_free(inode);
		inode = inserted;
		goto out;
	} else {
		INODE_DEREF(inserted);
	}

out:
	return inode;

idelete:
	nkfs_inode_delete(inode);
ifree:
	nkfs_inode_free(inode);
	return NULL;
}

static void nkfs_inode_block_to_sum_block(u64 block, u32 bsize,
		u64 *pblock, u32 *poff)
{
	u64 nbytes = block*sizeof(struct csum);
	*pblock = nkfs_div(nbytes, bsize);
	*poff = nkfs_mod(nbytes, bsize);
}

static int nkfs_inode_block_open_clus(struct nkfs_inode *inode,
	struct inode_block *ib)
{
	NKFS_BUG_ON(ib->clu || ib->sum_clu);

	ib->clu = dio_clu_get(inode->sb->ddev, ib->block);
	if (!ib->clu) {
		return -EIO;
	}

	ib->sum_clu = dio_clu_get(inode->sb->ddev, ib->sum_block);
	if (!ib->sum_clu) {
		return -EIO;
	}

	return 0;
}

static void nkfs_inode_block_relse(struct inode_block *ib)
{
	if (ib->clu)
		dio_clu_put(ib->clu);
	if (ib->sum_clu)
		dio_clu_put(ib->sum_clu);
}

static void nkfs_inode_block_zero(struct inode_block *ib)
{
	memset(ib, 0, sizeof(*ib));
}

static int nkfs_inode_block_write(struct nkfs_inode *inode,
		struct inode_block *ib)
{
	int err;

	NKFS_BUG_ON(!ib->clu || !ib->sum_clu);
	trace_inode_write_block(inode, ib);

	dio_clu_sum(ib->clu,
		(struct csum *)dio_clu_map(ib->sum_clu, ib->sum_off));

	dio_clu_set_dirty(ib->clu);
	dio_clu_set_dirty(ib->sum_clu);

	err = dio_clu_sync(ib->clu);
	if (!err)
		err = dio_clu_sync(ib->sum_clu);

	return err;
}

static int nkfs_inode_block_alloc(struct nkfs_inode *inode,
	u64 vblock,	
	struct inode_block *pib)
{
	int err;
	struct inode_block ib;
	struct nkfs_btree_key key;
	int sum_block_allocated = 0;
	int sum_block_inserted = 0;

	nkfs_inode_block_zero(&ib);
	nkfs_inode_block_zero(pib);

	ib.vblock = vblock;

	err = __nkfs_inode_block_alloc(inode, &ib.block);
	if (err) {
		KLOG(KL_ERR, "inode %llu balloc err %d",
			inode->block, err);
		return err;
	}

	nkfs_inode_block_to_sum_block(ib.vblock, inode->sb->bsize,
		&ib.vsum_block, &ib.sum_off);

	nkfs_btree_key_by_u64(ib.vsum_block, &key);
	err = nkfs_btree_find_key(inode->blocks_sum_tree, &key,
		(struct nkfs_btree_value *)&ib.sum_block);
	if (err) {
		err = __nkfs_inode_block_alloc(inode, &ib.sum_block);
		if (err) {
			KLOG(KL_ERR, "balloc sum block inode %llu err %d",
				inode->block, err);
			goto fail;
		}

		sum_block_allocated = 1;

		nkfs_btree_key_by_u64(ib.vsum_block, &key);
		err = nkfs_btree_insert_key(inode->blocks_sum_tree, &key,
			(struct nkfs_btree_value *)&ib.sum_block, 0);
		if (err) {
			KLOG(KL_ERR, "cant inode %llu insert key %llu",
				inode->block, ib.vsum_block);
			goto fail;
		}

		sum_block_inserted = 1;
	}

	err = nkfs_inode_block_open_clus(inode, &ib);
	if (err) {
		KLOG(KL_ERR, "inode %llu err %d",
			inode->block, err);
		goto fail;
	}


	nkfs_btree_key_by_u64(ib.vblock, &key);	
	err = nkfs_btree_insert_key(inode->blocks_tree, &key,
		(struct nkfs_btree_value *)&ib.block, 0);
	if (err) {
		KLOG(KL_ERR, "cant insert key inode %llu err %d",
			inode->block, err);
		goto fail;
	}

	memcpy(pib, &ib, sizeof(ib));
	return 0;

fail:
	if (sum_block_inserted) {
		nkfs_btree_key_by_u64(ib.vsum_block, &key);	
		nkfs_btree_delete_key(inode->blocks_sum_tree, &key);
	}

	if (sum_block_allocated)
		__nkfs_inode_block_free(inode, ib.sum_block);

	__nkfs_inode_block_free(inode, ib.block);
	nkfs_inode_block_relse(&ib);
	return err;
}

static void
nkfs_inode_block_erase(struct nkfs_inode *inode,
	struct inode_block *ib)
{
	struct nkfs_btree_key key;

	nkfs_btree_key_by_u64(ib->vblock, &key);
	nkfs_btree_delete_key(inode->blocks_tree, &key);

	__nkfs_inode_block_free(inode, ib->block);
}

static int
nkfs_inode_block_check_sum(struct nkfs_inode *inode,
	struct inode_block *ib)
{
	struct csum sum;
	NKFS_BUG_ON(!ib->clu || !ib->sum_clu);

	dio_clu_sum(ib->clu, &sum);
	if (0 != memcmp(dio_clu_map(ib->sum_clu, ib->sum_off), &sum, sizeof(sum))) {
		KLOG(KL_ERR, "invalid sum i[%llu] vb%llu b%llu",
			inode->block, ib->vblock, ib->block);
		return -EINVAL;
	}

	return 0;
}

static int
nkfs_inode_block_read(struct nkfs_inode *inode, u64 vblock,
	struct inode_block *pib)
{
	int err;
	struct nkfs_btree_key key;
	struct inode_block ib;
	
	nkfs_inode_block_zero(&ib);
	nkfs_inode_block_zero(pib);

	ib.vblock = vblock;
	nkfs_btree_key_by_u64(ib.vblock, &key);	
	err = nkfs_btree_find_key(inode->blocks_tree, &key,
		(struct nkfs_btree_value *)&ib.block);
	if (err) {
		return err;
	}

	nkfs_inode_block_to_sum_block(ib.vblock, inode->sb->bsize,
		&ib.vsum_block, &ib.sum_off);

	nkfs_btree_key_by_u64(ib.vsum_block, &key);
	err = nkfs_btree_find_key(inode->blocks_sum_tree, &key,
		(struct nkfs_btree_value *)&ib.sum_block);
	if (err) {
		KLOG(KL_ERR, "cant find vb %llu", ib.vsum_block);
		goto fail;
	}

	err = nkfs_inode_block_open_clus(inode, &ib);
	if (err)
		goto fail;

	memcpy(pib, &ib, sizeof(ib));
	return 0;
fail:
	nkfs_inode_block_relse(&ib);
	return err;
}

static int
nkfs_inode_block_read_create(struct nkfs_inode *inode, u64 vblock,
	struct inode_block *pib)
{
	int err;
	struct inode_block ib;
	
	nkfs_inode_block_zero(&ib);
	nkfs_inode_block_zero(pib);

	err = nkfs_inode_block_read(inode, vblock, &ib);
	if (err) {
		err = nkfs_inode_block_alloc(inode, vblock, &ib);		
		if (err) {
			KLOG(KL_ERR, "cant alloc block vblock %llu inode %llu",
				vblock, inode->block);
			goto fail;
		}
		err = nkfs_inode_block_write(inode, &ib);
		if (err) {
			KLOG(KL_ERR, "inode %llu ib.block %llu err %d",
				inode->block, ib.block, err);
			nkfs_inode_block_erase(inode, &ib);
			goto fail;
		}
	} else {
		err = nkfs_inode_block_check_sum(inode, &ib);
		if (err)
			goto fail;
	}

	memcpy(pib, &ib, sizeof(ib));
	return 0;
fail:
	nkfs_inode_block_relse(&ib);	
	return err;
}

static int
nkfs_inode_read_block_buf(struct nkfs_inode *inode, u64 vblock,
	u32 off, void *buf, u32 len, u32 *pio_count, u32 *peof)
{
	int err;
	struct inode_block ib;
	u64 data_off;
	u32 llen;

	NKFS_BUG_ON(((u64)off + (u64)len) > inode->sb->bsize);
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

	err = nkfs_inode_block_read(inode, vblock, &ib);
	if (err) {
		KLOG(KL_ERR, "cant read vb %llu err %d", vblock, err);
		return err;
	}

	err = nkfs_inode_block_check_sum(inode, &ib);
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

	err = dio_clu_read(ib.clu, buf, llen, off);
	if (err) {
		KLOG(KL_ERR, "cant read clu err %d", err);
		goto out;
	}

	*pio_count = llen;
	err = 0;
out:
	nkfs_inode_block_relse(&ib);
	return err;
}

static int
nkfs_inode_write_block_buf(struct nkfs_inode *inode, u64 vblock,
	u32 off, void *buf, u32 len, u32 *pio_count, int *peof)
{
	int err;
	struct inode_block ib;
	u64 data_end;


	NKFS_BUG_ON(((u64)off + (u64)len) > inode->sb->bsize);
	*peof = 0;
	*pio_count = 0;

	err = nkfs_inode_block_read_create(inode, vblock, &ib);
	if (err) {
		KLOG(KL_ERR, "cant read vblock %llu", vblock);
		return err;
	}

	KLOG(KL_DBG1, "vb %llu off %u len %u b %llu",
		vblock, off, len, ib.block);

	err = dio_clu_write(ib.clu, buf, len, off);
	if (err) {
		KLOG(KL_ERR, "cant write clu err %d", err);
		goto out;
	}

	err = dio_clu_sync(ib.clu);
	if (err) {
		KLOG(KL_ERR, "cant sync clu err %d", err);
		goto out;
	}

	err = nkfs_inode_block_write(inode, &ib);
	if (err) {
		KLOG(KL_ERR, "cant write to b %llu vb %llu",
			ib.block, ib.vblock);
		goto out;
	}

	data_end = vblock*inode->sb->bsize + off + len;
	down_write(&inode->rw_sem);
	if (data_end > inode->size) {
		nkfs_inode_set_size(inode, data_end);
		err = nkfs_inode_write_dirty(inode);
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
	nkfs_inode_block_relse(&ib);
	return err;
}

static int nkfs_inode_io_buf(struct nkfs_inode *inode, u64 off, void *buf, u32 len,
		int write, u32 *pio_count, int *peof)
{
	int err;
	u64 vblock = nkfs_div(off, inode->sb->bsize);
	u32 loff = nkfs_mod(off, inode->sb->bsize);
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
			err = nkfs_inode_write_block_buf(inode, vblock, loff,
							pos, llen, &io_count, &eof);
		} else {
			err = nkfs_inode_read_block_buf(inode, vblock, loff,
							pos, llen, &io_count, &eof);
		}
		if (err)
			goto out;
		io_count_sum+= io_count;
		if (eof)
			break;
		NKFS_BUG_ON(llen != io_count);
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

int nkfs_inode_io_pages(struct nkfs_inode *inode, u64 off, u32 pg_off, u32 len,
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
		err = nkfs_inode_io_buf(inode, off, (void *)((unsigned long)buf + pg_off),
			llen, write, &io_count, &eof);
		kunmap(pages[i]);
		if (err)
			goto fail;
		io_count_sum+= io_count;
		if (eof)
			break;
		NKFS_BUG_ON(io_count != llen);
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

int nkfs_inode_init(void)
{
	int err;
	nkfs_inode_cachep = kmem_cache_create("nkfs_inode_cache",
		sizeof(struct nkfs_inode), 0, SLAB_MEM_SPREAD, NULL);
	if (!nkfs_inode_cachep) {
		KLOG(KL_ERR, "cant create cache");
		return -ENOMEM;
	}

	nkfs_inode_disk_cachep = kmem_cache_create("nkfs_inode_disk_cache",
		sizeof(struct nkfs_inode_disk), 0, SLAB_MEM_SPREAD, NULL);
	if (!nkfs_inode_disk_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto del_inode_cache;
	}

	return 0;
del_inode_cache:
	kmem_cache_destroy(nkfs_inode_cachep);
	return err;
}

void nkfs_inode_finit(void)
{
	kmem_cache_destroy(nkfs_inode_disk_cachep);
	kmem_cache_destroy(nkfs_inode_cachep);
}
