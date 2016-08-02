#ifndef __NKFS_UPAGES_H__
#define __NKFS_UPAGES_H__

#include <linux/module.h>
#include <crt/include/csum.h>

struct nkfs_user_pages {
	unsigned long uaddr;
	struct page **pages;
	u32 nr_pages;
	int write;
};

struct nkfs_pages {
	struct page	**pages;
	u32		nr_pages;
	u32		len;
};

int nkfs_get_user_pages(unsigned long uaddr, u32 nr_pages,
	int write, struct nkfs_user_pages *pup);
void nkfs_release_user_pages(struct nkfs_user_pages *up);

void nkfs_pages_region(unsigned long buf, u32 len,
	unsigned long *ppg_addr, u32 *ppg_off, u32 *pnr_pages);

int nkfs_pages_create(u32 len, struct nkfs_pages *ppages);
int nkfs_pages_dsum(struct nkfs_pages *pages, struct csum *dsum, u32 len);
void nkfs_pages_release(struct nkfs_pages *pages);

#endif
