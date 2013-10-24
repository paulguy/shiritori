#define BUF_SIZE 1024

typedef enum {
	NOTCONNECTED, SERVER, CLIENT
} connection_type;

typedef struct {
	int sock;
	char *buf;
	int buffersize;
	int bufferstart;
	int bufferend;

	connection_type type;
	char *hostname;
	struct sockaddr address;
} Connection;

typedef struct {
	int sock;
	int nconnections;
	Connection *connection;
} Server;

/*
 * Initializes a new connection structure.
 *
 * returns	New Connection structure or NULL on error.
 */
Connection *connection_init();

/*
 * Attempts to close and free a Connection structure.
 *
 * c		Connection to free.
 */
void connection_free(Connection *c);

/*
 * Disconnects a currently connected socket or does nothing if it's already disconnected.
 *
 * c		Connection to disconnect.
 */
void connection_disconnect(Connection *c);

/*
 * Initializes server to start listening for incoming connections.
 *
 * port			Port on which to listen.
 * max_users	Maximum number of connections.
 *
 * returns		New Server structure or NULL on error.
 */
Server *server_init(int port, int max_users);

/*
 * Close server and all connections, then free all resources associated with a server.
 *
 * s		Server to free.
 */
void server_free(Server *s);

/*
 * Stops a server but keeps it's structure and connections open.
 *
 * s		Server to stop.
 */
void server_stop(Server *s);

/*
 * Closes all connections on a server without freeing them.
 *
 * s		Server to close all connections on.
 */
void server_close_all(Server *s);

/*
 * Check for and accept a connection on an open server.
 *
 * s		Server to accept a connection on.
 *
 * returns	1 on new connection, 0 on no new connection, -1 on max connections reached, -2 on accept() failure.
 */
int connection_accept(Server *s);

/*
 * Makes a file descriptor nonblocking.
 *
 * fd		File descriptor to be made nonblocking.
 *
 * returns	0 on success, -1 on failure.
 */
int fd_nonblocking(int fd);
