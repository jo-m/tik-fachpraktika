CC	= gcc
CFLAGS	= -Wall -O2

all: barbershop

barbershop.o: barbershop.c
	$(CC) -c barbershop.c

barbershop: barbershop.o
	$(CC) $(CFLAGS) -o barbershop barbershop.o -lpthread

clean:
	rm -f barbershop *.o

