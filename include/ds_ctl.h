#pragma once
#include <linux/ioctl.h>
#include <include/ds_obj_id.h>
#include <include/types.h>

#define DS_IOC_MAGIC 0xE1 
#define DS_NAME_MAX_SZ 256

#pragma pack(push, 1)

struct ds_ctl {
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
			u32 ip;
			int port;
		} server_start;
		struct {
			u32 ip;
			int port;
		} server_stop;
		struct {
			struct ds_obj_id	obj_id;	
			void			*buf;
			u64			off;
			u32			len;
		} put_obj;
		struct {
			struct ds_obj_id	obj_id;
			void			*buf;
			u64			off;
			u32			len;
		} get_obj;
		struct {
			struct ds_obj_id	obj_id;
		} create_obj;
		struct {
			struct ds_obj_id	obj_id;
		} delete_obj;
	} u;
};

#pragma pack(pop)

#define IOCTL_DS_DEV_ADD	_IOWR(DS_IOC_MAGIC, 1, struct ds_cmd *)
#define IOCTL_DS_DEV_REMOVE	_IOWR(DS_IOC_MAGIC, 2, struct ds_cmd *)
#define IOCTL_DS_DEV_QUERY	_IOWR(DS_IOC_MAGIC, 3, struct ds_cmd *)

#define IOCTL_DS_SRV_START	_IOWR(DS_IOC_MAGIC, 4, struct ds_cmd *)
#define IOCTL_DS_SRV_STOP	_IOWR(DS_IOC_MAGIC, 5, struct ds_cmd *)

#define IOCTL_DS_OBJ_TREE_CHECK	_IOWR(DS_IOC_MAGIC, 6, struct ds_cmd *)

#define IOCTL_DS_PUT_OBJ 	_IOWR(DS_IOC_MAGIC, 7, struct ds_cmd *)
#define IOCTL_DS_GET_OBJ	_IOWR(DS_IOC_MAGIC, 8, struct ds_cmd *)
#define IOCTL_DS_DELETE_OBJ	_IOWR(DS_IOC_MAGIC, 9, struct ds_cmd *)
#define IOCTL_DS_CREATE_OBJ	_IOWR(DS_IOC_MAGIC, 10, struct ds_cmd *)

