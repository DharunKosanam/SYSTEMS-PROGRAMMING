
CC = gcc


CFLAGS = -Wall -Wextra -g


virtmem: virtmem.o
	$(CC) $(CFLAGS) -o virtmem virtmem.o


virtmem.o: virtmem.c
	$(CC) $(CFLAGS) -c virtmem.c


clean:
	rm -f virtmem virtmem.o
