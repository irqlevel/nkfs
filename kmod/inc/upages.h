#pragma once

struct ds_user_pages {
	unsigned long uaddr;
	struct page **pages;
	u32 nr_pages;
	int write;
};

struct ds_pages {
	struct page 	**pages;
	u32 		nr_pages;
	u32		len;
};

int ds_get_user_pages(unsigned long uaddr, u32 nr_pages,
	int write, struct ds_user_pages *pup);
void ds_release_user_pages(struct ds_user_pages *up);

void ds_pages_region(unsigned long buf, u32 len,
	unsigned long *ppg_addr, u32 *ppg_off, u32 *pnr_pages);

int ds_pages_create(u32 len, struct ds_pages *ppages);
void ds_pages_dsum(struct ds_pages *pages, struct sha256_sum *dsum);
void ds_pages_release(struct ds_pages *pages);
