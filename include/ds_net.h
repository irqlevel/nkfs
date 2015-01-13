#pragma once

#include <include/types.h>
#include <crt/include/sha256.h>

enum {
	DS_NET_PKT_ECHO = 1,
	DS_NET_PKT_OBJ_GET,
	DS_NET_PKT_OBJ_PUT,
	DS_NET_PKT_OBJ_DELETE
};

#define DS_NET_PKT_SIGN1	((u32)0xBEDABEDA)
#define DS_NET_PKT_SIGN2	((u32)0xCBADCBAD)

#pragma pack(push, 1)
struct ds_net_pkt {
	u32			sign1;
	u32			type;
	u32			dsize;
	int			err;
	union {
		struct {
			struct ds_obj_id	obj_id;
			u64			off;
		} obj_get;
		struct {
			struct ds_obj_id	obj_id;
			u64			off;
		} obj_put;
		struct {
			struct ds_obj_id	obj_id;
		} obj_delete;
	} u;
	struct sha256_sum	dsum;
	struct sha256_sum	sum;	
	u32			sign2;
};
#pragma pack(pop)
