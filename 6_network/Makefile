CC	= gcc
CFLAGS	= -Wall -O2

all: client server

client.o: client.c chat-common.h
	$(CC) -c client.c

server.o: server.c chat-common.h
	$(CC) -c server.c

chat-common.o: chat-common.c chat-common.h
	$(CC) -c chat-common.c

client: client.o chat-common.o
	$(CC) $(CFLAGS) -o client client.o chat-common.o -lncurses

server: server.o chat-common.o
	$(CC) $(CFLAGS) -o server server.o chat-common.o -lncurses

sv: server
	./server 1337

clean:
	rm -f client server *.o
