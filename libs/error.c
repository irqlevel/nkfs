#include <error.h>

char *ds_error(int err)
{
	switch (err) {
		case DS_E_NO_MEM:
			return "no memory";
		case DS_E_UNK_IOCTL:
			return "unknown ioctl";
		default:
			return "unknown err code";
	}
}
