BIN=$(PROJECT_ROOT)/bin
LIB=$(PROJECT_ROOT)/lib
CC=gcc
AR=ar

PROJECT_CFLAGS=-static -Wall -Werror -I$(PROJECT_ROOT)	\
		-D__ARCH_BITS__=$(ARCH_BITS)

ifndef KERNEL_BUILD_PATH
KERNEL_BUILD_PATH=/lib/modules/$(shell uname -r)/build
endif
