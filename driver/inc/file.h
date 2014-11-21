#pragma once

int file_write(struct file *file, const void *buf, u32 len, loff_t *off);
int file_sync(struct file *file);
int file_read(struct file *file, const void *buf, u32 len, loff_t *off);

