#include <sys/socket.h>

typedef enum {
	NOTCONNECTED, SERVER, CLIENT
} connection_type;

typedef struct {
	int sock;

	connection_type type;
	char *hostname;
	struct sockaddr address;

	time_t timeout;
	time_t last_message;
} Connection;

typedef struct {
	int sock;
	int connections;
	Connection **connection;

	time_t timeout;
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
Server *server_init(char *port, int max_users, int timeout);

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
 * returns	The number of the connection (index in to connections[]) on new connection, -1 on error, -2 on no new connection.
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

/*
 * Read data from a socket.  On error or timeout, connection is closed.
 *
 * c		Connection to read data from.
 * buf		buffer to write data in to.
 * bytes	amount of bytes to read.
 *
 * return	amount of bytes read or 0 if nothing to read or -1 on error, -2 on timeout.
 */
int connection_read(Connection *c, char *buf, int bytes);

/*
 * Write data to a socket. (Wraps write)
 *
 * c		Connection to write data to.
 * buf		buffer to read data from.
 * bytes	amount of bytes to write.
 *
 * return	amount of bytes read or 0 if nothing to read or -1 on error, -2 on timeout.
 */
int connection_write(Connection *c, char *buf, int bytes);

