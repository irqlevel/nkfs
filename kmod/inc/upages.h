#pragma once

struct ds_user_pages {
	unsigned long uaddr;
	struct page **pages;
	int nr_pages;
	int write;
};

int ds_get_user_pages(unsigned long uaddr, u32 nr_pages,
	int write, struct ds_user_pages *pup);
void ds_release_user_pages(struct ds_user_pages *up);

void ds_pages_region(unsigned long buf, u32 len,
	unsigned long *ppg_addr, u32 *ppg_off, u32 *pnr_pages);
