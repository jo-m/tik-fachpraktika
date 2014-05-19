CC	= gcc
CFLAGS	= -Wall -O2

all: client server

client.o: client.c chat-common.h
	$(CC) $(CFLAGS) -c client.c

server.o: server.c chat-common.h
	$(CC) $(CFLAGS) -c server.c

chat-common.o: chat-common.c chat-common.h
	$(CC) $(CFLAGS) -c chat-common.c

client: client.o chat-common.o
	$(CC) $(CFLAGS) -o client client.o chat-common.o -lncurses

server: server.o chat-common.o
	$(CC) $(CFLAGS) -static -o server server.o chat-common.o

sv: all
	./server 1337

cl: all
	./client localhost 1337

clean:
	rm -f client server *.o
