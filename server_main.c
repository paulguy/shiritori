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
#include "game.h"

#ifndef MAX_COMMAND
#error MAX_COMMAND must be defined!
#endif

#define MAX_USERS (8)
#define MAX_NAME_LEN (32)
#define TIMEOUT (60)

int running;
void signalhandler(int signum);

int main(int argc, char **argv) {
	Game *g;
	Server *s;
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
	if(bufs == NULL)
		goto error1;
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

	g = game_init(MAX_USERS, MAX_NAME_LEN);
	if(g == NULL)
		goto error3;

	running = 1;
	while(running) {
		retval = connection_accept(s);
		if(retval >= 0) {
			fprintf(stderr, "New connection from %s.\n", inet_ntoa(((struct sockaddr_in *)&(s->connection[retval]->address))->sin_addr));
			if(connection_message(s->connection[retval], "SERVER\0Connection established, please identify.") == -1) {
				fprintf(stderr, "Failed to send message to %i.\n", i);
				connection_disconnect(s->connection[i]);
			}
		} else if(retval == -1) {
			fprintf(stderr, "Error accepting connection.\n");
			goto error4;
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
							if(g->player[i]->c == s->connection[i]) {
								player_disconnect(g->player[i]);
							} else { /* not identified */
								connection_disconnect(s->connection[i]);
							}
							fprintf(stderr, "Unknown command received from %i, disconnected.\n", i);
							break;
						case -1:
							if(g->player[i]->c == s->connection[i]) {
								player_disconnect(g->player[i]);
							} else { /* not identified */
								connection_disconnect(s->connection[i]);
							}
							fprintf(stderr, "Parse error from %i, disconnected.\n", i);
							break;
						case CMD_ERROR:
							fprintf(stderr, "A command from %i has been dropped.\n", i);
							break;
						case CMD_PING:
							if(connection_pong(s->connection[i]) == -1) {
								fprintf(stderr, "Failed to pong %i.\n", i);
								if(g->player[i]->c == s->connection[i]) {
									player_disconnect(g->player[i]);
								} else { /* not identified */
									connection_disconnect(s->connection[i]);
								}
							} else {
								fprintf(stderr, "Ponged %i.\n", i);
							}
							break;
						case CMD_PONG:
							s->connection[i]->pinged = 0;
							fprintf(stderr, "Pong received from %i.\n", i);
						case CMD_USER:
							databuf[datalen] = '\0';
							if(datalen <= MAX_NAME_LEN || memcmp(databuf, "SERVER", 6) != 0) {
								fprintf(stderr, "Connection %i username is now %s.\n", i, databuf);
								g->player[i]->c = s->connection[i];
								memcpy(g->player[i]->name, databuf, datalen + 1);
							} else { /* username is too long or equals "SERVER" */
								fprintf(stderr, "Connection %i specified invalid username %s.", i, databuf);
								if(connection_message(s->connection[i], "SERVER\0Invalid username!") == -1) {
									fprintf(stderr, "Failed to send message to %i.\n", i);
									connection_disconnect(s->connection[i]);
								}
							}
							break;
						default:
							fprintf(stderr, "Unimplemented command %s!\n", COMMANDS[command].name);
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
						if(g->player[i]->c == s->connection[i]) {
							player_disconnect(g->player[i]);
						} else { /* not identified */
							connection_disconnect(s->connection[i]);
						}
					}
					fprintf(stderr, "Pinged %i.\n", i);
				}
			}
		}

		idletime.tv_sec = 0;
		idletime.tv_nsec = 1000000;
		nanosleep(&idletime, NULL);
	}

	game_free(g);
	for(i = 0; i < s->connections; i++)
		free(bufs[i]);
	free(bufs);
	server_free(s);
	exit(EXIT_SUCCESS);

error4:
	game_free(g);
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
	running = 0;
	return;
}
