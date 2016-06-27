#include "page_alloc.h"
#include "page_checker.h"

int crt_page_alloc_init(void)
{
#ifdef __PAGE_CHECKER__
	return page_checker_init();
#else
	return 0;
#endif
}

void crt_page_alloc_deinit(void)
{
#ifdef __PAGE_CHECKER__
	page_checker_deinit();
#endif
}

struct page *crt_alloc_page(gfp_t flags)
{
#ifdef __PAGE_CHECKER__
	return page_checker_alloc_page(flags);
#else
	return alloc_page(flags);
#endif
}
EXPORT_SYMBOL(crt_alloc_page);

void crt_free_page(struct page *page)
{
#ifdef __PAGE_CHECKER__
	page_checker_free_page(page);
#else
	put_page(page);
#endif
}
EXPORT_SYMBOL(crt_free_page);
