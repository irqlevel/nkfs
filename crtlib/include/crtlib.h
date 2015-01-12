#pragma once

#include <include/types.h>
#include <include/ds_obj_id.h>
#include <include/ds_net_cmd.h>

void *crt_memset(void *ptr, int value, size_t num);
void *crt_memcpy(void * destination, const void * source, size_t num);
int crt_memcmp(const void *ptr1, const void * ptr2, size_t num);
void *crt_malloc(size_t size);
void crt_free(void *ptr);
int crt_random_buf(void *buf, size_t len);
size_t crt_strlen(const char * str);
void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...);

#include <crtlib/include/obj_id.h>
#include <crtlib/include/char2hex.h>
#include <crtlib/include/error.h>
#include <crtlib/include/sha256.h>
#include <crtlib/include/clog.h>
#include <crtlib/include/net_cmd.h>
#include <crtlib/include/random.h>

#ifdef __KERNEL__
#include <crtlib/kernel/crt.h>
#else
#include <crtlib/user/crt.h>
#define EXPORT_SYMBOL(s)
#endif
