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
