CC = clang
CFLAGS=-Wall -Wextra -Werror -pedantic

all: httpserver 

httpserver: httpserver.c
	$(CC) $(CFLAGS) -o httpserver httpserver.c asgn2_helper_funcs.a

clean:
	rm -f httpserver *.o