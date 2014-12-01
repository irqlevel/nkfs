#include <crtlib/include/crtlib.h>

asmlinkage char *ds_error(int err)
{
	switch (err) {
		case DS_E_NO_MEM:
			return "no memory";
		case DS_E_UNK_IOCTL:
			return "unknown ioctl";
		case DS_E_BUF_SMALL:
			return "buffer too small";
		default:
			return "unknown err code";
	}
}
