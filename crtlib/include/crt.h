#pragma once

#define NULL 0
extern void *crt_malloc(unsigned int size);
extern void crt_free(void *ptr);
extern int crt_random_buf(void *buf, unsigned int len);
