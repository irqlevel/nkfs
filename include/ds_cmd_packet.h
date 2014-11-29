#pragma once

#include <stdint.h>
#include <crtlib/include/crtlib.h>

struct ds_cmd {
	uint64_t	 data_size;
	uint64_t 	 data_off;
	struct ds_obj_id obj_id;
	char	*data;
	int		 cmd;   
	int      error;
};
