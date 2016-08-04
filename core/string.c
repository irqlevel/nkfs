#include "string.h"

void nkfs_copy_string(char *dst, int dst_len, const char *src)
{
	int i;

	for (i = 0; i < dst_len; i++) {
		dst[i] = src[i];
		if (dst[i] == '\0')
			break;
	}
	dst[dst_len - 1] = '\0';
}
