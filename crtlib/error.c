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
	case DS_E_OBJ_PUT:
		return "send obj failed";
	case DS_E_OBJ_CREATE:
		return "create obj on server failed";
	case DS_E_OBJ_DELETE:
		return "delete obj on server failed";
	case DS_E_CON_INIT_FAILED:
		return "connection initialization failed";
	case DS_E_OBJ_GET:
		return "failed to get object";
	case DS_E_INVAL:
		return "invalid value";
	default:
		return "unknown err code";
	}
}
