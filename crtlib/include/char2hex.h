#pragma once

extern asmlinkage char char_to_hex(char c);
extern asmlinkage int char_buf_to_hex_buf(char *src, int src_count, char *hex,
		int hex_count);

extern asmlinkage char *char_buf_to_hex_str(char *src, int src_count);

