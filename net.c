#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "net.h"

#ifndef MAX_COMMAND
#error MAX_COMMAND must be defined!
#endif

const  Command COMMANDS[] = {{"PING",	4},
                             {"PONG",	4},
                             {"ERROR",	5}};

static char outbuf[MAX_COMMAND];

Connection *connection_init(int timeout) {
	Connection *c;

	c = malloc(sizeof(Connection));
	if(c == NULL)
		return(NULL);

	c->sock = 0;
	c->type = NOTCONNECTED;
	c->hostname = NULL;
	c->buf = NULL;
	c->timeout = timeout;
	c->last_message = 0;
	c->pinged = 0;
	memset(&(c->address), 0, sizeof(struct sockaddr));

	return(c);
}

void connection_add_buffer(Connection *c, CMDBuffer *b) {
	c->buf = b;
}

void connection_free(Connection *c) {
	connection_disconnect(c);
	if(c->hostname != NULL)
		free(c->hostname);
	free(c);
}

int connection_connect(Connection *c, char *host, char *port, int timeout) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int retval;
	int sfd;
	int yes = 1; // used for setsockopt

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

	if (setsockopt(c->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("connection_connect(): setsockopt()");
		goto cerror1;
	}

	if(fd_nonblocking(c->sock))
		goto cerror1;

	// override timeout if greater than 0
	if(timeout > 0)
		c->timeout = timeout;

	c->type = SERVER;
	if(c->hostname != NULL)
		free(c->hostname);
	c->hostname = malloc(strlen(host) + strlen(port) + 2);
	memcpy(c->hostname, host, strlen(host));
	c->hostname[strlen(host)] = ':';
	memcpy(&(c->hostname[strlen(host) + 1]), port, strlen(port));
	c->last_message = time(NULL);
	c->pinged = 0;

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
	}
	c->type = NOTCONNECTED;
	c->sock = 0;
	if(c->buf != NULL)
		cmdbuffer_reset(c->buf);
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
		goto serror2;
	}

	/* allocate memory for max connections */
	s->connection = malloc(sizeof(Connection *) * max_users);
	if(s->connection == NULL) {
		fprintf(stderr, "server_init(): Couldn't allocate memory.\n");
		goto serror2;
	}
	for(i = 0; i < max_users; i++) {
		s->connection[i] = connection_init(timeout);
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
	struct sockaddr address;
	socklen_t addrlen;
	int sock;
	Connection *c;
	int i;

	addrlen = sizeof(struct sockaddr);
	sock = accept(s->sock, &address, &addrlen);
	if(sock < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return(-2);
		else {
			perror("connection_accept(): accept()");
			return(-1);
		}
	}

	for(i = 0; i < s->connections; i++) {
		if(s->connection[i]->type == NOTCONNECTED) {
			c = s->connection[i];
			break;
		}
	}

	if(i == s->connections) {
		fprintf(stderr, "connection_accept(): New connection from %s, but max connections reached (%i).\n",
		        inet_ntoa(((struct sockaddr_in *)&(address))->sin_addr), s->connections);
		close(sock);
		return(-3);
	}

	c->sock = sock;
	memcpy(&(c->address), &address, addrlen);
	c->type = CLIENT;
	c->timeout = s->timeout;
	c->last_message = time(NULL);
	c->pinged = 0;

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
		return(0);
	} else if(retval > 0) {
		c->last_message = time(NULL);
		c->pinged = 0;
		return(retval);
	}
	/* else */
	if(errno == EAGAIN || errno == EWOULDBLOCK) {
		return(0);
	}

	connection_disconnect(c);
	return(-1);
}

int connection_write(Connection *c, const char *buf, const int bytes) {
	return(write(c->sock, buf, bytes));
}

int connection_timeout_check(Connection *c, int timeout) {
	if(timeout != 0) {
		if(time(NULL) - c->last_message > timeout) {
			return(-2);
		}
	} else {
		if(time(NULL) - c->last_message > c->timeout) {
			return(-2);
		}
	}

	return(0);
}

CMDBuffer *cmdbuffer_init(int bsize) {
	CMDBuffer *b;

	b = malloc(sizeof(CMDBuffer));
	if(b == NULL)
		goto berror0;

	b->cmd = malloc(bsize);
	if(b->cmd == NULL)
		goto berror1;

	b->cmdsize = bsize;
	cmdbuffer_reset(b);

	return(b);

berror1:
	free(b);
berror0:
	return(NULL);
}

void cmdbuffer_free(CMDBuffer *b) {
	free(b->cmd);
	free(b);
}

static const char *protoerror = "\0\0ERROR";
static const int protoerrorlen = 7;

int connection_next_command(Connection *c) {
	int retval;

	if(c->buf == NULL)
		return(-1);

	if(c->buf->cmdsize < protoerrorlen) /* needs at least protoerrorlen bytes. unlikely but special use or misuse may cause this */
		return(-1);

	if(c->buf->cmdneeded == 0) { /* We don't know how much we need, yet. */
		if(c->buf->cmdhave == 0) { /* We have nothing so we need 2 bytes */
			retval = connection_read(c, c->buf->cmd, 2); /* Try to read 2 bytes */
			if(retval == -1) /* error */
				return(-1);
			if(retval == 1) { /* only 1 bytes received, we need 1 more */
				c->buf->cmdhave = 1;
				return(1);
			} else if(retval == 2) { /* we have all 2 bytes */
				c->buf->cmdhave = 2;
			} else { /* retval == 0, we didn't get anything */
				return(2);
			}
		} else { /* cmdhave == 1, we still need 1 more */
			retval = connection_read(c, &(c->buf->cmd[1]), 1);
			if(retval == -1)
				return(-1);
			if(retval == 1) { /* we have all 2 bytes */
				c->buf->cmdhave = 2;
			} else { /* retval == 0, we didn't get anything, we still need 1 more */
				return(1);
			}
		}
		if(c->buf->cmdhave == 2) { /* Check if we have enough to know how much we need */
			c->buf->cmdneeded = ntohs(*((unsigned short int *)(c->buf->cmd)));
		}
	} else { /* we know how much we need, so get it */
		if(c->buf->cmdneeded > c->buf->cmdsize) { /* incoming command is too big, discard it */
			retval = connection_read(c, c->buf->cmd, c->buf->cmdsize < c->buf->cmdneeded - c->buf->cmdhave ?
				                                     c->buf->cmdsize : c->buf->cmdneeded - c->buf->cmdhave);
			if(retval == -1) /* error */
				return(-1);
			c->buf->cmdhave += retval;
			if(c->buf->cmdhave == c->buf->cmdneeded) { /* we've eaten the overly large command, report the error */
				memcpy(c->buf->cmd, protoerror, protoerrorlen);
				c->buf->cmdhave = protoerrorlen;
				c->buf->cmdneeded = protoerrorlen;
			}
		} else { /* command will fit */
			if(c->buf->cmdhave < c->buf->cmdneeded) { /* we need more data */
				retval = connection_read(c, &(c->buf->cmd[c->buf->cmdhave]), c->buf->cmdneeded - c->buf->cmdhave);
				if(retval == -1) /* error */
					return(-1);
				c->buf->cmdhave += retval;
			}
		}
	}

	if(c->buf->cmdhave < c->buf->cmdneeded) /* if we don't have enough, report how much we need. */
		return(c->buf->cmdneeded - c->buf->cmdhave);

	return(0); /* we have enough */
}

int connection_ping(Connection *c) {
	int len;

	len = command_generate(outbuf, MAX_COMMAND, COMMANDS[CMD_PING].name, COMMANDS[CMD_PING].length, NULL, 0);
	if(len == -1)
		return(-1);
	if(connection_write(c, outbuf, len) == -1)
		return(-1);

	c->pinged = 1;

	return(0);
}

int connection_pong(Connection *c) {
	int len;

	len = command_generate(outbuf, MAX_COMMAND, COMMANDS[CMD_PONG].name, COMMANDS[CMD_PONG].length, NULL, 0);
	if(len == -1)
		return(-1);
	if(connection_write(c, outbuf, len) == -1)
		return(-1);

	return(0);
}

void cmdbuffer_reset(CMDBuffer *b) {
	b->cmdneeded = 0;
	b->cmdhave = 0;
}

int command_generate(char *buf, const unsigned short int bufsize, const char *cmd, const unsigned short int cmdsize, const char *data, unsigned short int datasize) {
	int totalsize;

	totalsize = cmdsize + datasize + 2;
	if(totalsize > bufsize || totalsize > 65536)
		return(-1);

	*((unsigned short int *)buf) = htons(totalsize);
	memcpy(&(buf[2]), cmd, cmdsize);
	if(data != NULL)
		memcpy(&(buf[2 + cmdsize]), data, datasize);

	return(totalsize);
}

int command_parse(char **cmd, unsigned short int *cmdsize, char **data, unsigned short int *datasize, char *buf, unsigned short int bufsize) {
	int i;
	unsigned short int hdrsize;

	if(bufsize < 2)
		return(-1);
	hdrsize = ntohs(*((unsigned short int *)buf));

	if(bufsize < hdrsize) /* Preliminary size checks */
		return(-1);

	for(i = 0; i < COMMANDS_MAX; i++) {
		if(bufsize < COMMANDS[i].length + 2)
			return(-1);
		if(memcmp(&(buf[2]), COMMANDS[i].name, COMMANDS[i].length) == 0) {
			*cmd = &(buf[2]);
			*cmdsize = COMMANDS[i].length;
			*data = &(buf[2 + *cmdsize]);
			*datasize = hdrsize - *cmdsize - 2;
			return(i);
		}
	}

	*cmd = &(buf[2]);
	*cmdsize = hdrsize - 2;
	*data = NULL;
	*datasize = 0;
	return(-2);
}
