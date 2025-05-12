# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2

TARGET = mts

mts: mts.c
	$(CC) $(CFLAGS) -o $(TARGET) mts.c


clean:
	rm -f $(TARGET) *.o
