// make all
// make mount

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/sendfile.h>
#include "tikfs.h"

static int tikfs_fd;

static char *tikfs_disc = NULL;

static struct tikfs_fnode *table = NULL;

static size_t table_len = 0, table_next = 0;

static volatile sig_atomic_t flushing = 0, rwing = 0;

static void tikfs_flush_dirty_nodes_to_disc(void);

static int tikfs_file_type(const char *path, size_t *node)
{
  int ret = TIK_NONE;
  size_t i;
  const char *ptr = path + 1; /* Omit '/' char */
  if (strcmp(path, "/") == 0)
    return TIK_ROOT;
  for (i = 0; i < table_next; ++i) {
    if (!strncmp(table[i].meta.key, ptr,
           min(strlen(ptr), sizeof(table[i].meta.key)))) {
      *node = i;
      ret = TIK_FILE;
      break;
    }
  }
  return ret;
}

static int tikfs_getattr(const char *path, struct stat *stbuf)
{
  size_t node;

  /* uid / gid */
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();

  switch (tikfs_file_type(path, &node)) {
    case TIK_ROOT:
      /* mode, atime, mtime */
      stbuf->st_nlink = 1;
      stbuf->st_mode = 0755 | S_IFDIR;
      stbuf->st_atime = time(NULL);
      stbuf->st_mtime = time(NULL);
      break;
    case TIK_FILE:
      /* mode, nlink, size, atime, mtime */
      if(node < 0) {
        return -ENOENT;
      }

      if(table[node].dirty == TIK_CLEAN) {
        stbuf->st_size = table[node].meta.len;
      } else {
        stbuf->st_size = table[node].cowlen;
      }

      stbuf->st_nlink = 2;
      stbuf->st_mode = 0644 | S_IFREG;
      stbuf->st_atime = table[node].meta.ts_r;
      stbuf->st_mtime = table[node].meta.ts_w;
      break;
    case TIK_NONE:
    default:
      return -ENOENT;
  }
  return 0;
}

static int tikfs_open(const char *path, struct fuse_file_info *fi)
{
  size_t node;
  (void) fi;
  if (tikfs_file_type(path, &node) != TIK_NONE)
    return 0;
  return -ENOENT;
}

static int tikfs_read(const char *path, char *buff, size_t size,
           off_t offset, struct fuse_file_info *fi)
{
  size_t node;
  size_t len;
  ssize_t ret = 0;
  (void) fi;
  if (tikfs_file_type(path, &node) != TIK_FILE)
    return -EINVAL;
  while (flushing)
    sleep(0);

  if(table[node].dirty == TIK_CLEAN) {
    len = table[node].meta.len;
  } else {
    len = table[node].cowlen;
  }

  if(offset > len)
    return -EINVAL;
  if(offset + size > len)
    size = len - offset;
  rwing = 1;

  if(table[node].dirty == TIK_CLEAN) {
    lseek(tikfs_fd, table[node].data + offset, SEEK_SET);
    ret = read(tikfs_fd, buff, size);
  } else {
    memcpy(buff, table[node].cowbuff + offset, size);
    ret = size;
  }

  table[node].meta.ts_r = time(NULL);

  rwing = 0;
  return ret;
}

static int tikfs_truncate(const char *path, off_t size)
{
  size_t node;
  if (tikfs_file_type(path, &node) != TIK_FILE)
    return -EINVAL;
  return size;
}

static int tikfs_write(const char *path, const char *buff, size_t size,
           off_t offset, struct fuse_file_info *fi)
{
  size_t node;
  ssize_t ret;
  (void) fi;
  if (tikfs_file_type(path, &node) != TIK_FILE)
    return -EINVAL;
  while (flushing)
    sleep(0);
  rwing = 1;
  if(table[node].dirty == TIK_CLEAN) {
    table[node].dirty = TIK_DIRTY;
    table[node].cowlen = table[node].meta.len;
    table[node].cowbuff = xmalloc(table[node].cowlen);

    lseek(tikfs_fd, table[node].data, SEEK_SET);
    ret = read(tikfs_fd, table[node].cowbuff, table[node].cowlen);

    if(ret < table[node].cowlen) {
      xfree(table[node].cowbuff);
      table[node].cowlen = 0;
      table[node].dirty = TIK_CLEAN;

      size = -EIO;
      goto out;
    }
  }

  if(offset + size != table[node].cowlen) {
    table[node].cowlen = offset + size;
    table[node].cowbuff = xrealloc(table[node].cowbuff, table[node].cowlen);
  }

  memcpy(table[node].cowbuff + offset, buff, size);
  table[node].meta.ts_w = time(NULL);
out:
  rwing = 0;
  return size;
}

static int tikfs_readdir(const char *path, void *buff,
       fuse_fill_dir_t filler,
       off_t offset, struct fuse_file_info *fi)
{
  size_t i;
  size_t node;
  char tmp[256];
  (void) fi;
  (void) offset;
  if (tikfs_file_type(path, &node) != TIK_ROOT)
    return -ENOENT;
  filler(buff, ".", NULL, 0);
  filler(buff, "..", NULL, 0);
  for (i = 0; i < table_next; ++i) {
    memset(tmp, 0, sizeof(tmp));
    snprintf(tmp, sizeof(tmp), "%s", table[i].meta.key);
    tmp[sizeof(tmp) - 1] = 0;
    filler(buff, tmp, NULL, 0);
  }
  return 0;
}

static struct fuse_operations tikfs_ops = {
  .open   = tikfs_open,
  .read   = tikfs_read,
  .write    = tikfs_write,
  .getattr  = tikfs_getattr,
  .readdir  = tikfs_readdir,
  .truncate = tikfs_truncate,
};

static void tikfs_build_cache(void)
{
  ssize_t ret;
#define INIT_SLOTS  1024
  table = xmalloc(sizeof(*table) * INIT_SLOTS);
  table_len = INIT_SLOTS;
  table_next = 0;
  posix_fadvise(tikfs_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
  while (1) {
    ret = read(tikfs_fd, &table[table_next].meta,
         sizeof(table[table_next].meta));
    if (ret == 0)
      break;
    if (ret != sizeof(table[table_next].meta))
      goto die;
    if (table[table_next].meta.len == 0 ||
        table[table_next].meta.len == 0)
      goto die;
    table[table_next].data = lseek(tikfs_fd, 0, SEEK_CUR);
    table[table_next].dirty = TIK_CLEAN;
    table[table_next].cowbuff = NULL;
    table[table_next].cowlen = 0;
    ret = lseek(tikfs_fd, table[table_next].meta.len, SEEK_CUR);
    if (ret < 0)
      goto die;
    table_next++;
    if (table_next == table_len) {
      table_len = (size_t) table_len * 3 / 2;
      table = xrealloc(table, table_len);
    }
  }
  lseek(tikfs_fd, 0, SEEK_SET);
  posix_fadvise(tikfs_fd, 0, 0, POSIX_FADV_RANDOM);
  return;
die:
  syslog(LOG_ERR, "error parsing the tikdb file! corrupted?!\n");
  exit(1);
}

static inline void tikfs_destroy_cache(void)
{
  free(table);
  table = NULL;
  table_len = 0;
  table_next = 0;
}

static void ____tikfs_flush_dirty_nodes_to_disc_dirty(size_t i)
{
  ssize_t ret;
  table[i].meta.len = table[i].cowlen;
  ret = write(tikfs_fd, &table[i].meta, sizeof(table[i].meta));
  if (ret != sizeof(table[i].meta))
    syslog(LOG_ERR, "disc flush meta error at node %zu,"
           "continuing\n", i + 1);
  ret = write(tikfs_fd, table[i].cowbuff, table[i].cowlen);
  if (ret != table[i].cowlen)
    syslog(LOG_ERR, "disc flush error at dirty node %zu,"
           "continuing\n", i);
  table[i].cowlen = 0;
  free(table[i].cowbuff);
  table[i].cowbuff = NULL;
  table[i].dirty = TIK_CLEAN;
  table[i].data = lseek(tikfs_fd, 0, SEEK_CUR) - table[i].meta.len;
}

static void
____tikfs_flush_dirty_nodes_to_disc_clean(size_t i, int tikfs_fd2,
            off_t offshift)
{
  ssize_t ret;
  uint8_t *tmp;
  lseek(tikfs_fd2, table[i].data - offshift, SEEK_SET);
  ret = write(tikfs_fd, &table[i].meta, sizeof(table[i].meta));
  if (ret != sizeof(table[i].meta))
    syslog(LOG_ERR, "disc flush meta error at node %zu,"
           "continuing\n", i + 1);
  /* we cannot do a sendfile backwards :-( but chunks here are smaller */
  tmp = xmalloc(table[i].meta.len);
  ret = read(tikfs_fd2, tmp, table[i].meta.len);
  if (ret != table[i].meta.len)
    syslog(LOG_ERR, "disc flush error (%s) at clean node %zu read,"
           "continuing\n", strerror(errno), i);
  ret = write(tikfs_fd, tmp, table[i].meta.len);
  if (ret != table[i].meta.len)
    syslog(LOG_ERR, "disc flush error (%s) at clean node %zu write,"
           "continuing\n", strerror(errno), i);
  table[i].data = lseek(tikfs_fd, 0, SEEK_CUR) - table[i].meta.len;
  free(tmp);
}

static void __tikfs_flush_dirty_nodes_to_disc(size_t i_dirty, int tikfs_fd2,
                size_t *count, off_t offshift)
{
  size_t i;
  for (i = i_dirty; i < table_next; ++i) {
    if (table[i].dirty == TIK_DIRTY) {
      ____tikfs_flush_dirty_nodes_to_disc_dirty(i);
      (*count)++;
    } else {
      ____tikfs_flush_dirty_nodes_to_disc_clean(i, tikfs_fd2,
                  offshift);
    }
  }
}

static void tikfs_flush_dirty_nodes_to_disc(void)
{
  size_t i, count = 0;
  ssize_t ret;
  while (rwing)
    sleep(0);
  flushing = 1;
  posix_fadvise(tikfs_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
  for (i = 0; i < table_next; ++i) {
    if (!table[i].dirty == TIK_DIRTY)
      continue;
    if (table[i].dirty == TIK_DIRTY &&
        table[i].cowlen == table[i].meta.len) {
      lseek(tikfs_fd, table[i].data, SEEK_SET);
      ret = write(tikfs_fd, table[i].cowbuff,
            table[i].cowlen);
      if (ret != table[i].cowlen)
        syslog(LOG_ERR, "disc flush error at node "
               "%zu, continuing\n", i);
      table[i].dirty = TIK_CLEAN;
      table[i].cowlen = 0;
      free(table[i].cowbuff);
      table[i].cowbuff = NULL;
      count++;
    } else if (table[i].dirty == TIK_DIRTY) {
      int tikfs_fd2;
      size_t ii;
      char *tmpfile = "/tmp/tikfs.fubar";
      off_t offshift = table[i].data;
      size_t to_copy, chunk_size, chunk_blocks, chunk_rest;
      struct stat ost;
      fstat(tikfs_fd, &ost);
      tikfs_fd2 = open(tmpfile, O_RDWR | O_CREAT | O_TRUNC,
          S_IRUSR | S_IWUSR);
      if (tikfs_fd2 < 0) {
        syslog(LOG_ERR, "error creating temp file!\n");
        break;
      }
      posix_fadvise(tikfs_fd2, 0, 0, POSIX_FADV_SEQUENTIAL);
      to_copy = ost.st_size - table[i].data;
      chunk_size = ost.st_blksize;
      chunk_blocks = (size_t) (to_copy / chunk_size);
      chunk_rest = to_copy % chunk_size;
      lseek(tikfs_fd, table[i].data, SEEK_SET);
      for (ii = 0; ii < chunk_blocks; ++ii) {
        ret = sendfile(tikfs_fd2, tikfs_fd, NULL,
                 chunk_size);
        if (ret != chunk_size)
          syslog(LOG_ERR, "error (%s) while "
                 "splicing!\n", strerror(errno));
      }
      ret = sendfile(tikfs_fd2, tikfs_fd, NULL, chunk_rest);
      if (ret != chunk_rest)
        syslog(LOG_ERR, "error while tee'ing!\n");
      lseek(tikfs_fd2, 0, SEEK_SET);
      lseek(tikfs_fd, table[i].data -
            sizeof(struct tikdb_datahdr), SEEK_SET);
      ftruncate(tikfs_fd, table[i].data -
          sizeof(struct tikdb_datahdr));
      __tikfs_flush_dirty_nodes_to_disc(i, tikfs_fd2, &count,
                 offshift);
      close(tikfs_fd2);
      unlink(tmpfile);
      break;
    }
  }
  fsync(tikfs_fd);
  posix_fadvise(tikfs_fd, 0, 0, POSIX_FADV_RANDOM);
  flushing = 0;
  syslog(LOG_INFO, "%zu dirty marked node(s) flushed\n", count);
}

static void tikfs_check_superblock(void)
{
  ssize_t ret;
  struct tikdb_filehdr hdr;
  ret = read(tikfs_fd, &hdr, sizeof(hdr));
  if (ret != sizeof(hdr))
    goto die;
  if (hdr.magic != TIKDB_MAGIC)
    goto die;
  if (hdr.version_major != TIKDB_VERSION_MAJ)
    goto die;
  if (hdr.version_minor != TIKDB_VERSION_MIN)
    goto die;
  return;
die:
  fprintf(stderr, "this isn't a tikdb file!\n");
  exit(1);
}

static inline void tikfs_lock_disc(void)
{
  int ret = flock(tikfs_fd, LOCK_EX);
  if (ret < 0) {
    syslog(LOG_ERR, "cannot lock tikdb disc!\n");
    exit(1);
  }
}

static inline void tikfs_unlock_disc(void)
{
  flock(tikfs_fd, LOCK_UN);
}

static inline void tikfs_init_disc(void)
{
  tikfs_fd = open(tikfs_disc, O_RDWR | O_APPEND);
  if (tikfs_fd < 0) {
    syslog(LOG_ERR, "cannot open tikdb disc!\n");
    exit(1);
  }
}

static inline void tikfs_halt_disc(void)
{
  close(tikfs_fd);
}

static void tikfs_cleanup(void)
{
  tikfs_flush_dirty_nodes_to_disc();
  tikfs_destroy_cache();
  tikfs_unlock_disc();
  tikfs_halt_disc();
  syslog(LOG_INFO, "unmounted\n");
  closelog();
}

static void tikfs_init(void)
{
  openlog("tikfs", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  tikfs_init_disc();
  tikfs_lock_disc();
  tikfs_check_superblock();
  tikfs_build_cache();
  syslog(LOG_INFO, "mounted\n");
}

int main(int argc, char **argv)
{
  int i, ret;
  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
  if (argc < 3) {
    fprintf(stderr, "usage: tikfs <mntpoint> <tikdb>\n");
    exit(1);
  }

  for (i = 0; i < argc - 1; i++)
    fuse_opt_add_arg(&args, argv[i]);
  tikfs_disc = argv[argc - 1];
  tikfs_init();
  ret = fuse_main(args.argc, args.argv, &tikfs_ops, NULL);
  tikfs_cleanup();
  return ret;
}
