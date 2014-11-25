#pragma once

#define DS_E_BASE	0xE000F000
#define DS_E_NO_MEM	DS_E_BASE + 1
#define DS_E_UNK_IOCTL	DS_E_BASE + 2
#define DS_E_BUF_SMALL	DS_E_BASE + 3
#define DS_E_PUT_FLD DS_E_BASE + 4
#define DS_E_CRT_FLD DS_E_BASE + 5

extern char *ds_error(int err);


