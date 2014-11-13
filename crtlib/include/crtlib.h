#pragma once

typedef __SIZE_TYPE__ size_t;
typedef unsigned int uint32_t;

#ifndef NULL
#define NULL 0
#endif

extern void *crt_memset(void *ptr, int value, size_t num);
extern void *crt_memcpy(void * destination, const void * source, size_t num);

extern void *crt_malloc(size_t size);
extern void crt_free(void *ptr);
extern int crt_random_buf(void *buf, size_t len);

#include <crtlib/include/obj_id.h>
#include <crtlib/include/char2hex.h>
#include <crtlib/include/error.h>
#include <crtlib/include/sha256.h>

