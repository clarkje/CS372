CC=gcc
CFLAGS=-I.

all: ftserver

# http://bit.ly/2lDEmlf
debug: CFLAGS += -g
debug: ftserver

ftserver: ftserver.o
	$(CC) -o ftserver ftserver.o -I.

clean:
	rm *.o
