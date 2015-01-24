#pragma once

#include <include/ds_types.h>

#pragma pack(push, 1)
struct ds_obj_id {
	union {
		char	__bytes[16];
		struct {
			u64 high;
			u64 low;
		};
	};
};
#pragma pack(pop)

_Static_assert(sizeof(struct ds_obj_id) == 16, "size incorrect");
