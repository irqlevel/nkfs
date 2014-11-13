#include <ds_priv.h>

void *crt_malloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

void *crt_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

void *crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}

void crt_free(void *ptr)
{
	kfree(ptr);
}

int crt_random_buf(void *buf, size_t len)
{
	return ds_random_buf_read(buf, len, 0);
}
