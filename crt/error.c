#include <crt/include/crt.h>

char *nkfs_error(int err)
{
	switch (err) {
	case NKFS_E_NO_MEM:
		return "no memory";
	case NKFS_E_UNK_IOCTL:
		return "unknown ioctl";
	case NKFS_E_BUF_SMALL:
		return "buffer too small";
	case NKFS_E_OBJ_PUT:
		return "send obj failed";
	case NKFS_E_OBJ_CREATE:
		return "create obj on server failed";
	case NKFS_E_OBJ_DELETE:
		return "delete obj on server failed";
	case NKFS_E_CON_INIT_FAILED:
		return "connection initialization failed";
	case NKFS_E_OBJ_GET:
		return "failed to get object";
	case NKFS_E_INVAL:
		return "invalid value";
	case NKFS_E_LIMIT:
		return "limit reached";
	default:
		return "unknown err code";
	}
}
EXPORT_SYMBOL(nkfs_error);
