/* Implementation is lame, but not targeted for a high amount of
 * key-vals anyway */

// gcc -o tikdb tikdb.c

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
#include "tikfs.h"

static int __create_db(char *name)
{
  int fd, ret;
  struct tikdb_filehdr db;
  fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    panic("Cannot open file!\n");
  memset(&db, 0, sizeof(db));
  db.magic = TIKDB_MAGIC;
  db.version_major = TIKDB_VERSION_MAJ;
  db.version_minor = TIKDB_VERSION_MIN;
  ret = write(fd, &db, sizeof(db));
  if (ret != sizeof(db))
    panic("Could not write db\n");
  return fd;
}

static void create_db(char *name)
{
  close(__create_db(name));
  exit(0);
}

static void check_db_hdr(int fd)
{
  int ret;
  struct tikdb_filehdr db;
  memset(&db, 0, sizeof(db));
  ret = read(fd, &db, sizeof(db));
  if (ret != sizeof(db))
    panic("Could not read db\n");
  if (db.magic != TIKDB_MAGIC)
    panic("Wrong magic number!\n");
  if (db.version_major != TIKDB_VERSION_MAJ)
    panic("Wrong version major number!\n");
  if (db.version_minor != TIKDB_VERSION_MIN)
    panic("Wrong version minor number!\n");
}

static void __add_key_val(char *db, char *key, char *val, size_t vlen,
        uint8_t *db_pay, size_t db_len)
{
  int ret, written = 0, fd = __create_db(db);
  off_t off = 0;
  while (off < db_len) {
    struct tikdb_datahdr *hdr;
    hdr = (struct tikdb_datahdr *) (db_pay + off);
    if (!written &&
        !memcmp(hdr->key, key, min(strlen(key),
          sizeof(hdr->key) - 1))) {
      struct tikdb_datahdr thdr;
      memset(&thdr, 0, sizeof(thdr));
      memcpy(thdr.key, key, min(strlen(key),
             sizeof(thdr.key) - 1));
      thdr.len = vlen;
      thdr.ts_r = thdr.ts_w = time(NULL);
      ret = write(fd, &thdr, sizeof(thdr));
      if (ret != sizeof(thdr))
        panic("Writing to db .. corrupted!\n");
      ret = write(fd, val, vlen);
      if (ret != vlen)
        panic("Writing to db .. corrupted!\n");
      written = 1;
    } else {
      ret = write(fd, hdr, sizeof(*hdr));
      if (ret != sizeof(*hdr))
        panic("Writing to db .. corrupted!\n");
      ret = write(fd, db_pay + off + sizeof(*hdr), hdr->len);
      if (ret != hdr->len)
        panic("Writing to db .. corrupted!\n");
    }
    off += sizeof(struct tikdb_datahdr);
    off += hdr->len;
  }
  if (!written) {
    struct tikdb_datahdr thdr;
    memset(&thdr, 0, sizeof(thdr));
    memcpy(thdr.key, key, min(strlen(key), sizeof(thdr.key) - 1));
    thdr.len = vlen;
    thdr.ts_r = thdr.ts_w = time(NULL);
    ret = write(fd, &thdr, sizeof(thdr));
    if (ret != sizeof(thdr))
      panic("Writing to db .. corrupted!\n");
    ret = write(fd, val, vlen);
    if (ret != vlen)
      panic("Writing to db .. corrupted!\n");
  }
  fsync(fd);
  close(fd);
}

static void add_key_val(char *db, char *key, char *val, size_t vlen)
{
  int fd, ret;
  struct stat buff;
  uint8_t *db_pay;
  size_t db_len;
  fd = open(db, O_RDWR);
  if (fd < 0)
    panic("Cannot open file!\n");
  check_db_hdr(fd);
  ret = fstat(fd, &buff);
  if (ret < 0)
    panic("Cannot fstat file!\n");
  db_len = buff.st_size - sizeof(struct tikdb_filehdr);
  if (db_len > 0) {
    db_pay = xmalloc(db_len);
    ret = read(fd, db_pay, db_len);
    if (ret != db_len)
      panic("Cannot read out db!\n");
    close(fd);
    __add_key_val(db, key, val, vlen, db_pay, db_len);
    xfree(db_pay);
  } else {
    struct tikdb_datahdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.key, key, min(strlen(key), sizeof(hdr.key) - 1));
    hdr.len = vlen;
    hdr.ts_r = hdr.ts_w = time(NULL);
    ret = write(fd, &hdr, sizeof(hdr));
    if (ret != sizeof(hdr))
      panic("Writing to db .. corrupted!\n");
    ret = write(fd, val, vlen);
    if (ret != vlen)
      panic("Writing to db .. corrupted!\n");
    fsync(fd);
    close(fd);
  }
  exit(0);
}

static void get_key_val(char *db, char *key)
{
  int fd, ret;
  struct tikdb_datahdr hdr;
  fd = open(db, O_RDONLY);
  if (fd < 0)
    panic("Cannot open file!\n");
  check_db_hdr(fd);
  while (read(fd, &hdr, sizeof(hdr)) > 0) {
    if (!memcmp(hdr.key, key, min(strlen(key),
          sizeof(hdr.key) - 1))) {
      char *buff = xmalloc(hdr.len);
      ret = read(fd, buff, hdr.len);
      if (ret != hdr.len)
        panic("Cannnot read value!\n");
      printf("%s", buff);
      fflush(stdout);
      xfree(buff);
#if 0
      ret = sendfile(0, fd, 0, hdr.len);
      if (ret != hdr.len)
        panic("Cannnot read value!\n");
      fflush(stdout);
#endif
      break;
    } else {
      lseek(fd, hdr.len, SEEK_CUR);
    }
    memset(&hdr, 0, sizeof(hdr));
  }
  close(fd);
  exit(0);
}

int main(int argc, char**argv)
{
  if (argc < 2)
    panic("Too few arguments!\n");
  if (!strcmp(argv[1], "create") && argc == 3)
    create_db(argv[2]);
  if (!strcmp(argv[1], "get") && argc == 4)
    get_key_val(argv[2], argv[3]);
  if (!strcmp(argv[1], "add") && argc == 5)
    add_key_val(argv[2], argv[3], argv[4], strlen(argv[4]));
  printf("Usage i.e.:\n");
  printf("   tikdb create todo.db\n");
  printf(" Add key-value:\n");
  printf("   tikdb add todo.db \"25.02.2012\" \"Watch new Dr Who episodes\"\n");
  printf(" Update key-value:\n");
  printf("   tikdb add todo.db \"25.02.2012\" \"Watch old Dr Who episodes\"\n");
  printf(" Retrival:\n");
  printf("   tikdb get todo.db \"25.02.2012\"\n");
  printf("Note: max key size is 31 ASCII chars\n");
  return 0;
}
