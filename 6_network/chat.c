// gcc -Wall -O2 chat.c -o chat -lncurses

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
#include "tiklib.h"

#define LISTENQ		64
#define INIT_QSIZ	(100*1024)

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
	char alias[32];
};

struct eventset {
	struct fdvec read;
	struct client clients[FD_SETSIZE];
};

static struct eventset eset;

static ssize_t write_exact(int fd, void *buf, size_t len)
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
		panic("Usage: chat server <port>\n");

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

static WINDOW *client_input_win, *client_board_win;

static void client_reset_input_win(void)
{
	wclear(client_input_win);
	mvwprintw(client_input_win, 0, 0, "----------[Connected]----------(F9 to exit)----------");
	mvwprintw(client_input_win, 1, 0, "> ");
	wrefresh(client_input_win);
}

static void client_refresh_tui(void)
{
	wrefresh(client_board_win);
	wrefresh(client_input_win);
}

static void client_close_tui(void)
{
	endwin();
}

static void client_init_tui(void)
{
	const int INPUT_WIN_HEIGHT = 7;

	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	timeout(50);
	refresh();

	client_input_win = newwin(INPUT_WIN_HEIGHT, COLS,
				  LINES - INPUT_WIN_HEIGHT, 0);
	client_reset_input_win();

	client_board_win = newwin(LINES - INPUT_WIN_HEIGHT, COLS, 0, 0);
	scrollok(client_board_win, TRUE);

	client_refresh_tui();
}

static void client_post_message(char *msg)
{
	wprintw(client_board_win, "%s\n", msg);
	wrefresh(client_board_win);
	wrefresh(client_input_win);
}

static void client_echo_character(char c)
{
	waddch(client_input_win, c);
	wrefresh(client_input_win);
}

static void client_main(int argc, char **argv)
{
	int fd, ret, ch;
	struct hostent *sent;
	struct sockaddr_in saddr;
	struct pollfd rpoll;
	char *rbuff, *wbuff;
	size_t rlen, wlen, wuse;
	ssize_t len;

	if (argc < 3)
		panic("Usage: chat client <ip/name> <port>\n");

	/* Connect to server, socket descriptor is 'fd' */
	/* ... */

	rlen = INIT_QSIZ;
	rbuff = xzmalloc(INIT_QSIZ);

	wlen = INIT_QSIZ;
	wuse = 0;
	wbuff = xzmalloc(INIT_QSIZ);

	/* Prepare polling */
	/* ... */

	client_init_tui();
	while ((ch = getch()) != KEY_F(9)) {
		switch (ch) {
		case ERR:
			/* Implement poll, read data and post it to the board */
			/* ... */
			client_post_message("Poll not yet implemented!\n");
			break;
		case '\n':
			wbuff[wlen - 1] = 0;
			len = write(fd, wbuff, strlen(wbuff));
			if (len < 0)
				panic("Connection to server lost!\n");
			memset(wbuff, 0, wlen);
			wuse = 0;
			client_reset_input_win();
			client_refresh_tui();
			break;
		default:
			if (wuse < wlen)
				wbuff[wuse++] = ch;
			client_echo_character(ch);
			break;
		}
	}

	client_close_tui();
	close(fd);
}

int main(int argc, char **argv)
{
	if (argc == 1)
		panic("Usage: chat {server,client} [...]\n");
	if (!strncmp("server", argv[1], min(strlen("server"),
		     strlen(argv[1]))))
		server_main(argc - 1, &argv[1]);
	else if (!strncmp("client", argv[1], min(strlen("client"),
			  strlen(argv[1]))))
		client_main(argc - 1, &argv[1]);
	else
		panic("Usage: chat {server,client} [...]\n");
	return 0;
}
