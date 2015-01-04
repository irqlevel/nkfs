#include <inc/ds_priv.h>

asmlinkage void *crt_malloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

asmlinkage void * crt_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

asmlinkage void * crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}

asmlinkage void crt_free(void *ptr)
{
	kfree(ptr);
}

asmlinkage int crt_random_buf(void *buf, size_t len)
{
	return ds_random_buf_read(buf, len, 0);
}

asmlinkage void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	klog_v(level, "crt", file, line, func, fmt, args);  
	va_end(args);
}

asmlinkage size_t crt_strlen(const char *s)
{
	return strlen(s);
}
