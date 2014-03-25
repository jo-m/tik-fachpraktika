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
  /* Message: 'alias foobar' */
  /* Set alias to connected user */
  /* ... */
}

static void server_process_broadcast(int fd)
{
  int i;
  for (i = 0; i < FD_SETSIZE; ++i) {
    if (eset.clients[i].active && i != fd) {
      server_write_fd_queue(i, "From ");
      if (eset.clients[fd].has_alias) {
        server_write_fd_queue(i, eset.clients[fd].alias);
      } else {
        server_write_fd_queue(i,
          inet_ntoa(eset.clients[fd].sin.sin_addr));
      }
      server_write_fd_queue(i, ": ");
      server_write_fd_queue(i, eset.clients[fd].inbuff);
      server_schedule_write(i);
    }
  }
}

static void server_process_who(int fd)
{
  /* Message: 'who' */
  /* List connected people (either IP or if available Alias) */
  /* ... */
}

static void server_process_private(int fd)
{
  /* Message: '@foobar message' */
  /* Send a private message, give a note that it's private */
  /* ... */
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
  } else if (strlen(eset.clients[fd].inbuff) == strlen("who") &&
       !strnicmp(eset.clients[fd].inbuff, "who", strlen("who"))) {
    server_process_who(fd);
  } else if (strlen(eset.clients[fd].inbuff) > strlen("alias ") &&
       !strnicmp(eset.clients[fd].inbuff, "alias ", strlen("alias "))) {
    server_process_alias(fd);
  } else if (!strncmp(eset.clients[fd].inbuff, "@", strlen("@"))) {
    server_process_private(fd);
  } else {
    server_process_broadcast(fd);
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

  printf("Waiting for connection ...\n");
  while (1)
    server_handle_events(&eset);

  close(lfd);
}

int main(int argc, char **argv)
{
  server_main(argc, argv);
  return 0;
}
