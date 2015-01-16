#include <crt/include/crt.h>

static char char_to_hex(char c)
{
	if (c < 10)
		return '0' + c;
	else
		return 'a' + c - 10;
}

static int hex_to_char(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

int hex_bytes(char *hex, int hex_len, char *dst, int dst_len)
{
	int i, pos;
	int low, high;

	if (hex_len <= 0 || dst_len <= 0) {
		CLOG(CL_ERR, "inv len");
		return -EINVAL;
	}

	if (hex_len & 1) {
		CLOG(CL_ERR, "inv hex len");
		return -EINVAL;
	}

	if (dst_len != hex_len/2) {
		CLOG(CL_ERR, "inv dst len");
		return -EINVAL;
	}

	for (i = 0, pos = 0; i < hex_len; i+=2, pos+=1) {
		high = hex_to_char(hex[i]);
		low = hex_to_char(hex[i+1]);
		if (high == -1 || low == -1) {
			CLOG(CL_ERR, "cant conv hex to char");
			return -EINVAL;
		}
		dst[pos] = (high << 4) + low;
	}

	return 0;
}
EXPORT_SYMBOL(hex_bytes);

static int bytes_buf_hex(char *src, int src_count, char *hex, int hex_count)
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

char *bytes_hex(char *src, int src_count)
{
	char *hex;
	int hex_count;

	if (!src || src_count <= 0)
		return NULL;

	hex_count = 2*src_count + 1;
	hex = crt_malloc(hex_count);
	if (!hex)
		return NULL;

	if (bytes_buf_hex(src, src_count, hex, hex_count)) {
		crt_free(hex);
		return NULL;
	}

	hex[2*src_count] = '\0';
	return hex;
}
EXPORT_SYMBOL(bytes_hex);
