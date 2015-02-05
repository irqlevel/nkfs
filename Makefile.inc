BIN=$(PROJ_ROOT)/bin
LIB=$(PROJ_ROOT)/lib
CC=gcc
AR=ar
COMMON_CFLAGS=-c -Wall -Werror -g3 -ggdb3 -I$(PROJ_ROOT) -D__ARCH_BITS__=$(ARCH_BITS)
