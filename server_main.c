#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "net.h"

#define MAX_USERS (8)
#define TIMEOUT (60)

Server *s;
void signalhandler(int signum);

int main(int argc, char **argv) {
	int running;
	int retval;
	int i;
	struct sigaction sa;
	struct timespec idletime;
	CMDBuffer **bufs;
	char outbuf[MAX_COMMAND];
	char *cmdbuf;
	char *databuf;
	int command;
	short unsigned int cmdlen, datalen;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		goto error0;
	}

	s = server_init(argv[1], MAX_USERS, TIMEOUT);
	if(s == NULL) {
		fprintf(stderr, "main(): couldn't initialize server.\n");
		goto error0;
	}

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

	/* We'll need command buffers, so initialize all of them */
	bufs = malloc(sizeof(CMDBuffer *) * s->connections);
	if(bufs == NULL) {
		goto error1;
	}
	for(i = 0; i < s->connections; i++) {
		bufs[i] = cmdbuffer_init(MAX_COMMAND);
		if(bufs[i] == NULL)
			break;
		connection_add_buffer(s->connection[i], bufs[i]);
	}
	if(i < s->connections) {
		i--;
		for(; i >= 0; i--)
			free(bufs[i]);
		goto error2;
	}

	running = 1;
	while(running) {
		retval = connection_accept(s);
		if(retval >= 0) {
			fprintf(stderr, "New connection from %s.\n", inet_ntoa(((struct sockaddr_in *)&(s->connection[retval]->address))->sin_addr));
		} else if(retval == -1) {
			fprintf(stderr, "Error accepting connection.\n");
			goto error3;
		}

		for(i = 0; i < s->connections; i++) {
			if(s->connection[i]->type == CLIENT) {
				retval = connection_next_command(s->connection[i]);
				if(retval == -1) { /* socket read error */
					connection_disconnect(s->connection[i]);
					fprintf(stderr, "Error reading from socket, disconnected.\n");
				} else if(retval == 0) { /* full command received */
					command = command_parse(&cmdbuf, &cmdlen, &databuf, &datalen, s->connection[i]->buf->cmd, s->connection[i]->buf->cmdhave);
					switch(command) {
						case -2:
							connection_disconnect(s->connection[i]);
							fprintf(stderr, "Unknown command received from %i, disconnected.\n", i);
							break;
						case -1:
							connection_disconnect(s->connection[i]);
							fprintf(stderr, "Parse error from %i, disconnected.\n", i);
							break;
						case CMD_ERROR:
							fprintf(stderr, "A command from %i has been dropped.\n", i);
							break;
						case CMD_PING:
							if(connection_pong(s->connection[i]) == -1) {
								fprintf(stderr, "Failed to pong %i.\n", i);
								connection_disconnect(s->connection[i]);
							} else {
								fprintf(stderr, "Ponged %i.\n", i);
							}
							break;
						case CMD_PONG:
							s->connection[i]->pinged = 0;
							fprintf(stderr, "Pong received from %i.\n", i);
					}
					cmdbuffer_reset(s->connection[i]->buf);
				}
			}
			if(s->connection[i]->type == CLIENT) { /* make sure we didn't disconnect it already */
				if(connection_timeout_check(s->connection[i], 0)) {
					/* disconnect connection who hasn't responded or sent any data in a while */
					connection_disconnect(s->connection[i]);
					fprintf(stderr, "Connection %i had no activity in %lu seconds, disconnected.\n", i, time(NULL) - s->connection[i]->last_message);
				} else if(s->connection[i]->pinged == 0 &&
				          connection_timeout_check(s->connection[i], s->connection[i]->timeout / 2)) {
					/* ping the connection to create some activity and reset timeout timer */
					if(connection_ping(s->connection[i]) == -1) {
						fprintf(stderr, "Failed to ping %i.\n", i);
						connection_disconnect(s->connection[i]);
					}
					fprintf(stderr, "Pinged %i.\n", i);
				}
			}
		}

		idletime.tv_sec = 0;
		idletime.tv_nsec = 1000000;
		nanosleep(&idletime, NULL);
	}

	for(i = 0; i < s->connections; i++)
		free(bufs[i]);
	free(bufs);
	server_free(s);
	exit(EXIT_SUCCESS);

error3:
	for(i = 0; i < s->connections; i++)
		free(bufs[i]);
error2:
	free(bufs);
error1:
	server_free(s);
error0:
	exit(EXIT_FAILURE);
}

void signalhandler(int signum) {
	fprintf(stderr, "\n\nSignal %i received.\n", signum);
	server_free(s);
	exit(EXIT_SUCCESS);
}
