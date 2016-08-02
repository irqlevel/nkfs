#ifndef __CRT_KERNEL_MALLOC_H__
#define __CRT_KERNEL_MALLOC_H__

#include <linux/gfp.h>

void *crt_kmalloc(size_t size, gfp_t flags);

void *crt_kcalloc(size_t n, size_t size, gfp_t flags);

void crt_kfree(void *ptr);

void *crt_malloc(size_t size);

void *crt_memset(void *ptr, int value, size_t len);

void *crt_memcpy(void *ptr1, const void *ptr2, size_t len);

int crt_memcmp(const void *ptr1, const void *ptr2, size_t len);

void crt_free(void *ptr);

int crt_kmalloc_init(void);

void crt_kmalloc_deinit(void);

#endif
