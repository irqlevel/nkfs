#include <ds_priv.h>

void *crt_malloc(unsigned int size)
{
	return kmalloc(size, GFP_KERNEL);
}

void crt_free(void *ptr)
{
	kfree(ptr);
}

int crt_random_buf(void *buf, unsigned int len)
{
	return ds_random_buf_read(buf, len, 0);
}
