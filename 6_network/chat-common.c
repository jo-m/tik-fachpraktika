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
#include "chat-common.h"

void panic(const char *serror)
{
  printf("%s", serror);
  exit(1);
}

void whine(const char *serror)
{
  printf("%s", serror);
}

void *xzmalloc(size_t size)
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
int strnicmp(const char *s1, const char *s2, size_t len)
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
size_t strlcat(char *dest, const char *src, size_t count)
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

ssize_t write_exact(int fd, void *buf, size_t len)
{
  register ssize_t num = 0, written;
  while (len > 0) {
    if ((written = write(fd, buf, len)) < 0)
      continue;
    if (!written)
      return 0;
    len -= written;
    buf += written;
    num += written;
  }
  return num;
}
