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
	client_main(argc, argv);
	return 0;
}