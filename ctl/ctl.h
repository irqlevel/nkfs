#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <include/ds_ctl.h>
#include <crt/include/crt.h>

int ds_dev_add(const char *dev_name, int format);
int ds_dev_rem(const char *dev_name);

int ds_dev_query(const char *dev_name,
			struct ds_obj_id *psb_id);

int ds_obj_read(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len);

int ds_obj_write(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len);

int ds_obj_delete(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id);

int ds_obj_tree_check(struct ds_obj_id *sb_id);

int ds_server_stop(u32 ip, int port);
int ds_server_start(u32 ip, int port);
