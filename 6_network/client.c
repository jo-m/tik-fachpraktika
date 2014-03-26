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

#define READ_POLL_TIMEOUT_MS 100

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
		panic("Usage: ./client <ip/name> <port>\n");

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		panic("Cannot create socket!\n");

	sent = gethostbyname(argv[1]);
	if (!sent)
		panic("No such host!\n");

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	memcpy(&saddr.sin_addr.s_addr, sent->h_addr, sent->h_length);
	saddr.sin_port = htons((uint16_t) atoi(argv[2]));

	ret = connect(fd, (struct sockaddr *) &saddr, sizeof(saddr));

	if (ret < 0)
		panic("Cannot connect to server!\n");
	printf("Connection successful!\n");

	rlen = INIT_QSIZ;
	rbuff = xzmalloc(INIT_QSIZ);

	wlen = INIT_QSIZ;
	wuse = 0;
	wbuff = xzmalloc(INIT_QSIZ);

	/* Prepare polling */
	rpoll.fd = fd;
	rpoll.events = POLLIN | POLLPRI;
	rpoll.revents = 0;

	client_init_tui();
	while ((ch = getch()) != KEY_F(9)) {
		switch (ch) {
		case ERR:
			/* Implement poll, read data and post it to the board */
			ret = poll(&rpoll, 1, READ_POLL_TIMEOUT_MS);
			if(ret > 0) {
				len = read(fd, rbuff, rlen);
				if (len <= 0)
					panic("Cannot read from server!\n");

				rbuff[rlen - 1] = 0;
				client_post_message(rbuff);
			}
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
	client_main(argc, argv);
	return 0;
}
