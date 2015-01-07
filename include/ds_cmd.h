#pragma once
#include <linux/ioctl.h>
#include <include/ds_obj_id.h>
#include <include/types.h>

#define DS_IOC_MAGIC 0xE1 
#define DS_NAME_MAX_SZ 256

#pragma pack(push, 1)

struct ds_cmd {
	int err;
	union {
		struct {
			char dev_name[DS_NAME_MAX_SZ];
			int format;
		} dev_add;
		struct {
			char dev_name[DS_NAME_MAX_SZ];
		} dev_remove;
		struct {
			char dev_name[DS_NAME_MAX_SZ];
			struct ds_obj_id sb_id;
		} dev_query;
		struct  {
			int port;
		} server_start;
		struct {
			int port;
		} server_stop;
		struct {
			struct ds_obj_id	sb_id;
			struct ds_obj_id	obj_id;
			u64			value;
			int			replace;			
		} obj_insert;
		struct {
			struct ds_obj_id	sb_id;
			struct ds_obj_id	obj_id;
		} obj_delete;
		struct {
			struct ds_obj_id	sb_id;
			struct ds_obj_id	obj_id;
			u64			value;
		} obj_find;
	} u;
};

#pragma pack(pop)

#define IOCTL_DS_DEV_ADD	_IOWR(DS_IOC_MAGIC, 1, struct ds_cmd *)
#define IOCTL_DS_DEV_REMOVE	_IOWR(DS_IOC_MAGIC, 2, struct ds_cmd *)
#define IOCTL_DS_DEV_QUERY	_IOWR(DS_IOC_MAGIC, 3, struct ds_cmd *)

#define IOCTL_DS_SRV_START	_IOWR(DS_IOC_MAGIC, 4, struct ds_cmd *)
#define IOCTL_DS_SRV_STOP	_IOWR(DS_IOC_MAGIC, 5, struct ds_cmd *)

#define IOCTL_DS_OBJ_INSERT	_IOWR(DS_IOC_MAGIC, 6, struct ds_cmd *)
#define IOCTL_DS_OBJ_FIND	_IOWR(DS_IOC_MAGIC, 7, struct ds_cmd *)
#define IOCTL_DS_OBJ_DELETE	_IOWR(DS_IOC_MAGIC, 8, struct ds_cmd *)
