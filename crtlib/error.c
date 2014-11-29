#include <crtlib/include/crtlib.h>

char *ds_error(int err)
{
	switch (err) {
		case DS_E_NO_MEM:
			return "no memory";
		case DS_E_UNK_IOCTL:
			return "unknown ioctl";
		case DS_E_BUF_SMALL:
			return "buffer too small";
		case DS_E_PUT_FLD:
			return "send obj failed";
		case DS_E_CRT_FLD:
			return "create obj on server failed";
		case DS_E_DEL_FLD:
			return "delete obj on server failed";
		case DS_E_CONNECT_
		default:
			return "unknown err code";
	}
}
