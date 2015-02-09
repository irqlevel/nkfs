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

