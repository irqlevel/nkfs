#pragma once

#define DS_OBJ_ID_BYTES	16	/* 128 bits */

struct ds_obj_id {
	char	bytes[DS_OBJ_ID_BYTES];
};

