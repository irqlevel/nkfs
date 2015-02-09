#include <crt/include/crt.h>

void csum_reset(struct csum_ctx *ctx)
{
	if (XXH64_reset(&ctx->state, 0))
		CRT_BUG();
}
EXPORT_SYMBOL(csum_reset);

void csum_update(struct csum_ctx *ctx, const void *input, size_t len)
{
	if (XXH64_update(&ctx->state, input, len))
		CRT_BUG();
}
EXPORT_SYMBOL(csum_update);

void csum_digest(struct csum_ctx *ctx, struct csum *sum)
{
	sum->val = XXH64_digest(&ctx->state);
}
EXPORT_SYMBOL(csum_digest);

u64 csum_u64(struct csum *sum)
{
	return sum->val;
}
EXPORT_SYMBOL(csum_u64);
