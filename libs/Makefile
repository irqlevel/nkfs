CC=gcc
CFLAGS=-c -Wall -Werror -Iinclude

all: error.o
	cp error.o ../objs/error.o

error.o: error.c
	$(CC) $(CFLAGS) error.c

clean:
	rm -rf *.o
