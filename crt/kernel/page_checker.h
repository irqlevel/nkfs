#ifndef __CRT_KERNEL_PAGE_CHECKER_H__
#define __CRT_KERNEL_PAGE_CHECKER_H__

#include <linux/gfp.h>

int page_checker_init(void);
void page_checker_deinit(void);

struct page *page_checker_alloc_page(gfp_t flags);
void page_checker_free_page(struct page *page);

#endif
