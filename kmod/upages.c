#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "upages"

int ds_get_user_pages(unsigned long uaddr, u32 nr_pages,
	int write, struct ds_user_pages *pup)
{
	struct ds_user_pages up;
	int ret, err;

	if (uaddr & (PAGE_SIZE -1))
		return -EINVAL;
	if (nr_pages == 0)
		return -EINVAL;

	memset(&up, 0, sizeof(up));
	up.nr_pages = nr_pages;
	up.pages = kmalloc(up.nr_pages*sizeof(struct page *), GFP_NOIO);
	if (!up.pages)
		return -ENOMEM;

	memset(up.pages, 0, up.nr_pages*sizeof(struct page *));

	up.write = !!write;

	down_read(&current->mm->mmap_sem);
	ret = get_user_pages(current, current->mm, uaddr, up.nr_pages,
		(up.write) ? WRITE : READ, 0, up.pages, NULL);
	up_read(&current->mm->mmap_sem);

	if (ret != up.nr_pages) {
		if (ret >= 0)
			up.nr_pages = ret;
		else
			up.nr_pages = 0;
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

	if (up->write) {
		for (i = 0; i < up->nr_pages; i++) {
			BUG_ON(!up->pages[i]);
			set_page_dirty_lock(up->pages[i]);	
		}
	}

	for (i = 0; i < up->nr_pages; i++) {
		BUG_ON(!up->pages[i]);
		put_page(up->pages[i]);
	}

	kfree(up->pages);
}

void ds_pages_region(unsigned long buf, u32 len,
	unsigned long *ppg_addr, u32 *ppg_off, u32 *pnr_pages)
{
	u32 pg_off;
	unsigned long pg_addr;
	u64 buf_page, end_page, pages_delta;

	pg_off = buf & (PAGE_SIZE - 1);
	pg_addr = buf & ~(PAGE_SIZE -1);

	buf_page = ds_div(buf, PAGE_SIZE);
	end_page = ds_div(buf + len, PAGE_SIZE);

	pages_delta = end_page - buf_page;
	pages_delta += ((buf + len) & (PAGE_SIZE - 1)) ? 1 : 0;
	KLOG(KL_DBG, "pg_addr %lx len %u nr_pages %llu pg_off %u",
		pg_addr, len, pages_delta, pg_off);
	*ppg_addr = pg_addr;
	*ppg_off = pg_off;
	*pnr_pages = (u32)pages_delta;
}
