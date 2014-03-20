CC	= gcc
CFLAGS	= -Wall -O2

DB_FILE	= test.db
MOUNTPOINT	= mnt

all: tikfs tikdb

tikfs: tikfs.c tikfs.h
	$(CC) $(CFLAGS) tikfs.c `pkg-config fuse --cflags --libs` -o tikfs

tikdb: tikdb.c tikfs.h
	$(CC) $(CFLAGS) -o tikdb tikdb.c

createdb: tikdb
	if [ -f $(DB_FILE) ]; then rm $(DB_FILE); fi
	./tikdb create $(DB_FILE)
	./tikdb add $(DB_FILE) '25.02.2014' 'Watch new Dr Who episodes 1'
	./tikdb add $(DB_FILE) '26.02.2014' 'Watch old Dr Who episodes 2'
	./tikdb add $(DB_FILE) '27.02.2014' 'Watch blah 3'
	./tikdb add $(DB_FILE) '28.02.2014' 'Watch blubb 4'

mount: tikdb tikfs
	mkdir -p $(MOUNTPOINT)
	fusermount -uq $(MOUNTPOINT) || true
	./tikfs $(MOUNTPOINT) $(DB_FILE)

test: createdb mount
	@ls -l $(MOUNTPOINT)/25.02.2014
	@cat $(MOUNTPOINT)/25.02.2014
	echo blah > $(MOUNTPOINT)/25.02.2014
	@ls -l $(MOUNTPOINT)/25.02.2014
	@cat $(MOUNTPOINT)/25.02.2014
	fusermount -uq $(MOUNTPOINT) || true
	./tikfs $(MOUNTPOINT) $(DB_FILE)
	@ls -l $(MOUNTPOINT)/25.02.2014
	@cat $(MOUNTPOINT)/25.02.2014
