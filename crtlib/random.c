#include <crtlib/include/crtlib.h>

asmlinkage u32 get_random_u32(void)
{
	u32 cand;
	if (crt_random_buf(&cand, sizeof(cand)))
		return -1;
	return cand;
}

asmlinkage u64 get_random_u64(void)
{
	u64 cand;
	if (crt_random_buf(&cand, sizeof(cand)))
		return -1;
	return cand;
}

