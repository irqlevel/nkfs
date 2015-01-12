#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "balloc"

int ds_balloc_bm_clear(struct ds_sb *sb)
{
	struct buffer_head *bh;
	u64 i;
	int err;

	down_write(&sb->rw_lock);
	sb->used_blocks = 0;
	for (i = sb->bm_block; i < sb->bm_block + sb->bm_blocks; i++) {
		bh = __bread(sb->bdev, i, sb->bsize);
		if (!bh) {
			KLOG(KL_ERR, "cant read block %llu", i);
			err = -EIO;
			goto out;
		}
		memset(bh->b_data, 0, sb->bsize);
		mark_buffer_dirty(bh);
		err = sync_dirty_buffer(bh);
		if (err) {
			KLOG(KL_ERR, "sync 0block err %d", err);
			brelse(bh);
			goto out;
		}
		brelse(bh);
	}

	err = 0;
out:
	up_write(&sb->rw_lock);

	return err;
}

static int ds_balloc_block_bm_bit(struct ds_sb *sb, u64 block,
	u64 *pblock, u32 *pbyte_off, u32 *pbit)
{
	u64 byte_off;

	if (block >= sb->nr_blocks) {
		KLOG(KL_ERR, "block %llu out of sb blocks %llu",
			block, sb->nr_blocks);
		return -EINVAL;
	}

	byte_off = ds_div(block, 8);

	*pbit = ds_mod(block, 8);
	*pblock = sb->bm_block + ds_div(byte_off, sb->bsize);
	*pbyte_off = ds_mod(byte_off, sb->bsize);

	return 0;
}

int ds_balloc_block_mark(struct ds_sb *sb, u64 block, int use)
{
	int err;
	u32 byte_off, bit;
	u64 bm_block;
	struct buffer_head *bh;
	
	err = ds_balloc_block_bm_bit(sb, block, &bm_block, &byte_off, &bit);
	if (err)
		return err;

	down_write(&sb->rw_lock);
	bh = __bread(sb->bdev, bm_block, sb->bsize);	
	if (!bh) {
		KLOG(KL_ERR, "cant read bm block %llu", bm_block);
		err = -EIO;
		goto out;
	}

	if (use) {
		BUG_ON(test_bit_le(bit, bh->b_data + byte_off));
		set_bit_le(bit, bh->b_data + byte_off);
		sb->used_blocks++;
	} else {
		BUG_ON(!test_bit_le(bit, bh->b_data + byte_off));
		clear_bit_le(bit, bh->b_data + byte_off);
		sb->used_blocks--;
	}

	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "cant sync block %llu", bm_block);
		goto cleanup;		
	}

	err = 0;

cleanup:
	brelse(bh);
out:
	up_write(&sb->rw_lock);
	return err;
}

int ds_balloc_block_free(struct ds_sb *sb, u64 block)
{
	return ds_balloc_block_mark(sb, block, 0);
}

static int ds_balloc_block_find_set_free_bit(struct ds_sb *sb,
	struct buffer_head *bh, u32 *pbyte, u32 *pbit)
{
	u32 pos, bit;
	int err;

	down_write(&sb->rw_lock);
	for (pos = 0; pos < sb->bsize; pos++) {
		for (bit = 0; bit < 8; bit++) {
			if (!test_bit_le(bit, bh->b_data + pos)) {
				set_bit_le(bit, bh->b_data + pos);
				sb->used_blocks++;
				mark_buffer_dirty(bh);
				err = sync_dirty_buffer(bh);
				if (err) {
					KLOG(KL_ERR, "cant sync bh err %d",
						err);
				} else {
					*pbyte = pos;
					*pbit = bit;
				}
				goto out;
			}
		}
	}
	err = -ENOENT;
out:
	up_write(&sb->rw_lock);
	return err;
}

int ds_balloc_block_alloc(struct ds_sb *sb, u64 *pblock)
{
	struct buffer_head *bh;
	u64 i;
	int err;
	u32 byte, bit;

	for (i = sb->bm_block; i < sb->bm_block + sb->bm_blocks; i++) {
		bh = __bread(sb->bdev, i, sb->bsize);
		if (!bh) {
			KLOG(KL_ERR, "cant read block %llu", i);
			err = -EIO;
			goto out;
		}

		err = ds_balloc_block_find_set_free_bit(sb, bh,
			&byte, &bit);
		if (!err) {
			u64 block = ((i - sb->bm_block)*sb->bsize + byte)*8
				+ bit;
			KLOG(KL_DBG, "alloc byte %u bit %u i %llu block %llu",
				byte, bit , i, block);
			*pblock = block;
			brelse(bh);
			return 0;
		}			
		brelse(bh);
	}

	err = -ENOSPC;
out:
	return err;
}
