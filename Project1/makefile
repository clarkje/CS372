CC=gcc
CFLAGS=-I.

all: chatclient

# http://bit.ly/2lDEmlf
debug: CFLAGS += -g
debug: chatclient

chatclient: chatclient.o
	$(CC) -o chatclient chatclient.o -I.

clean:
	rm *.o
