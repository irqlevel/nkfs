#include "inc/nkfs_priv.h"

int nkfs_get_user_pages(unsigned long uaddr, u32 nr_pages,
	int write, struct nkfs_user_pages *pup)
{
	struct nkfs_user_pages up;
	int ret, err;

	if (uaddr & (PAGE_SIZE - 1))
		return -EINVAL;
	if (nr_pages == 0)
		return -EINVAL;

	memset(&up, 0, sizeof(up));
	up.nr_pages = nr_pages;
	up.pages = crt_kcalloc(up.nr_pages, sizeof(struct page *), GFP_NOIO);
	if (!up.pages)
		return -ENOMEM;

	up.write = !!write;
	down_read(&current->mm->mmap_sem);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 3)
	ret = get_user_pages(uaddr, up.nr_pages,
		(up.write) ? WRITE : READ, 0, up.pages, NULL);
#else
	ret = get_user_pages(current, current->mm, uaddr, up.nr_pages,
		(up.write) ? WRITE : READ, 0, up.pages, NULL);
#endif
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
	nkfs_release_user_pages(&up);
	return err;
}

void nkfs_release_user_pages(struct nkfs_user_pages *up)
{
	int i;

	if (up->write) {
		for (i = 0; i < up->nr_pages; i++) {
			NKFS_BUG_ON(!up->pages[i]);
			set_page_dirty_lock(up->pages[i]);
		}
	}

	for (i = 0; i < up->nr_pages; i++) {
		NKFS_BUG_ON(!up->pages[i]);
		crt_free_page(up->pages[i]);
	}

	crt_kfree(up->pages);
}

void nkfs_pages_region(unsigned long buf, u32 len,
	unsigned long *ppg_addr, u32 *ppg_off, u32 *pnr_pages)
{
	u32 pg_off;
	unsigned long pg_addr;
	u64 buf_page, end_page, pages_delta;

	pg_off = buf & (PAGE_SIZE - 1);
	pg_addr = buf & ~(PAGE_SIZE - 1);

	buf_page = buf >> PAGE_SHIFT;
	end_page = (buf + len) >> PAGE_SHIFT;

	pages_delta = end_page - buf_page;
	pages_delta += ((buf + len) & (PAGE_SIZE - 1)) ? 1 : 0;
	KLOG(KL_DBG1, "pg_addr %lx len %u nr_pages %llu pg_off %u",
		pg_addr, len, pages_delta, pg_off);
	*ppg_addr = pg_addr;
	*ppg_off = pg_off;
	*pnr_pages = (u32)pages_delta;
}

int nkfs_pages_create(u32 len, struct nkfs_pages *ppages)
{
	struct nkfs_pages pages;
	u32 i, j;

	if (len == 0)
		return -EINVAL;

	memset(&pages, 0, sizeof(pages));
	pages.nr_pages = (len >> PAGE_SHIFT);
	pages.nr_pages += (len & (PAGE_SIZE - 1)) ? 1 : 0;

	KLOG(KL_DBG1, "nr_pages %u len %u psize %lu",
		pages.nr_pages, len, PAGE_SIZE);

	pages.len = len;
	pages.pages = crt_kcalloc(pages.nr_pages, sizeof(struct page *),
				  GFP_NOIO);
	if (!pages.pages)
		return -ENOMEM;

	for (i = 0; i < pages.nr_pages; i++) {
		pages.pages[i] = crt_alloc_page(GFP_NOIO);
		if (!pages.pages[i])
			goto fail;
	}

	memcpy(ppages, &pages, sizeof(pages));
	return 0;
fail:
	for (j = 0; j < i; j++)
		crt_free_page(pages.pages[j]);
	crt_kfree(pages.pages);
	return -ENOMEM;
}

int nkfs_pages_dsum(struct nkfs_pages *pages, struct csum *dsum, u32 len)
{
	struct csum_ctx ctx;
	u32 i, ilen;
	void *ibuf;

	if (pages->len < len)
		return -EINVAL;

	csum_reset(&ctx);

	i = 0;
	while (len > 0) {
		NKFS_BUG_ON(i >= pages->nr_pages);
		ilen = (len > PAGE_SIZE) ? PAGE_SIZE : len;
		ibuf = kmap(pages->pages[i]);
		csum_update(&ctx, ibuf, ilen);
		kunmap(pages->pages[i]);
		len -= ilen;
		i++;
	}

	csum_digest(&ctx, dsum);
	return 0;
}

void nkfs_pages_release(struct nkfs_pages *pages)
{
	u32 i;

	for (i = 0; i < pages->nr_pages; i++)
		crt_free_page(pages->pages[i]);
	crt_kfree(pages->pages);
}
