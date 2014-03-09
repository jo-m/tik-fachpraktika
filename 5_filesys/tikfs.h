#ifndef _TIKFS_H_
#define _TIKFS_H_

#include <sys/sendfile.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syslog.h>

#define TIKDB_MAGIC   0xbeefee
#define TIKDB_VERSION_MAJ 1
#define TIKDB_VERSION_MIN 0

struct tikdb_filehdr {
  uint32_t magic;
  uint16_t version_major;
  uint16_t version_minor;
};

struct tikdb_datahdr {
  char key[32];
  time_t ts_r;
  time_t ts_w;
  uint32_t len;
};

enum {
  TIK_NONE,
  TIK_ROOT,
  TIK_FILE,
};

struct tikfs_fnode {
  struct tikdb_datahdr meta;
  off_t data;
  int dirty;
  uint8_t *cowbuff;
  size_t cowlen;
};

#ifndef min
# define min(a, b)                         \
        ({                                 \
                typeof (a) _a = (a);       \
                typeof (b) _b = (b);       \
                _a < _b ? _a : _b;         \
        })
#endif

static inline void die(void)
{
  exit(1);
}

static void panic(const char *serror)
{
  printf("%s", serror);
  die();
}

static void *xmalloc(size_t len)
{
  void *ptr = malloc(len);
  if (!ptr) {
    syslog(LOG_ERR, "no mem left! panic!\n");
    exit(1);
  }
  return ptr;
}

static void *xrealloc(void *ptr, size_t nlen)
{
  void *nptr = realloc(ptr, nlen);
  if (!nptr) {
    syslog(LOG_ERR, "no mem left! panic!\n");
    exit(1);
  }
  return nptr;
}

static void xfree(void *ptr)
{
  if (ptr == NULL)
    panic("Got Null-Pointer!\n");
  free(ptr);
}

#endif