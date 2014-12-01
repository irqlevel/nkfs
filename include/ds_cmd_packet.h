#pragma once

#include <stdint.h>
#include <crtlib/include/crtlib.h>

struct ds_cmd {
	struct ds_obj_id  obj_id;
	uint64_t	  data_size;
	uint64_t 	  data_off;
	char		  *data;
	int		  cmd;   
	int		  error;
};
