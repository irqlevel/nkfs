#pragma once

#include <include/nkfs_types.h>

#pragma pack(push, 1)
struct nkfs_obj_id {
	union {
		char	__bytes[16];
		struct {
			u64 high;
			u64 low;
		};
	};
};
#pragma pack(pop)

_Static_assert(sizeof(struct nkfs_obj_id) == 16, "size incorrect");
