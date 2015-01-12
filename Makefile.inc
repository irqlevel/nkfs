CC=gcc
AR=ar
COMMON_CFLAGS=-c -Wall -Werror -I$(PROJ_ROOT) -D__ARCH_BITS__=$(ARCH_BITS)
