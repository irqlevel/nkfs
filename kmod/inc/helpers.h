#pragma once

static inline u64 ds_div_round_up(u64 x, u64 y)
{
	u64 tmp = x + y - 1;
	do_div(tmp, y);
	return tmp;
}

static inline u64 ds_div(u64 x, u64 y)
{
	u64 tmp = x;	
	do_div(tmp, y);
	return tmp;
}

static inline u32 ds_mod(u64 x, u64 y)
{
	u64 tmp = x;
	return do_div(tmp, y);
}

#define KLOG_BUF_SUM(buf, len)	do {				\
		struct sha256_sum bsum;				\
		char *hsum;					\
		char *be = NULL, *en = NULL;			\
		u32 llen = (len > 8) ? 8 : len;			\
		sha256(buf, len, &bsum, 0);			\
		hsum = sha256_sum_hex(&bsum);			\
		be = bytes_hex(buf, llen);			\
		en = bytes_hex((char *)buf + len - llen, llen);	\
		KLOG(KL_INF, "b %p len %u sum %s be %s en %s",	\
			buf, len, hsum, be, en);		\
		if (hsum)					\
			crt_free(hsum);				\
		if (be)						\
			crt_free(be);				\
		if (en)						\
			crt_free(en);				\
	} while (0);


