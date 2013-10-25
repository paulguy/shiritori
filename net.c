#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "net.h"

Connection *connection_init(int timeout) {
	Connection *c;

	c = malloc(sizeof(Connection));
	if(c == NULL)
		return(NULL);

	c->sock = 0;
	c->type = NOTCONNECTED;
	c->hostname = NULL;
	c->timeout = timeout;
	c->last_message = 0;
	memset(&(c->address), 0, sizeof(struct sockaddr));

	return(c);
}

void connection_free(Connection *c) {
	connection_disconnect(c);
	free(c);
}

int *connection_connect(Connection *c, char *host, char *port, int timeout) {
	struct addrinfo hints;
	struct addrinfo *result, *rp
	int retval;
	int sfd;

	if(c == NULL) {
		if(timeout <= 0)
			return(-1);
		c = connection_init(timeout);
		if(c == NULL)
			goto cerror0;
	}

	/* Obtain address(es) matching host/port */

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */

	retval = getaddrinfo(host, port, &hints, &result);
	if (retval != 0) {
		fprintf(stderr, "connection_connect(): getaddrinfo(): %s\n", gai_strerror(retval));
		goto cerror0;
	}

	/* getaddrinfo() returns a list of address structures.
	Try each address until we successfully connect(2).
	If socket(2) (or connect(2)) fails, we (close the socket
	and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (c->sock == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		close(sfd);
	}

	freeaddrinfo(result);           /* No longer needed */

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "connection_connect(): Could not connect\n");
		goto cerror0;
	}

	// Copy socket address info in to connection.
	c->sock = sfd;
	memcpy(&(c->address), rp->ai_addr, sizeof(struct sockaddr));

	if (setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("connection_connect(): setsockopt()");
		goto cerror1;
	}

	if(fd_nonblocking(s->sock))
		goto cerror1;

	// override timeout if greater than 0
	if(timeout > 0)
		c->timeout = timeout;

	return(0);

cerror1:
	connection_disconnect(c);
cerror0:
	return(-1);
}

void connection_disconnect(Connection *c) {
	/* Don't close stdin/out/err */
	if(c->sock > 2) {
		close(c->sock);
		c->type = NOTCONNECTED;
		c->sock = 0;
	}
}

Server *server_init(char *port, int max_users, int timeout) {
	Server *s;
	struct addrinfo hints;
	struct addrinfo *result, *rp; // first item, current item in linked list
	int retval;
	int i;
	int yes = 1; // used for setsockopt

	s = malloc(sizeof(Server));
	if(s == NULL) {
		fprintf(stderr, "server_init(): Couldn't allocate memory.\n");
		goto serror0;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	retval = getaddrinfo(NULL, port, &hints, &result);
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
		if (bind(s->sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
	}

	freeaddrinfo(result);           /* No longer needed */

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "server_init(): Could not bind\n");
		goto serror2;
	}

	if (setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("server_init(): setsockopt()");
		goto serror2
	}

	/* allocate memory for max connections */
	s->connection = malloc(sizeof(Connection *) * max_users);
	if(s->connection == NULL) {
		fprintf(stderr, "server_init(): Couldn't allocate memory.\n");
		goto serror2;
	}
	for(i = 0; i < max_users; i++) {
		s->connection[i] = connection_init();
		if(s->connection[i] == NULL)
			break;
	}
	/* if not all connections could be allocated, free what has been */
	if(i < max_users - 1 && i > 0) {
		i--;
		for(; i >= 0; i--) {
			connection_free(s->connection[i]);
		}
		goto serror3;
	}
	s->connections = max_users;
	s->timeout = timeout;

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
	server_stop(s);
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
		if(s->connection[i]->type == NOTCONNECTED) {
			c = s->connection[i];
			break;
		}
	}

	if(i == s->connections) {
		fprintf(stderr, "connection_accept(): Max connections reached.\n");
		return(-1);
	}

	addrlen = sizeof(struct sockaddr);
	sock = accept(s->sock, &(c->address), &addrlen);
	if(sock < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return(-2);
		else {
			perror("connection_accept(): accept()");
			return(-1);
		}
	}

	c->sock = sock;
	c->type = CLIENT;
	c->timeout = s->timeout;
	c->last_message = time(NULL);

	if(fd_nonblocking(c->sock)) {
		fprintf(stderr, "connection_accept(): Couldn't make socket nonblocking.\n");
		connection_disconnect(c);
		return(-1);
	}

	return(i);
}

int fd_nonblocking(int fd) {
	int opts;

	opts = fcntl(fd, F_GETFL);
	if (opts < 0) {
		perror("socket_nonblocking(): fcntl(F_GETFL)");
		return(-1);
	}
	opts |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, opts) < 0) {
		perror("socket_nonblocking(): fcntl(F_SETFL)");
		return(-1);
	}

	return(0);
}

int connection_read(Connection *c, char *buf, int bytes) {
	int retval;

	retval = read(c->sock, buf, bytes);

	if(retval == 0) {
		if(time(NULL) - c->last_message > c->timeout) {
			connection_disconnect(c);
			return(-2);
		}
		return(0);
	} else if(retval > 0) {
		c->last_message = time(NULL);
		return(retval);
	}
	/* else */
	if(errno == EAGAIN || errno == EWOULDBLOCK) {
		if(time(NULL) - c->last_message > c->timeout) {
			connection_disconnect(c);
			return(-2);
		}
		return(0);
	}

	connection_disconnect(c);
	return(-1);
}

int connection_write(Connection *c, char *buf, int bytes) {
	return(write(c->sock, buf, bytes));
}
