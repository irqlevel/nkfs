#pragma once

extern char char_to_hex(char c);
extern int char_buf_to_hex_buf(char *src, int src_count, char *hex,
		int hex_count);

extern char *char_buf_to_hex_str(char *src, int src_count);

