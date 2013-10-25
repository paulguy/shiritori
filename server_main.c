#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "net.h"

#define MAX_USERS (8)
#define TIMEOUT (60)
#define MAX_READ (1024)

Server *s;
void signalhandler(int signum);

int main(int argc, char *argv[]) {
	char readbuf[MAX_READ + 1];
	int running;
	int retval;
	int i;
	struct sigaction sa;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sa.sa_handler = signalhandler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	s = server_init(argv[1], MAX_USERS, TIMEOUT);
	if(s == NULL) {
		fprintf(stderr, "main(): couldn't initialize server.\n");
		exit(EXIT_FAILURE);
	}

	running = 1;

	while(running) {
		retval = connection_accept(s);
		if(retval >= 0)
			fprintf(stderr, "New connection from %s.\n", inet_ntoa(((struct sockaddr_in *)&(s->connection[retval]->address))->sin_addr));
		else if(retval == -1) {
			fprintf(stderr, "Error accepting connection.\n");
			server_free(s);
			exit(EXIT_FAILURE);
		}

		for(i = 0; i < s->connections; i++) {
			if(s->connection[i]->type == CLIENT) {
				retval = connection_read(s->connection[i], readbuf, MAX_READ);
				if(retval == -1) {
					fprintf(stderr, "Error reading from socket.\n");
				} else if(retval == -2) {
					fprintf(stderr, "Connection %i timed out.\n", i);
				} else if(retval > 0) {
					readbuf[retval] = '\0';
					fprintf(stderr, "%i(%i): %s", i, retval, readbuf);
					connection_write(s->connection[i], readbuf, retval);
				}
			}
		}
	}

	server_free(s);
	exit(EXIT_SUCCESS);
}

void signalhandler(int signum) {
	fprintf(stderr, "\n\nSignal %i received.\n", signum);
	server_free(s);
	exit(EXIT_SUCCESS);
}
