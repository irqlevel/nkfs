#pragma once

#define NKFS_E_BASE		0xE000F000
#define NKFS_E_NO_MEM		NKFS_E_BASE + 1
#define NKFS_E_UNK_IOCTL		NKFS_E_BASE + 2
#define NKFS_E_BUF_SMALL		NKFS_E_BASE + 3
#define NKFS_E_OBJ_PUT		NKFS_E_BASE + 4
#define NKFS_E_OBJ_CREATE		NKFS_E_BASE + 5
#define NKFS_E_OBJ_DELETE		NKFS_E_BASE + 6
#define NKFS_E_CON_INIT_FAILED	NKFS_E_BASE + 7
#define NKFS_E_OBJ_GET		NKFS_E_BASE + 8
#define NKFS_E_INVAL		NKFS_E_BASE + 9

char *nkfs_error(int err);

