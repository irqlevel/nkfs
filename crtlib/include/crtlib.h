#pragma once

#include <include/ds_obj_id.h>

#ifndef NULL
#define NULL 0
#endif

extern void *crt_malloc(unsigned int size);
extern void crt_free(void *ptr);
extern int crt_random_buf(void *buf, unsigned int len);


#define DS_E_BASE	0xE000F000
#define DS_E_NO_MEM	DS_E_BASE + 1
#define DS_E_UNK_IOCTL	DS_E_BASE + 2
#define DS_E_BUF_SMALL	DS_E_BASE + 3

extern char *ds_error(int err);

extern char *ds_obj_id_to_str(struct ds_obj_id *id);
extern struct ds_obj_id *ds_obj_id_gen(void);

extern char char_to_hex(char c);
extern int char_to_hex_buf(char *src, int src_count, char *hex, int hex_count);
