#pragma once

#include <include/types.h>
#include <include/ds_obj_id.h>
#include <include/ds_net.h>
#include <include/ds_ctl.h>

void *crt_memset(void *ptr, int value, size_t num);
void *crt_memcpy(void * destination, const void * source, size_t num);
int crt_memcmp(const void *ptr1, const void * ptr2, size_t num);
void *crt_malloc(size_t size);
void crt_free(void *ptr);
int crt_random_buf(void *buf, size_t len);
size_t crt_strlen(const char * str);
void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...);

#include <crt/include/obj_id.h>
#include <crt/include/char2hex.h>
#include <crt/include/error.h>
#include <crt/include/sha256.h>
#include <crt/include/clog.h>
#include <crt/include/net_cmd.h>
#include <crt/include/random.h>

#ifdef __KERNEL__
#include <crt/kernel/crt.h>
#else
#include <crt/user/crt.h>
#define EXPORT_SYMBOL(s)
#endif
