BIN=$(PROJ_ROOT)/bin
LIB=$(PROJ_ROOT)/lib
CC=gcc
AR=ar
COMMON_CFLAGS=-c -Wall -Werror -g3 -ggdb3 -I$(PROJ_ROOT) -D__ARCH_BITS__=$(ARCH_BITS)
ifndef NKFS_KERNEL_PATH
NKFS_KERNEL_PATH=/lib/modules/$(shell uname -r)/build
endif
