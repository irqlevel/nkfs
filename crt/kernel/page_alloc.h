#pragma once

#include <linux/gfp.h>

struct page *crt_alloc_page(gfp_t flags);
void crt_free_page(struct page *page);

int crt_page_alloc_init(void);
void crt_page_alloc_deinit(void);
