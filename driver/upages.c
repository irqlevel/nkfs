#include <inc/ds_priv.h>

int ds_get_user_pages(unsigned long uaddr, u32 len,
	int write, struct ds_user_pages *pup)
{
	struct ds_user_pages up;
	int ret, err;

	if (uaddr & (PAGE_SIZE -1))
		return -EINVAL;
	if (len == 0 || (len & (PAGE_SIZE - 1)))
		return -EINVAL;

	memset(&up, 0, sizeof(up));
	up.nr_pages = len/PAGE_SIZE;
	up.pages = kmalloc(up.nr_pages*sizeof(struct page *), GFP_NOFS);
	if (!up.pages)
		return -ENOMEM;
	up.write = !!write;
	ret = get_user_pages_fast(uaddr, up.nr_pages, up.write, up.pages);
	if (ret != up.nr_pages) {
		err = -EINVAL;
		goto fail;
	}
	memcpy(pup, &up, sizeof(up));
	return 0;	
fail:
	ds_release_user_pages(&up);
	return err;
}

void ds_release_user_pages(struct ds_user_pages *up)
{
	int i;
	if (!up->write) {
		for (i = 0; i < up->nr_pages; i++) {
			SetPageDirty(up->pages[i]);
		}
	}

	for (i = 0; i < up->nr_pages; i++) {
		if (up->pages[i])
			page_cache_release(up->pages[i]);
	}

	kfree(up->pages);
}
