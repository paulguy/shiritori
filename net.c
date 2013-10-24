#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#include "net.h"

connection *connection_init() {
	connection *c;

	c = malloc(sizeof(connection));
	if(c == NULL)
		return(NULL);
	c->buf = malloc(BUF_SIZE);
	if(c->buf = NULL) {
		free(c);
		return(NULL);
	}

	c->sock = 0;
	c->buffersize = BUF_SIZE;
	c->bufferstart = 0;
	c->bufferend = 0;
	c->type = NOTCONNECTED;
	c->hostname = NULL;
	c->address = NULL;

	return(c);
}

void connection_free(connection *c) {
	connection_disconnect(connection *c);

	free(c->buf);
	free(c);
}

void connection_disconnect(connection *c) {
	/* Don't close stdin/out/err */
	if(c->sock > 2) {
		close(c->sock);
		c->type = NOTCONNECTED;
		c->sock = 0;
	}
}

Server *server_init(int port, int max_users) {
	Server *s;
	struct addrinfo hints;
	struct addrinfo *result, *rp; // first item, current item in linked list
	int retval;
	int opts;
	int i;
	fd_set rfds;
	struct timeval tv;

	s = malloc(sizeof(Server));
	if(s == NULL) {
		fprintf(stderr, "server_init(): Couldn't allocate memory.\n");
		goto serror0;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	retval = getaddrinfo(NULL, argv[1], &hints, &result);
	if (retval != 0) {
		fprintf(stderr, "server_init(): getaddrinfo(): %s\n", gai_strerror(retval));
		goto serror1;
	}

	/* getaddrinfo() returns a list of address structures.
	Try each address until we successfully bind(2).
	If socket(2) (or bind(2)) fails, we (close the socket
	and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		s->sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s->sock == -1)
			continue;
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
	}

	freeaddrinfo(result);           /* No longer needed */

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "server_init(): Could not bind\n");
		goto serror2;
	}

	s->connection = malloc(sizeof(Connection *) * max_users);
	if(s->connection == NULL) {
		fprintf(stderr, "server_init(): Couldn't allocate memory.\n");
		goto serror3;
	}
	for(i = 0; i < max_users; i++) {
		s->connection[i] = connection_init();
		if(s->connection[i] == NULL)
			break;
	}
	if(i < max_users - 1 && i > 0) {
		i--;
		for(; i >= 0; i--) {
			connection_free(s->connection[i]);
		}
		goto serror3;
	}

	/* Start listening */
	if(listen(s->sock, 10) == -1) {
		perror("server_init(): listen()");
		goto serror4;
	}

	if(fd_nonblocking(s->sock))
		goto serror4;

	return(s);

serror4:
	for(i = 0; i < max_users; i++) {
		connection_free(s->connection[i]);
	}
serror3:
	free(s->connection);
serror2:
	close(s->sock);
serror1:
	free(s);
serror0:
	return(NULL);
}

void server_free(Server *s) {
	int i;

	server_stop(s);
	server_close_all(s);
	for(i = 0; i < s->connections; i++)
		connection_free(s->connection[i]);
	free(s->connection);
	free(s);
}

void server_stop(Server *s) {
	/* Don't close stdin/out/err */
	if(s->sock > 2) {
		close(s->sock);
		s->sock = 0;
	}
}

void server_close_all(Server *s) {
	int i;

	for(i = 0; i < s->connections; i++)
		connection_disconnect(s->connection[i]);
}

int connection_accept(Server *s) {
	socklen_t addrlen;
	int sock;
	Connection *c;
	int i;

	for(i = 0; i < s->connections; i++) {
		if(s->connection[i]->type == NOTCONNECTED)
			c = s->connection[i];
		}
	}
	if(i == s->connections)
		return(-1);

	addrlen = sizeof(struct sockaddr);
	sock = accept(s->sock, &(c->address), &addrlen);
	if(sock < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return(0);
		else {
			perror("connection_accept(): accept()");
			return(-2);
		}
	}

	/* update connection and reset buffer */
	c->sock = sock;
	c->type = CLIENT;
	c->bufferstart = 0;
	c->bufferend = 0;

	return(1);
}

int fd_nonblocking(int fd) {
	opts = fcntl(c->sock, F_GETFL);
	if (opts < 0) {
		perror("socket_nonblocking(): fcntl(F_GETFL)");
		return(-1);
	}
	opts |= O_NONBLOCK;
	if (fcntl(c->sock, F_SETFL, opts) < 0) {
		perror("socket_nonblocking(): fcntl(F_SETFL)");
		return(-1);
	}

	return(0);
}
