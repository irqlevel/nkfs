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

void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	klog_v(level,  __LOGNAME__, "crt", file, line, func, fmt, args);  
	va_end(args);
}

size_t crt_strlen(const char *s)
{
	return strlen(s);
}
