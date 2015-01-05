#pragma once

int ds_balloc_bm_clear(struct ds_sb *sb);
int ds_balloc_block_free(struct ds_sb *sb, u64 block);
int ds_balloc_block_alloc(struct ds_sb *sb, u64 *pblock);
int ds_balloc_block_mark(struct ds_sb *sb, u64 block, int use);
