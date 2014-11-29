#pragma once

#define DS_E_BASE	    	 0xE000F000
#define DS_E_NO_MEM	    	 DS_E_BASE + 1
#define DS_E_UNK_IOCTL		 DS_E_BASE + 2
#define DS_E_BUF_SMALL		 DS_E_BASE + 3
#define DS_E_OBJ_PUT    	 DS_E_BASE + 4
#define DS_E_OBJ_CREATE      DS_E_BASE + 5
#define DS_E_OBJ_DELETE      DS_E_BASE + 6
#define DS_E_CON_INIT_FAILED DS_E_BASE + 7
#define DS_E_OBJ_GET		 DS_E_BASE + 8

extern char *ds_error(int err);


