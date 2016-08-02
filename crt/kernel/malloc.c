#include "malloc.h"
#include "malloc_checker.h"

void *crt_malloc(size_t size)
{
	return crt_kmalloc(size, GFP_NOIO);
}
EXPORT_SYMBOL(crt_malloc);

void *crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}
EXPORT_SYMBOL(crt_memset);

void *crt_memcpy(void *ptr1, const void *ptr2, size_t len)
{
	return memcpy(ptr1, ptr2, len);
}
EXPORT_SYMBOL(crt_memcpy);

int crt_memcmp(const void *ptr1, const void *ptr2, size_t len)
{
	return memcmp(ptr1, ptr2, len);
}
EXPORT_SYMBOL(crt_memcmp);

void crt_free(void *ptr)
{
	crt_kfree(ptr);
}
EXPORT_SYMBOL(crt_free);

void *crt_kmalloc(size_t size, gfp_t flags)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_kmalloc(size, flags);
#else
	return kmalloc(size, flags);
#endif
}
EXPORT_SYMBOL(crt_kmalloc);

void *crt_kcalloc(size_t n, size_t size, gfp_t flags)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_kmalloc(n * size, flags);
#else
	return kmalloc(n * size, flags);
#endif
}
EXPORT_SYMBOL(crt_kcalloc);

void crt_kfree(void *ptr)
{
#ifdef __MALLOC_CHECKER__
	malloc_checker_kfree(ptr);
#else
	kfree(ptr);
#endif
}
EXPORT_SYMBOL(crt_kfree);

int crt_kmalloc_init(void)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_init();
#else
	return 0;
#endif
}

void crt_kmalloc_deinit(void)
{
#ifdef __MALLOC_CHECKER__
	malloc_checker_deinit();
#endif
}
