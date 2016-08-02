#ifndef __NKFS_CRT_NK8_H__
#define __NKFS_CRT_NK8_H__

extern int nk8_init(void);
extern void nk8_release(void);

extern int nk8_split_block(u8 *block, u32 block_size, int n, int k,
	u8 ***pparts, u8 **pids);

extern int nk8_assemble_block(u8 **parts, u8 *ids, int n, int k,
	u8 *block, u32 block_size);

#endif
