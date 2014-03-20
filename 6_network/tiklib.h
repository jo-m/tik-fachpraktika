#ifndef _TIKLIB_H_
#define _TIKLIB_H_

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

#ifndef min
# define min(a, b)                         \
  ({                                 \
    typeof (a) _a = (a);       \
    typeof (b) _b = (b);       \
    _a < _b ? _a : _b;         \
  })
#endif /* min */
#ifndef max
# define max(a, b)                         \
  ({                                 \
    typeof (a) _a = (a);       \
    typeof (b) _b = (b);       \
    _a > _b ? _a : _b;         \
  })
#endif /* max */

static void panic(const char *serror)
{
  printf("%s", serror);
  exit(1);
}

static void whine(const char *serror)
{
  printf("%s", serror);
}

static void *xzmalloc(size_t size)
{
  void *ptr;
  if (size == 0)
    panic("Size is 0!\n");
  ptr = malloc(size);
  if (!ptr)
    panic("No mem left!\n");
  memset(ptr, 0, size);
  return ptr;
}

/* from: Linux kernel */
static int strnicmp(const char *s1, const char *s2, size_t len)
{
  unsigned char c1, c2;
  if (!len)
    return 0;
  do {
    c1 = *s1++;
    c2 = *s2++;
    if (!c1 || !c2)
      break;
    if (c1 == c2)
      continue;
    c1 = tolower(c1);
    c2 = tolower(c2);
    if (c1 != c2)
      break;
  } while (--len);
  return (int)c1 - (int)c2;
}

/* from: Linux kernel */
static size_t strlcat(char *dest, const char *src, size_t count)
{
  size_t dsize = strlen(dest);
  size_t len = strlen(src);
  size_t res = dsize + len;
  /* This would be a bug */
  if (dsize >= count)
    return 0;
  dest += dsize;
  count -= dsize;
  if (len >= count)
    len = count-1;
  memcpy(dest, src, len);
  dest[len] = 0;
  return res;
}

#endif