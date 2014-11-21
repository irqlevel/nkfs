#pragma once

int ds_random_init(void);
void ds_random_release(void);
int ds_random_buf_read(void *buf, __u32 len, int urandom);


