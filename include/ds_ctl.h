#pragma once
#include <linux/ioctl.h>
#include <include/ds_obj_id.h>
#include <include/ds_dev_info.h>
#include <include/ds_types.h>
#include <include/ds_const.h>

#define DS_IOC_MAGIC 0xE1 
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
			struct ds_dev_info info;
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
			u32 d_ip;
			int d_port;
			u32 s_ip;
			int s_port;
		} neigh_add;
		struct {
			u32 d_ip;
			int d_port;
		} neigh_remove;
	} u;
};

#pragma pack(pop)

#define IOCTL_DS_DEV_ADD	_IOWR(DS_IOC_MAGIC, 1, struct ds_cmd *)
#define IOCTL_DS_DEV_REMOVE	_IOWR(DS_IOC_MAGIC, 2, struct ds_cmd *)
#define IOCTL_DS_DEV_QUERY	_IOWR(DS_IOC_MAGIC, 3, struct ds_cmd *)

#define IOCTL_DS_SRV_START	_IOWR(DS_IOC_MAGIC, 4, struct ds_cmd *)
#define IOCTL_DS_SRV_STOP	_IOWR(DS_IOC_MAGIC, 5, struct ds_cmd *)

#define IOCTL_DS_NEIGH_ADD	_IOWR(DS_IOC_MAGIC, 6, struct ds_cmd *)
#define IOCTL_DS_NEIGH_REMOVE	_IOWR(DS_IOC_MAGIC, 7, struct ds_cmd *)

