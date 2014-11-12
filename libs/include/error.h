#pragma once

#define DS_E_BASE	0xE000F000
#define DS_E_NO_MEM	DS_E_BASE + 1
#define DS_E_UNK_IOCTL	DS_E_BASE + 2

char *ds_error(int err);

