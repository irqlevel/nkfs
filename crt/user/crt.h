#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>

#define __LOGNAME__ "ds.log"

#define PAGE_SIZE 4096
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void crt_log_set_path(char *log_path, char *log_name);
void crt_log_set_level(int level);
