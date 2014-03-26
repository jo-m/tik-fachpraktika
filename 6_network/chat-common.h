#ifndef _CHAT_COMMON_H_
#define _CHAT_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <ncurses.h>

#define LISTENQ   64
#define INIT_QSIZ (100*1024)

#ifndef min
#define min(a, b)              \
  ({                           \
    typeof (a) _a = (a);       \
    typeof (b) _b = (b);       \
    _a < _b ? _a : _b;         \
  })
#endif /* min */
#ifndef max
#define max(a, b)              \
  ({                           \
    typeof (a) _a = (a);       \
    typeof (b) _b = (b);       \
    _a > _b ? _a : _b;         \
  })
#endif

ssize_t write_exact(int fd, void *buf, size_t len);

void panic(const char *);

void whine(const char *);

void *xzmalloc(size_t);

int strnicmp(const char *, const char *, size_t);

size_t strlcat(char *, const char *, size_t);

#endif
