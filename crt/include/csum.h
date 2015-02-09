#pragma once

#include <crt/include/xxhash.h>

struct csum {
	u64 val;
};

struct csum_ctx {
	XXH64_state_t state;
};

void csum_reset(struct csum_ctx *ctx);
void csum_update(struct csum_ctx *ctx, const void *input, size_t len);
void csum_digest(struct csum_ctx *ctx, struct csum *sum);
u64 csum_u64(struct csum *sum);
