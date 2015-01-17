#pragma once

#include <include/ds_types.h>
#include <include/ds_obj_info.h>
#include <include/ds_const.h>
#include <crt/include/sha256.h>

enum {
	DS_NET_PKT_ECHO = 1,
	DS_NET_PKT_GET_OBJ,
	DS_NET_PKT_PUT_OBJ,
	DS_NET_PKT_DELETE_OBJ,
	DS_NET_PKT_QUERY_OBJ,
	DS_NET_PKT_CREATE_OBJ,
	DS_NET_PKT_HANDSHAKE
};

#define DS_NET_PKT_SIGN1	((u32)0xBEDABEDA)
#define DS_NET_PKT_SIGN2	((u32)0xCBADCBAD)

#pragma pack(push, 1)
struct ds_net_pkt {
	u32			sign1;
	u32			type;
	u32			dsize; /* data(after header) size */
	int			err;
	union {
		struct {
			struct ds_obj_id	obj_id;
			u64			off;
		} get_obj;
		struct {
			struct ds_obj_id	obj_id;
			u64			off;
		} put_obj;
		struct {
			struct ds_obj_id	obj_id;
		} delete_obj;
		struct {
			struct ds_obj_id	obj_id;
		} create_obj;
		struct {
			struct ds_obj_id	obj_id;
			struct ds_obj_info	obj_info;
		} query_obj;
	} u;
	struct sha256_sum	dsum;
	struct sha256_sum	sum;	
	u32			sign2;
};
#pragma pack(pop)
