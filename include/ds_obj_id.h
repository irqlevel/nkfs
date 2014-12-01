#pragma once

#define DS_OBJ_ID_BYTES	16	/* 128 bits */

#pragma pack(push, 1)
struct ds_obj_id {
	char	__bytes[DS_OBJ_ID_BYTES];
};
#pragma pack(pop)
