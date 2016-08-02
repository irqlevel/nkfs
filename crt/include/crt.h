#ifndef __NKFS_CRT_H__
#define __NKFS_CRT_H__

#include <include/nkfs_types.h>
#include <include/nkfs_obj_id.h>
#include <include/nkfs_network.h>
#include <include/nkfs_control.h>

void *crt_memset(void *ptr, int value, size_t num);
void *crt_memcpy(void * destination, const void * source, size_t num);
int crt_memcmp(const void *ptr1, const void * ptr2, size_t num);
void *crt_malloc(size_t size);
void crt_free(void *ptr);
int crt_random_buf(void *buf, size_t len);
size_t crt_strlen(const char * str);
void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...);

void crt_msleep(u32 ms);


void *crt_file_open(char *path);
int crt_file_read(void *file, const void *buf, u32 len, loff_t *off);
int crt_file_write(void *file, const void *buf, u32 len, loff_t *off);
int crt_file_sync(void *file);
void crt_file_close(void *file);

#include <crt/include/obj_id.h>
#include <crt/include/char2hex.h>
#include <crt/include/error.h>
#include <crt/include/sha256.h>
#include <crt/include/clog.h>
#include <crt/include/net_pkt.h>
#include <crt/include/random.h>
#include <crt/include/nk8.h>
#include <crt/include/xxhash.h>
#include <crt/include/csum.h>

#ifdef __KERNEL__
#include <crt/kernel/crt.h>
#else
#include <crt/user/crt.h>
#endif

#define CLOG_BUF_SUM(buf, len)	do {				\
		struct sha256_sum bsum;				\
		char *hsum;					\
		char *be = NULL, *en = NULL;			\
		u32 llen = (len > 8) ? 8 : len;			\
		sha256(buf, len, &bsum, 0);			\
		hsum = sha256_sum_hex(&bsum);			\
		be = bytes_hex(buf, llen);			\
		en = bytes_hex((char *)buf + len - llen, llen);	\
		CLOG(CL_INF, "b %p len %u sum %s be %s en %s",	\
			buf, len, hsum, be, en);		\
		if (hsum)					\
			crt_free(hsum);				\
		if (be)						\
			crt_free(be);				\
		if (en)						\
			crt_free(en);				\
	} while (0);

#endif
