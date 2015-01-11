#pragma once

struct ds_user_pages {
	unsigned long uaddr;
	struct page **pages;
	int nr_pages;
	int write;
};

int ds_get_user_pages(unsigned long uaddr, u32 len,
	int write, struct ds_user_pages *pup);
void ds_release_user_pages(struct ds_user_pages *up);
