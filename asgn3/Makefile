CC = clang
CFLAGS=-Wall -Wextra -Werror -pedantic

all: queue.o rwlock.o 

rwlock: rwlock.c
	$(CC) $(CFLAGS) -o rwlock rwlock.c

queue: queue.c
	$(CC) $(CFLAGS) -o queue queue.c

clean:
	rm -f *.o *.out