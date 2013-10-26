#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "net.h"
#ifndef MAX_COMMAND
#error MAX_COMMAND must be defined!
#endif

#define TIMEOUT (60)

Connection *c;
void signalhandler(int signum);

int main(int argc, char **argv) {
	struct sigaction sa;
	struct timespec idletime;
	int retval, running;
	char outbuf[MAX_COMMAND];
	char *cmdbuf;
	char *databuf;
	int command;
	short unsigned int cmdlen, datalen;
	CMDBuffer *buf;

	if(argc != 3) {
		fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
		goto error0;
	}

	c = connection_init(TIMEOUT);
	if(c == NULL) {
		fprintf(stderr, "main(): couldn't initialize connection context.\n");
		goto error0;
	}

	buf = cmdbuffer_init(MAX_COMMAND);
	if(buf == NULL) {
		goto error1;
	}
	connection_add_buffer(c, buf);

	sa.sa_handler = signalhandler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	retval = connection_connect(c, argv[1], argv[2], 0);
	if(retval == -1) {
		goto error2;
	}
	fprintf(stderr, "Successfully connected to %s(%s).\n", c->hostname, inet_ntoa(((struct sockaddr_in *)&(c->address))->sin_addr));

	running = 1;
	while(running) {
		retval = connection_next_command(c);
		if(retval == -1) { /* socket read error */
			fprintf(stderr, "Error reading from socket, disconnected.\n");
			goto error2;
		} else if(retval == 0) { /* full command received */
			command = command_parse(&cmdbuf, &cmdlen, &databuf, &datalen, c->buf->cmd, c->buf->cmdhave);
			switch(command) {
				case -2:
					connection_disconnect(c);
					fprintf(stderr, "Unknown command received from server, disconnected.\n");
					break;
				case -1:
					connection_disconnect(c);
					fprintf(stderr, "Parse error from server, disconnected.\n");
					break;
				case CMD_ERROR:
					fprintf(stderr, "A command from server has been dropped.\n");
					break;
				case CMD_PING:
					fprintf(stderr, "Ping? ");
					if(connection_pong(c) == -1) {
						fprintf(stderr, "Error!\n");
						connection_disconnect(c);
					} else {
						fprintf(stderr, "Pong!\n");
					}
					break;
				case CMD_PONG:
					c->pinged = 0;
					fprintf(stderr, "Pong received from server.\n");
			}
			cmdbuffer_reset(c->buf);
		}
		if(c->type == SERVER) { /* make sure we didn't disconnect it already */
			if(connection_timeout_check(c, 0)) {
				/* disconnect connection who hasn't responded or sent any data in a while */
				connection_disconnect(c);
				fprintf(stderr, "Server connection had no activity in %lu seconds, disconnected.\n", time(NULL) - c->last_message);
			}
		}
		if(c->type == NOTCONNECTED) {
			running = 0;
		}

		idletime.tv_sec = 0;
		idletime.tv_nsec = 1000000;
		nanosleep(&idletime, NULL);
	}

	free(buf);
	connection_free(c);
	exit(EXIT_SUCCESS);

error2:
	free(buf);
error1:
	connection_free(c);
error0:
	exit(EXIT_FAILURE);
}

void signalhandler(int signum) {
	fprintf(stderr, "\n\nSignal %i received.\n", signum);
	connection_free(c);
	exit(EXIT_SUCCESS);
}
