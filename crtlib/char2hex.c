#include <crtlib/include/crtlib.h>

char char_to_hex(char c)
{
	if (c < 10)
		return '0' + c;
	else
		return 'a' + c - 10;
}

int char_buf_to_hex_buf(char *src, int src_count, char *hex, int hex_count)
{
	int i;

	if (hex_count < 2*src_count)
		return -DS_E_BUF_SMALL;

	for (i = 0; i < src_count; i++) {
		*hex++ = char_to_hex((*src >> 4) & 0xF);
		*hex++ = char_to_hex(*src++ & 0xF);
	}
	return 0;
}

char *char_buf_to_hex_str(char *src, int src_count)
{
	char *hex;
	int hex_count;

	if (src_count == 0)
		src_count = crt_strlen(src);

	hex_count = 2*src_count + 1;
	hex = crt_malloc(hex_count);
	if (!hex)
		return NULL;

	if (char_buf_to_hex_buf(src, src_count, hex, hex_count)) {
		crt_free(hex);
		return NULL;
	}

	hex[2*src_count] = '\0';
	return hex;
}	
