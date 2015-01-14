#include <crt/include/crt.h>


static int inited = 0;
static u8 gf_log[0x100][0x100];
static u8 gf_alog[0x100][0x100];

#define gf_div(a, b)	gf_log[(a)][(b)]
#define gf_mult(a, b)	gf_alog[(a)][(b)]

static u8 gf_mult_direct(u8 m, u8 n)
{
	unsigned long k = 0;
	int i;

	if (m == 0 || n == 0)
		return 0;

	for (i = 7; i >= 0; i--)
		if (m >> i) {
			k ^= (n << i);
			m ^= (1 << i);
		}

  	if (k >> 8)
    		for (i = 15; i >= 8; i--)
      			if ( k >> i )
        			k ^= 283 << (i-8);

	return	k;
}

static u8 gf_div_brute(u8 i, u8 j) 
{	
	int found = 0;
	u32 k;

	if (i == 0 || j == 0)
		return 0;

	for (k = 1; k < 0x100; k++) {
    		if (gf_mult(k, j) == i) {
      			found = 1;
      			break;
    		}
		k++;
	}

	return (found) ? k : 0;
}

static void gf_init(void)
{
	u32 i, j;
	u32 count = 0;

	for (i = 0; i < 0x100; i++) {
		for (j = 0; j < 0x100; j++) {
			gf_log[i][j] = gf_mult_direct(i, j);
			count++;
			if ((count % 1000) == 0) {
				CLOG(CL_DBG, "count %u i %u j %u",
					count, i, j);
				crt_msleep(1);
			}
		}
	}

	count = 0;
	for (i = 0; i < 0x100; i++) {
		for (j = 0; j < 0x100; j++) {
			gf_alog[i][j] = gf_div_brute(i, j);
			count++;
			if ((count % 1000) == 0) {
				CLOG(CL_DBG, "count %u i %u j %u",
					count, i, j);
				crt_msleep(1);
			}
		}
	}
}

int nk8_init(void)
{
	gf_init();
	inited = 1;
	return 0;
}

void nk8_release(void)
{

}

