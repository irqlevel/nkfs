#include "super.h"
#include "inode.h"
#include "helpers.h"
#include "dio.h"
#include "dev.h"
#include "balloc.h"

#include <crt/include/crt.h>
#include <linux/fs.h>

static DECLARE_RWSEM(sb_list_lock);
static LIST_HEAD(sb_list);

static int nkfs_sb_sync(struct nkfs_sb *sb);

static void nkfs_sb_release(struct nkfs_sb *sb)
{
	KLOG(KL_DBG, "sb %p inodes tree %p", sb, sb->inodes_tree);
	if (sb->inodes_tree)
		nkfs_btree_deref(sb->inodes_tree);

	KLOG(KL_DBG, "sb %p released, inodes tree %p",
		sb, sb->inodes_tree);
}

static void nkfs_sb_delete(struct nkfs_sb *sb)
{
	nkfs_sb_release(sb);
	crt_kfree(sb);
}

void nkfs_sb_stop(struct nkfs_sb *sb)
{
	KLOG(KL_DBG, "sb %p dev %s stopping",
			sb, sb->dev->dev_name);

	sb->stopping = 1;
	down_write(&sb_list_lock);
	list_del_init(&sb->list);
	up_write(&sb_list_lock);

	if (sb->inodes_tree)
		nkfs_btree_stop(sb->inodes_tree);

	NKFS_BUG_ON(sb->inodes_active);
	nkfs_sb_sync(sb);
	KLOG(KL_INF, "sb %p used_blocks %llu",
		sb, atomic64_read(&sb->used_blocks));
}

void nkfs_sb_ref(struct nkfs_sb *sb)
{
	NKFS_BUG_ON(atomic_read(&sb->refs) <= 0);
	atomic_inc(&sb->refs);
}

void nkfs_sb_deref(struct nkfs_sb *sb)
{
	NKFS_BUG_ON(atomic_read(&sb->refs) <= 0);
	if (atomic_dec_and_test(&sb->refs))
		nkfs_sb_delete(sb);
}

struct nkfs_sb *nkfs_sb_lookup(struct nkfs_obj_id *id)
{
	struct nkfs_sb *sb, *found = NULL;

	down_read(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		if (0 == nkfs_obj_id_cmp(&sb->id, id)) {
			nkfs_sb_ref(sb);
			found = sb;
			break;
		}
	}
	up_read(&sb_list_lock);
	return found;
}

static u64 nkfs_sb_free_blocks(struct nkfs_sb *sb)
{
	NKFS_BUG_ON(sb->nr_blocks < atomic64_read(&sb->used_blocks));
	return sb->nr_blocks - atomic64_read(&sb->used_blocks);
}

static struct nkfs_sb *nkfs_sb_select_most_free(void)
{
	struct nkfs_sb *sb, *found = NULL;
	u64 max_free_blocks = 0;

	down_read(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		u32 free_blocks = nkfs_sb_free_blocks(sb);

		if (free_blocks >= max_free_blocks) {
			found = sb;
			max_free_blocks = free_blocks;
		}
	}

	if (found)
		nkfs_sb_ref(found);

	up_read(&sb_list_lock);
	return found;
}

static void nkfs_sb_list_release(struct list_head *phead)
{
	struct nkfs_sb_link *curr, *tmp;

	list_for_each_entry_safe(curr, tmp, phead, list) {
		list_del_init(&curr->list);
		nkfs_sb_deref(curr->sb);
		crt_kfree(curr);
	}
}

static u64 nkfs_sb_list_count(struct list_head *phead)
{
	struct nkfs_sb_link *curr;
	u64 count;

	count = 0;
	list_for_each_entry(curr, phead, list) {
		count++;
	}
	return count;
}

static struct nkfs_sb *nkfs_sb_list_first(struct list_head *phead)
{
	return (list_empty(phead)) ? NULL : list_first_entry(phead,
		struct nkfs_sb_link, list)->sb;
}

static struct nkfs_sb_link *nkfs_sb_link_create(struct nkfs_sb *sb)
{
	struct nkfs_sb_link *link;

	link = crt_kmalloc(sizeof(*link), GFP_NOIO);
	if (!link)
		return NULL;
	nkfs_sb_ref(sb);
	link->sb = sb;
	return link;
}

static int nkfs_sb_list_by_obj(struct nkfs_obj_id *obj_id,
	struct list_head *phead)
{
	u64 block;
	int err;
	struct nkfs_sb *sb;
	struct nkfs_sb_link *link;

	INIT_LIST_HEAD(phead);

	down_read(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		err = nkfs_btree_find_key(sb->inodes_tree,
				(struct nkfs_btree_key *)obj_id,
				(struct nkfs_btree_value *)&block);
		if (err)
			continue;
		link = nkfs_sb_link_create(sb);
		if (!link) {
			KLOG(KL_ERR, "no memory");
			err = -ENOMEM;
			goto unlock;
		}
		list_add_tail(&link->list, phead);
	}
	err = 0;

unlock:
	up_read(&sb_list_lock);
	if (err) {
		nkfs_sb_list_release(phead);
		INIT_LIST_HEAD(phead);
	}

	return err;
}

int nkfs_sb_insert(struct nkfs_sb *cand)
{
	struct nkfs_sb *sb;
	int err = 0;

	down_write(&sb_list_lock);
	list_for_each_entry(sb, &sb_list, list) {
		if (0 == nkfs_obj_id_cmp(&sb->id, &cand->id)) {
			err = -EEXIST;
			break;
		}
	}
	if (!err)
		list_add_tail(&cand->list, &sb_list);
	up_write(&sb_list_lock);
	return err;
}

static int nkfs_sb_gen_header(struct nkfs_sb *sb,
	u64 size,
	u32 bsize)
{
	int err;
	u64 bm_blocks;

	if (nkfs_mod(size, bsize) || (size <= bsize)) {
		KLOG(KL_ERR, "size %lld bsize %u", size, bsize);
		return -EINVAL;
	}

	bm_blocks = nkfs_div_round_up(
				nkfs_div_round_up(nkfs_div(size, bsize), 8),
				bsize);
	if (size <= (NKFS_IMAGE_BM_BLOCK + bm_blocks)*bsize) {
		KLOG(KL_ERR, "size %lld to small bsize %u bm_blocks %u",
			size, bsize, bm_blocks);
		return -EINVAL;
	}

	err = nkfs_obj_id_gen(&sb->id);
	if (err) {
		KLOG(KL_ERR, "cant gen obj id err %d", err);
		return err;
	}


	sb->magic = NKFS_IMAGE_MAGIC;
	sb->version = NKFS_IMAGE_VER_1;
	sb->size = size;
	sb->bsize = bsize;

	sb->bm_block = NKFS_IMAGE_BM_BLOCK;
	sb->bm_blocks = bm_blocks;

	return 0;
}

static void nkfs_image_header_sum(struct nkfs_image_header *header,
	struct csum *sum)
{
	struct csum_ctx ctx;

	csum_reset(&ctx);
	csum_update(&ctx, header, offsetof(struct nkfs_image_header, sum));
	csum_digest(&ctx, sum);
}

static int nkfs_sb_parse_header(struct nkfs_sb *sb,
	struct nkfs_image_header *header)
{
	struct csum sum;

	if (be32_to_cpu(header->sig) != NKFS_IMAGE_SIG) {
		KLOG(KL_ERR, "invalid header sig");
		return -EINVAL;
	}

	nkfs_image_header_sum(header, &sum);
	if (0 != memcmp(&header->sum, &sum, sizeof(sum))) {
		KLOG(KL_ERR, "invalid header sum");
		return -EINVAL;
	}

	sb->magic = be32_to_cpu(header->magic);
	sb->version = be32_to_cpu(header->version);
	sb->size = be64_to_cpu(header->size);
	sb->bsize = be32_to_cpu(header->bsize);
	sb->bm_block = be64_to_cpu(header->bm_block);
	sb->bm_blocks = be64_to_cpu(header->bm_blocks);
	sb->inodes_tree_block = be64_to_cpu(header->inodes_tree_block);
	atomic64_set(&sb->used_blocks, be64_to_cpu(header->used_blocks));

	memcpy(&sb->id, &header->id, sizeof(header->id));

	return 0;
}

static void nkfs_sb_fill_header(struct nkfs_sb *sb,
	struct nkfs_image_header *header)
{
	memset(header, 0, sizeof(*header));
	header->magic = cpu_to_be32(sb->magic);
	header->version = cpu_to_be32(sb->version);
	header->used_blocks = cpu_to_be64(atomic64_read(&sb->used_blocks));
	header->size = cpu_to_be64(sb->size);
	header->bsize = cpu_to_be32(sb->bsize);
	header->bm_block = cpu_to_be64(sb->bm_block);
	header->bm_blocks = cpu_to_be64(sb->bm_blocks);
	header->inodes_tree_block = cpu_to_be64(sb->inodes_tree_block);
	header->sig = cpu_to_be32(NKFS_IMAGE_SIG);
	memcpy(&header->id, &sb->id, sizeof(sb->id));
	nkfs_image_header_sum(header, &header->sum);
}

static int nkfs_sb_sync(struct nkfs_sb *sb)
{
	struct dio_cluster *clu;
	struct nkfs_image_header header;
	int err;

	clu = dio_clu_get(sb->ddev, 0);
	if (!clu) {
		KLOG(KL_ERR, "cant get 0block");
		return -EIO;
	}

	nkfs_sb_fill_header(sb, &header);

	err = dio_clu_write(clu, &header, sizeof(header), 0);
	if (err) {
		KLOG(KL_ERR, "cant write header err %d", err);
		goto out;
	}

	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "cant sync 0block err %d", err);
		goto out;
	}

out:
	dio_clu_put(clu);
	return err;
}

static int nkfs_sb_check(struct nkfs_sb *sb)
{
	int err;

	if (sb->magic != NKFS_IMAGE_MAGIC) {
		KLOG(KL_ERR, "sb %p invalid magic %x", sb,
			sb->magic);
		err = -EINVAL;
		goto out;
	}

	if (sb->version != NKFS_IMAGE_VER_1) {
		KLOG(KL_ERR, "sb %p invalid version %x", sb,
				sb->version);
		err = -EINVAL;
		goto out;
	}

	if ((sb->size == 0) || (sb->bsize == 0) ||
		(sb->size <= sb->bsize) ||
		(nkfs_mod(sb->size, sb->bsize))) {
		KLOG(KL_ERR, "sb %p invalid size %llu bsize %u",
			sb, sb->size, sb->bsize);
		err = -EINVAL;
		goto out;
	}

	if (sb->bm_block != NKFS_IMAGE_BM_BLOCK) {
		KLOG(KL_ERR, "sb %p invalid bm_block %u",
			sb, sb->bm_block);
		err = -EINVAL;
		goto out;
	}

	if (sb->size <= (sb->bm_blocks + NKFS_IMAGE_BM_BLOCK)) {
		KLOG(KL_ERR, "sb %p invalid size %llu bm_blocks %llu",
			sb, sb->size, sb->bm_blocks);
		err = -EINVAL;
		goto out;
	}

	err = 0;
out:
	return err;
}


static int nkfs_sb_create(struct nkfs_dev *dev,
		struct nkfs_image_header *header,
		struct nkfs_sb **psb)
{
	int err;
	struct nkfs_sb *sb;

	sb = crt_kmalloc(sizeof(*sb), GFP_NOIO);
	if (!sb) {
		KLOG(KL_ERR, "cant alloc sb");
		return -ENOMEM;
	}
	memset(sb, 0, sizeof(*sb));
	init_rwsem(&sb->rw_lock);
	INIT_LIST_HEAD(&sb->list);
	atomic_set(&sb->refs, 1);
	sb->inodes = RB_ROOT;
	rwlock_init(&sb->inodes_lock);

	if (!header) {
		err = nkfs_sb_gen_header(sb, i_size_read(dev->bdev->bd_inode),
			dev->bsize);
		if (err) {
			KLOG(KL_ERR, "can gen header");
			goto free_sb;
		}
	} else {
		err = nkfs_sb_parse_header(sb, header);
		if (err) {
			KLOG(KL_ERR, "cant parse header");
			goto free_sb;
		}
	}

	err = nkfs_sb_check(sb);
	if (err) {
		KLOG(KL_ERR, "invalid sb");
		goto free_sb;
	}

	sb->nr_blocks = nkfs_div(sb->size, sb->bsize);
	sb->dev = dev;
	sb->bdev = dev->bdev;
	sb->ddev = dev->ddev;

	*psb = sb;
	return 0;

free_sb:
	crt_kfree(sb);
	return err;
}

void nkfs_sb_log(struct nkfs_sb *sb)
{
	KLOG(KL_DBG, "sb %p blocks %llu ind tree %llu bm %llu bm_blocks %llu",
		sb, sb->nr_blocks, sb->inodes_tree_block, sb->bm_block,
		sb->bm_blocks);
}

int nkfs_sb_format(struct nkfs_dev *dev, struct nkfs_sb **psb)
{
	struct dio_cluster *clu;
	int err;
	struct nkfs_sb *sb = NULL;
	struct nkfs_image_header header;
	u64 i;

	clu = dio_clu_get(dev->ddev, 0);
	if (!clu) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;
	}

	err = nkfs_sb_create(dev, NULL, &sb);
	if (err) {
		KLOG(KL_ERR, "cant create sb");
		goto free_clu;
	}

	err = nkfs_balloc_bm_clear(sb);
	if (err) {
		KLOG(KL_ERR, "cant clear balloc bm err %d", err);
		goto del_sb;
	}

	for (i = 0; i < sb->bm_block + sb->bm_blocks; i++) {
		err = nkfs_balloc_block_mark(sb, i, 1);
		if (err) {
			KLOG(KL_ERR, "cant mark block %llu as used err %d",
				i, err);
			goto del_sb;
		}
	}

	sb->inodes_tree = nkfs_btree_create(sb, 0);
	if (!sb->inodes_tree) {
		KLOG(KL_ERR, "cant create obj btree");
		goto del_sb;
	}

	sb->inodes_tree_block = nkfs_btree_root_block(sb->inodes_tree);

	dio_clu_zero(clu);
	nkfs_sb_fill_header(sb, &header);

	err = dio_clu_write(clu, &header, sizeof(header), 0);
	if (err) {
		KLOG(KL_ERR, "write err %d", err);
		goto del_sb;
	}

	err = dio_clu_sync(clu);
	if (err) {
		KLOG(KL_ERR, "sync err %d", err);
		goto del_sb;
	}

	nkfs_sb_log(sb);
	*psb = sb;
	err = 0;
	goto free_clu;

del_sb:
	nkfs_sb_delete(sb);
free_clu:
	dio_clu_put(clu);
out:
	return err;
}

int nkfs_sb_load(struct nkfs_dev *dev, struct nkfs_sb **psb)
{
	struct dio_cluster *clu;
	struct nkfs_sb *sb = NULL;
	struct nkfs_image_header header;
	int err;

	clu = dio_clu_get(dev->ddev, 0);
	if (!clu) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;
	}

	err = dio_clu_read(clu, &header, sizeof(header), 0);
	if (err) {
		KLOG(KL_ERR, "read err %d", err);
		goto free_clu;
	}

	err = nkfs_sb_create(dev, &header,
		&sb);
	if (err) {
		KLOG(KL_ERR, "cant create sb");
		goto free_clu;
	}

	if (sb->size > i_size_read(dev->bdev->bd_inode)) {
		KLOG(KL_ERR, "dev %p invalid size %llu sb %p", dev,
				sb->size, sb);
		err = -EINVAL;
		goto free_sb;
	}

	if (sb->inodes_tree_block >= sb->nr_blocks) {
		KLOG(KL_ERR, "sb %p obj tree %llu nr_blocks %llud",
			sb, sb->inodes_tree_block, sb->nr_blocks);
		err = -EINVAL;
		goto free_sb;
	}
	sb->inodes_tree = nkfs_btree_create(sb, sb->inodes_tree_block);
	if (!sb->inodes_tree) {
		KLOG(KL_ERR, "sb %p cant load obj tree %llu",
			sb, sb->inodes_tree_block);
		err = -EINVAL;
		goto free_sb;
	}

	nkfs_sb_log(sb);
	*psb = sb;
	err = 0;
	goto free_clu;

free_sb:
	nkfs_sb_delete(sb);
free_clu:
	dio_clu_put(clu);
out:
	return err;
}

int nkfs_sb_init(void)
{
	return 0;
}

void nkfs_sb_finit(void)
{
}

static int nkfs_sb_get_obj(struct nkfs_sb *sb,
	struct nkfs_obj_id *id, u64 off, u32 pg_off, u32 len,
	struct page **pages,
	int nr_pages,
	u32 *pread)
{
	struct nkfs_inode *inode;
	u64 iblock;
	int err;

	if (sb->stopping)
		return -EAGAIN;

	err = nkfs_btree_find_key(sb->inodes_tree, (struct nkfs_btree_key *)id,
			(struct nkfs_btree_value *)&iblock);
	if (err) {
		KLOG(KL_ERR, "obj not found");
		return err;
	}

	inode = nkfs_inode_read(sb, iblock);
	if (!inode) {
		KLOG(KL_ERR, "cant read inode %llu", iblock);
		return -EIO;
	}
	NKFS_BUG_ON(inode->block != iblock);

	if (nkfs_obj_id_cmp(&inode->ino, id) != 0) {
		KLOG(KL_ERR, "inode %llu has another id", iblock);
		err = -ENOENT;
		goto cleanup;
	}

	err = nkfs_inode_io_pages(inode, off, pg_off, len,
		pages, nr_pages, 0, pread);
	if (err) {
		KLOG(KL_ERR, "read inode %llu at %llu pages %u len %u err %d",
			    iblock, off, nr_pages, len, err);
		goto cleanup;
	}

cleanup:
	INODE_DEREF(inode);
	return err;
}

static int nkfs_sb_create_obj(struct nkfs_sb *sb,
	struct nkfs_obj_id *pobj_id)
{
	struct nkfs_obj_id obj_id;
	struct nkfs_inode *inode;
	int err;

	if (sb->stopping)
		return -EAGAIN;

	nkfs_obj_id_gen(&obj_id);
	inode = nkfs_inode_create(sb, &obj_id);
	if (!inode) {
		KLOG(KL_ERR, "no memory");
		return -ENOMEM;
	}

	err = nkfs_btree_insert_key(sb->inodes_tree,
			(struct nkfs_btree_key *)&inode->ino,
			(struct nkfs_btree_value *)&inode->block, 0);
	if (err) {
		KLOG(KL_ERR, "cant insert ino in inodes_tree err %d",
				err);
		nkfs_inode_delete(inode);
		goto out;
	}

	nkfs_obj_id_copy(pobj_id, &obj_id);
	err = 0;
out:
	INODE_DEREF(inode);
	return err;
}

static int nkfs_sb_put_obj(struct nkfs_sb *sb,
	struct nkfs_obj_id *obj_id, u64 off, u32 pg_off, u32 len,
	struct page **pages, int nr_pages)
{
	struct nkfs_inode *inode;
	u64 iblock;
	int err;
	u32 io_count;

	if (sb->stopping)
		return -EAGAIN;

	err = nkfs_btree_find_key(sb->inodes_tree,
				(struct nkfs_btree_key *)obj_id,
				(struct nkfs_btree_value *)&iblock);
	if (err)
		return err;

	inode = nkfs_inode_read(sb, iblock);
	if (!inode) {
		KLOG(KL_ERR, "cant read inode at %llu off %llu",
			iblock, off);
		return -EIO;
	}
	NKFS_BUG_ON(inode->block != iblock);

	err = nkfs_inode_io_pages(inode, off, pg_off, len,
		pages, nr_pages, 1, &io_count);
	if (err) {
		KLOG(KL_ERR, "block %llu off %llu pages %u len %u err %d",
			inode->block, off, nr_pages, len, err);
	}

	INODE_DEREF(inode);
	return err;
}

static int nkfs_sb_delete_obj(struct nkfs_sb *sb, struct nkfs_obj_id *obj_id)
{
	int err;
	u64 iblock;
	struct nkfs_inode *inode;

	NKFS_BUG_ON(!sb->inodes_tree);
	NKFS_BUG_ON(sb->inodes_tree->sig1 != NKFS_BTREE_SIG1);

	if (sb->stopping)
		return -EAGAIN;

	err = nkfs_btree_find_key(sb->inodes_tree,
				  (struct nkfs_btree_key *)obj_id,
				  (struct nkfs_btree_value *)&iblock);
	if (err)
		return err;

	inode = nkfs_inode_read(sb, iblock);
	if (!inode) {
		KLOG(KL_ERR, "cant read inode %llu", iblock);
		return -EIO;
	}

	nkfs_btree_delete_key(sb->inodes_tree,
			      (struct nkfs_btree_key *)&inode->ino);
	nkfs_inode_delete(inode);
	INODE_DEREF(inode);
	return err;
}

int nkfs_sb_list_get_obj(struct nkfs_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages, u32 *pread)
{
	struct list_head list;
	int err;
	struct nkfs_sb *sb;

	err = nkfs_sb_list_by_obj(obj_id, &list);
	if (err)
		return err;

	NKFS_BUG_ON(nkfs_sb_list_count(&list) > 1);
	sb = nkfs_sb_list_first(&list);
	if (!sb) {
		err = -ENOENT;
		goto cleanup;
	}

	err = nkfs_sb_get_obj(sb, obj_id, off, pg_off, len,
		pages, nr_pages, pread);

cleanup:
	nkfs_sb_list_release(&list);
	return err;
}

int nkfs_sb_list_put_obj(struct nkfs_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages)
{
	struct list_head list;
	int err;
	struct nkfs_sb *sb;

	err = nkfs_sb_list_by_obj(obj_id, &list);
	if (err)
		return err;

	NKFS_BUG_ON(nkfs_sb_list_count(&list) > 1);
	sb = nkfs_sb_list_first(&list);
	if (!sb) {
		err = -ENOENT;
		goto cleanup;
	}

	err = nkfs_sb_put_obj(sb, obj_id, off, pg_off, len, pages, nr_pages);

cleanup:
	nkfs_sb_list_release(&list);
	return err;
}

int nkfs_sb_list_delete_obj(struct nkfs_obj_id *obj_id)
{
	struct list_head list;
	int err;
	struct nkfs_sb *sb;

	err = nkfs_sb_list_by_obj(obj_id, &list);
	if (err)
		return err;

	NKFS_BUG_ON(nkfs_sb_list_count(&list) > 1);
	sb = nkfs_sb_list_first(&list);
	if (!sb) {
		err = -ENOENT;
		goto cleanup;
	}

	err = nkfs_sb_delete_obj(sb, obj_id);

cleanup:
	nkfs_sb_list_release(&list);
	return err;

}

int nkfs_sb_list_create_obj(struct nkfs_obj_id *pobj_id)
{
	struct nkfs_sb *sb;
	int err;

	sb = nkfs_sb_select_most_free();
	if (!sb)
		return -ENOENT;

	err = nkfs_sb_create_obj(sb, pobj_id);
	nkfs_sb_deref(sb);
	return err;
}

static int nkfs_sb_query_obj(struct nkfs_sb *sb, struct nkfs_obj_id *obj_id,
	struct nkfs_obj_info *info)
{
	int err;
	u64 iblock;
	struct nkfs_inode *inode;

	NKFS_BUG_ON(!sb->inodes_tree);
	NKFS_BUG_ON(sb->inodes_tree->sig1 != NKFS_BTREE_SIG1);

	if (sb->stopping)
		return -EAGAIN;

	err = nkfs_btree_find_key(sb->inodes_tree,
			(struct nkfs_btree_key *)obj_id,
			(struct nkfs_btree_value *)&iblock);
	if (err)
		return err;

	inode = nkfs_inode_read(sb, iblock);
	if (!inode) {
		KLOG(KL_ERR, "cant read inode %llu", iblock);
		return -EIO;
	}
	KLOG(KL_INF, "in query");
	nkfs_obj_id_copy(&info->sb_id, &sb->id);
	nkfs_obj_id_copy(&info->obj_id, &inode->ino);
	info->block = inode->block;
	info->bsize = sb->bsize;
	snprintf(info->dev_name, sizeof(info->dev_name),
		"%s", sb->dev->dev_name);
	info->size = inode->size;
	err = 0;

	INODE_DEREF(inode);
	return err;
}

int nkfs_sb_list_query_obj(struct nkfs_obj_id *obj_id,
			   struct nkfs_obj_info *info)
{
	struct list_head list;
	int err;
	struct nkfs_sb *sb;

	err = nkfs_sb_list_by_obj(obj_id, &list);
	if (err)
		return err;

	NKFS_BUG_ON(nkfs_sb_list_count(&list) > 1);
	sb = nkfs_sb_list_first(&list);
	if (!sb) {
		err = -ENOENT;
		goto cleanup;
	}

	err = nkfs_sb_query_obj(sb, obj_id, info);

cleanup:
	nkfs_sb_list_release(&list);
	return err;
}

