#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "tiklib.h"

#define LISTENQ		64

// gcc -Wall -O2 chat-simple.c -o chat-simple -lncurses

static void server_main(int argc, char **argv)
{
	int lfd, nfd, ret, one;
	size_t slen;
	socklen_t clen;
	ssize_t len;
	struct sockaddr_in saddr, caddr;
	char buffer[256], welcome[512];

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

	ret = bind(lfd, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in));
	if (ret < 0)
		panic("Cannot bind socket!\n");

	ret = listen(lfd, LISTENQ);
	if (ret < 0)
		panic("Cannot listen on socket!\n");

	while (1) {
		printf("Waiting for connection ...\n");

		clen = sizeof(caddr);
		nfd = accept(lfd, (struct sockaddr *) &caddr, &clen);
		if (nfd < 0) {
			whine("Error on accept!\n");
			continue;
		}

		printf("Connection from a new client!\n");
		slen = strlen("Hi! Here's a simple server. What's your name?") + 1;
		len = write(nfd, "Hi! Here's a simple server. What's your "
			    "name?", slen);
		if (len <= 0) {
			whine("Error greeting client!\n");
			goto out;
		}

		memset(buffer, 0, sizeof(buffer));
		len = read(nfd, buffer, sizeof(buffer));
		if (len <= 0) {
			whine("Error reading from client\n");
			goto out;
		}

		buffer[sizeof(buffer) - 1] = 0;
		printf("The client's name is: %s\n", buffer);

		memset(welcome, 0, sizeof(welcome));
		snprintf(welcome, sizeof(welcome), "Hi %s. What's your "
			 "message?", buffer);

		len = write(nfd, welcome, strlen(welcome) + 1);
		if (len <= 0) {
			whine("Error sending welcome to client!\n");
			goto out;
		}

		memset(buffer, 0, sizeof(buffer));
		len = read(nfd, buffer, sizeof(buffer));
		if (len <= 0) {
			whine("Error reading from client\n");
			goto out;
		}

		buffer[sizeof(buffer) - 1] = 0;
		printf("The message is: %s\n", buffer);

		slen = strlen("Thanks, bye!") + 1;
		len = write(nfd, "Thanks, bye!", slen);
		if (len <= 0) {
			whine("Error sending welcome to client!\n");
			goto out;
		}
out:
		close(nfd);
	}
	close(lfd);
}

static void client_main(int argc, char **argv)
{
	int fd, ret;
	ssize_t len;
	struct hostent *sent;
	struct sockaddr_in saddr;
	char buffer[256];

	if (argc < 3)
		panic("Usage: chat client <ip/name> <port>\n");

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

	memset(buffer, 0, sizeof(buffer));
	len = read(fd, buffer, sizeof(buffer));
	if (len <= 0)
		panic("Cannot read from server!\n");

	buffer[sizeof(buffer) - 1] = 0;
	printf("Server says: %s\n", buffer);

	printf("> ");
	fflush(stdout);
	fgets(buffer, sizeof(buffer), stdin);
	buffer[sizeof(buffer) - 1] = 0; // \0
	buffer[strlen(buffer) - 1] = 0; // \n

	len = write(fd, buffer, strlen(buffer) + 1);
	if (len <= 0)
		panic("Could not write answer to server!\n");

	memset(buffer, 0, sizeof(buffer));
	len = read(fd, buffer, sizeof(buffer));
	if (len <= 0)
		panic("Cannot read from server!\n");

	buffer[sizeof(buffer) - 1] = 0;
	printf("Server says: %s\n", buffer);

	printf("> ");
	fflush(stdout);
	fgets(buffer, sizeof(buffer), stdin);
	buffer[sizeof(buffer) - 1] = 0; // \0
	buffer[strlen(buffer) - 1] = 0; // \n

	len = write(fd, buffer, strlen(buffer) + 1);
	if (len <= 0)
		panic("Could not write answer to server!\n");

	memset(buffer, 0, sizeof(buffer));
	len = read(fd, buffer, sizeof(buffer));
	if (len <= 0)
		panic("Cannot read from server!\n");

	buffer[sizeof(buffer) - 1] = 0;
	printf("Server says: %s\n", buffer);

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
