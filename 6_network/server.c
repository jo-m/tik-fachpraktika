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

#define  MAGIC_STR_WHO "\\who"
#define  MAGIC_STR_ALIAS "\\alias "
#define  MAGIC_STR_PRIVATE "@"
#define  MAGIC_STR_HELP "\\help"
#define  MAGIC_STR_COW "\\cow "

#define ALIAS_MAXLEN 15
#define ALIAS_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_-"

struct fdvec {
  fd_set fds;
  void (*f[FD_SETSIZE])(int fd);
  int max;
};

struct client {
  int active;
  struct sockaddr_in sin;
  char *outbuff;
  size_t out_len, out_used;
  char *inbuff;
  size_t in_len;
  int has_alias;
  char alias[ALIAS_MAXLEN + 1];
};

struct eventset {
  struct fdvec read;
  struct client clients[FD_SETSIZE];
};

static struct eventset eset;

static void server_init_fdvec(struct fdvec *v)
{
  FD_ZERO(&v->fds);
  memset(&v->f, 0, sizeof(v->f));
  v->max = 0;
}

static void server_init_eventset(struct eventset *e)
{
  server_init_fdvec(&e->read);
  memset(&e->clients, 0, sizeof(e->clients));
}

static void server_handle_events(struct eventset *e)
{
  int i, ret;
  fd_set set;
  /* Note: select changes the descriptor set inplace */
  memcpy(&set, &e->read.fds, sizeof(set));
  ret = select(e->read.max + 1, &set, 0, 0, 0);
  if (ret <= 0)
    return;
  for (i = 0; i < e->read.max + 1; ++i) {
    if (FD_ISSET(i, &set)) {
      e->read.f[i](i);
    }
  }
}

static void server_event_register_fd(struct fdvec *v, int fd,
             void (*f)(int fd))
{
  FD_SET(fd, &v->fds);
  v->f[fd] = f;
  if (fd > v->max) {
    v->max = fd;
  }
}

static void server_event_unregister_fd(struct fdvec *v, int fd)
{
  FD_CLR(fd, &v->fds);
  v->f[fd] = NULL;
  if (fd == v->max) {
    int i;
    for (i = 0; i < fd; ++i) {
      if (FD_ISSET(i, &v->fds)) {
        v->max = i;
      }
    }
  }
}

static void server_disconnect_client(int fd)
{
  eset.clients[fd].active = 0;
  eset.clients[fd].has_alias = 0;
  server_event_unregister_fd(&eset.read, fd);

  free(eset.clients[fd].outbuff);
  eset.clients[fd].out_len = eset.clients[fd].out_used = 0;
  free(eset.clients[fd].inbuff);
  eset.clients[fd].in_len = 0;

  close(fd);
  printf("Client %s unregistered!\n",
         inet_ntoa(eset.clients[fd].sin.sin_addr));
}

static void server_flush_queued_data(int fd)
{
  ssize_t ret;
  if (eset.clients[fd].active == 0)
    return;
  if (eset.clients[fd].out_used == 0)
    return;
  if (eset.clients[fd].out_used >= eset.clients[fd].out_len)
    eset.clients[fd].out_used = eset.clients[fd].out_len - 1;
  eset.clients[fd].outbuff[eset.clients[fd].out_used] = 0;
  if (eset.clients[fd].outbuff[eset.clients[fd].out_used - 1] == '\n')
    eset.clients[fd].outbuff[eset.clients[fd].out_used - 1] = 0;
  ret = write_exact(fd, eset.clients[fd].outbuff,
        eset.clients[fd].out_used + 1);
  if (ret <= 0) {
    server_disconnect_client(fd);
    return;
  }
  eset.clients[fd].out_used = 0;
  memset(eset.clients[fd].outbuff, 0, eset.clients[fd].out_len);
}

static void server_write_fd_queue(int fd, char *data)
{
  ssize_t diff = eset.clients[fd].out_len - eset.clients[fd].out_used;
  if (diff > strlen(data)) {
    strlcat(eset.clients[fd].outbuff, data,
      eset.clients[fd].out_len);
    eset.clients[fd].out_used += strlen(data);
  }
}

static void server_schedule_write(int fd)
{
  server_flush_queued_data(fd);
}

static void server_process_alias(int fd)
{
  int i;
  struct client *c = &eset.clients[fd], *c2 = NULL;
  char *alias = c->inbuff + strlen(MAGIC_STR_ALIAS);

  if(c->has_alias) {
    server_write_fd_queue(fd, "<Server alias cmd>: You have already set an alias.");
    server_schedule_write(fd);
    return;
  }

  if(strlen(alias) > ALIAS_MAXLEN) {
    server_write_fd_queue(fd, "<Server alias cmd>: Sorry, this alias is too long (max 32 chars).");
    server_schedule_write(fd);
    return;
  }

  for(i = 0; alias[i] != 0; i++) {
    if(strchr(ALIAS_ALLOWED_CHARS, alias[i]) == NULL) {
      server_write_fd_queue(fd, "<Server alias cmd>: Sorry, this alias contains invalid chars (allowed: a-zA-Z0-9_-).");
      server_schedule_write(fd);
      return;
    }
  }

  for(i = 0; i <= eset.read.max; c2 = &eset.clients[++i]) {
    if(FD_ISSET(i, &eset.read.fds) && c2->active && c2->has_alias && fd != i) {
      if(0 == strnicmp(alias, c2->alias, strlen(alias))) {
        server_write_fd_queue(fd, "<Server alias cmd>: Sorry, this alias is already in use.");
        server_schedule_write(fd);
        return;
      }
    }
  }

  c->has_alias = 1;
  memcpy(c->alias, alias, strlen(alias) + 1);
  server_write_fd_queue(fd, "<Server alias cmd>: OK, alias set to ");
  server_write_fd_queue(fd, alias);
  server_write_fd_queue(fd, ".");
  server_schedule_write(fd);
}

static char* server_get_client_repr(int fd) {
  struct client *c = &eset.clients[fd];
  if (c->has_alias) {
    return c->alias;
  } else {
    return inet_ntoa(c->sin.sin_addr);
  }
}

static void server_process_message_from(int fd)
{
  int i;
  for (i = 0; i < FD_SETSIZE; ++i) {
    if (eset.clients[i].active) {
      server_write_fd_queue(i, server_get_client_repr(fd));
      if(i == fd) {
        server_write_fd_queue(i, " (you) ");
      }
      server_write_fd_queue(i, ": ");
      server_write_fd_queue(i, eset.clients[fd].inbuff);
      server_schedule_write(i);
    }
  }
}

static void server_process_help(int fd)
{
  char *commands[] = {MAGIC_STR_WHO, MAGIC_STR_ALIAS, MAGIC_STR_PRIVATE, MAGIC_STR_HELP, MAGIC_STR_COW, 0};
  char *desc[] = {
    "Shows a list of all available users",
    "Set your alias (\\alias <name>)",
    "Private messages (@<alias> <message>)",
    "Shows this help",
    "Invoke Cowsay (\\cowsay <message>)"
  };
  int i;

  for(i = 0; commands[i] != 0; i ++) {
    server_write_fd_queue(fd, "<Server help cmd>: ");
    server_write_fd_queue(fd, commands[i]);
    server_write_fd_queue(fd, " -> ");
    server_write_fd_queue(fd, desc[i]);
    server_write_fd_queue(fd, "\n");
  }

  server_write_fd_queue(fd, "<Server who cmd>: -- End of List --\n");
  server_schedule_write(fd);
}

static void server_process_cow(int fd)
{
  int i;
  struct client *c = &eset.clients[fd], *c2 = NULL;
  char *msg = c->inbuff + strlen(MAGIC_STR_COW);
  char cowsay[] = "cowsay ";
  char *cmd = malloc(sizeof(char) * (strlen(cowsay) + strlen(msg) + 2));
  #define RESULT_BUFLEN (1024 * 8)
  char result[RESULT_BUFLEN + 1] = {0};
  int read_b;

  memset(cmd, 0, sizeof(char) * (strlen(cowsay) + strlen(msg) + 2));
  memcpy(cmd, cowsay, strlen(cowsay));
  memcpy(cmd + strlen(cowsay), msg, strlen(msg));

  FILE *proc = popen(cmd, "r");

  read_b = fread(result, 1, RESULT_BUFLEN, proc);

  if(read_b > 0) {
    for (i = 0; i < FD_SETSIZE; ++i) {
      if (eset.clients[i].active) {
        server_write_fd_queue(i, "<Server cowsay cmd>:\n");
        server_write_fd_queue(i, result);
        server_write_fd_queue(i, "<Server cowsay cmd>: this comes from user ");
        server_write_fd_queue(i, server_get_client_repr(fd));
        server_write_fd_queue(i, ".\n");
        server_schedule_write(i);
      }
    }
  } else {
    server_write_fd_queue(fd, "<Server cowsay cmd>: 'cowsay' command not installed on server!\n");
    server_schedule_write(fd);
  }

  fclose(proc);
  free(cmd);
}

static void server_process_who(int fd)
{
  int i;
  struct client *c = NULL;
  for(i = 0; i <= eset.read.max; c = &eset.clients[++i]) {
    if(FD_ISSET(i, &eset.read.fds) && c->active) {
      server_write_fd_queue(fd, "<Server who cmd>: ");
      server_write_fd_queue(fd, server_get_client_repr(i));
      if(fd == i) {
        server_write_fd_queue(fd, " (you)");
      }
      server_write_fd_queue(fd, "\n");
    }
  }
  server_write_fd_queue(fd, "<Server who cmd>: -- End of List --\n");
  server_schedule_write(fd);
}

static void server_process_private(int fd)
{
  int i, c_to_fd = -1;
  struct client *c = &eset.clients[fd], *c_to = NULL;
  char alias[ALIAS_MAXLEN + 1] = { 0 }, chr = 0, *msg = NULL;

  for(i = 0; i < ALIAS_MAXLEN; i++) {
    chr = c->inbuff[i + strlen(MAGIC_STR_PRIVATE)];
    if(chr == ' ' || chr == '\0')
      break;
    alias[i] = chr;
    msg = &c->inbuff[i + 1 + strlen(MAGIC_STR_PRIVATE)];
  }

  for(i = 0; i <= eset.read.max; c_to = &eset.clients[++i]) {
    if(FD_ISSET(i, &eset.read.fds) && c_to->active && c_to->has_alias && fd != i) {
      if(0 == strnicmp(alias, c_to->alias, strlen(alias))) {
        c_to_fd = i;
        break;
      }
    }
  }

  if(c_to_fd == -1) {
    server_write_fd_queue(fd, "<Server @ cmd>: Alias not found.");
    server_schedule_write(fd);
    return;
  }

  server_write_fd_queue(fd, server_get_client_repr(fd));
  server_write_fd_queue(fd, " (-> ");
  server_write_fd_queue(fd, server_get_client_repr(c_to_fd));
  server_write_fd_queue(fd, "): ");
  server_write_fd_queue(fd, msg);
  server_schedule_write(fd);

  server_write_fd_queue(c_to_fd, server_get_client_repr(fd));
  server_write_fd_queue(c_to_fd, " [priv] :");
  server_write_fd_queue(c_to_fd, msg);
  server_schedule_write(c_to_fd);
}

static void server_process_client(int fd)
{
  ssize_t ret;
  memset(eset.clients[fd].inbuff, 0, eset.clients[fd].in_len);
  ret = read(fd, eset.clients[fd].inbuff, eset.clients[fd].in_len);
  if (ret <= 0 || strlen(eset.clients[fd].inbuff) == 0) {
    if (errno == EAGAIN)
      return;
    if (errno != 0)
      perror("Error reading from client");
    server_disconnect_client(fd);
    return;
  } else if (strlen(eset.clients[fd].inbuff) == strlen(MAGIC_STR_WHO) &&
       !strnicmp(eset.clients[fd].inbuff, MAGIC_STR_WHO, strlen(MAGIC_STR_WHO))) {
    server_process_who(fd);
  } else if (strlen(eset.clients[fd].inbuff) > strlen(MAGIC_STR_ALIAS) &&
       !strnicmp(eset.clients[fd].inbuff, MAGIC_STR_ALIAS, strlen(MAGIC_STR_ALIAS))) {
    server_process_alias(fd);
  } else if (strlen(eset.clients[fd].inbuff) == strlen(MAGIC_STR_HELP) &&
       !strnicmp(eset.clients[fd].inbuff, MAGIC_STR_HELP, strlen(MAGIC_STR_HELP))) {
    server_process_help(fd);
  } else if (strlen(eset.clients[fd].inbuff) > strlen(MAGIC_STR_COW) &&
       !strnicmp(eset.clients[fd].inbuff, MAGIC_STR_COW, strlen(MAGIC_STR_COW))) {
    server_process_cow(fd);
  } else if (!strncmp(eset.clients[fd].inbuff, MAGIC_STR_PRIVATE, strlen(MAGIC_STR_PRIVATE))) {
    server_process_private(fd);
  } else {
    server_process_message_from(fd);
  }
}

static void server_accept_client(int lfd)
{
  int nfd;
  socklen_t clen;
  struct sockaddr_in caddr;

  clen = sizeof(caddr);
  nfd = accept(lfd, (struct sockaddr *) &caddr, &clen);
  if (nfd < 0)
    return;

  fcntl(nfd, F_SETFL, fcntl(nfd, F_GETFL, 0) | O_NONBLOCK);
  assert(eset.clients[nfd].active == 0);

  memset(&eset.clients[nfd], 0, sizeof(struct client));
  memcpy(&eset.clients[nfd].sin, &caddr, sizeof(caddr));
  eset.clients[nfd].active = 1;
  eset.clients[nfd].has_alias = 0;

  eset.clients[nfd].outbuff = xzmalloc(INIT_QSIZ);
  eset.clients[nfd].out_len = INIT_QSIZ;
  eset.clients[nfd].out_used = 0;

  eset.clients[nfd].inbuff = xzmalloc(INIT_QSIZ);
  eset.clients[nfd].in_len = INIT_QSIZ;

  server_event_register_fd(&eset.read, nfd, server_process_client);

  server_write_fd_queue(nfd, "Hello from server!\n");
  server_write_fd_queue(nfd, "Enter '\\help' for a list of commands.\n");
  server_schedule_write(nfd);

  printf("Client %s registered!\n", inet_ntoa(caddr.sin_addr));
}

static void server_main(int argc, char **argv)
{
  int lfd, ret, one;
  struct sockaddr_in saddr;

  if (argc == 1)
    panic("Usage: ./server <port>\n");

  lfd = socket(AF_INET, SOCK_STREAM, 0);
  if (lfd < 0)
    panic("Cannot create socket!\n");

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(atoi(argv[argc - 1]));

  one = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

  ret = bind(lfd, (struct sockaddr *) &saddr, sizeof(saddr));
  if (ret < 0)
    panic("Cannot bind socket!\n");

  ret = listen(lfd, LISTENQ);
  if (ret < 0)
    panic("Cannot listen on socket!\n");

  fcntl(lfd, F_SETFL, fcntl(lfd, F_GETFL, 0) | O_NONBLOCK);

  server_init_eventset(&eset);
  server_event_register_fd(&eset.read, lfd, server_accept_client);

  printf("Waiting for connection on port %d...\n", ntohs(saddr.sin_port));
  while (1)
    server_handle_events(&eset);

  close(lfd);
}

int main(int argc, char **argv)
{
  server_main(argc, argv);
  return 0;
}
