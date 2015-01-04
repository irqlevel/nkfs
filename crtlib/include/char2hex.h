#pragma once

extern asmlinkage char char_to_hex(char c);
extern asmlinkage int bytes_buf_hex(char *src, int src_count, char *hex,
		int hex_count);

extern asmlinkage char *bytes_hex(char *src, int src_count);

