CC	= gcc
CFLAGS	= -Wall -O2

all: consumer producer tester

consumer.o: consumer.c
	$(CC) -c consumer.c

producer.o: producer.c
	$(CC) -c producer.c

tester.o: tester.c
	$(CC) -c tester.c

consumer: consumer.o
	$(CC) $(CFLAGS) -o consumer consumer.o

producer: producer.o
	$(CC) $(CFLAGS) -o producer producer.o

tester: tester.o
	$(CC) $(CFLAGS) -o tester tester.o

clean:
	rm -f tester producer consumer *.o

run: all
	(./producer &)
	./consumer
