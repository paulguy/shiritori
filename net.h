#include <sys/socket.h>

typedef enum {
	NOTCONNECTED, SERVER, CLIENT
} connection_type;

typedef struct {
	char *cmd;
	int cmdsize;
	int cmdneeded;
	int cmdhave;
} CMDBuffer;

typedef struct {
	int sock;

	connection_type type;
	char *hostname;
	struct sockaddr address;

	time_t timeout;
	time_t last_message;
	int pinged;

	CMDBuffer *buf;
} Connection;

typedef struct {
	int sock;
	int connections;
	Connection **connection;

	time_t timeout;
} Server;

typedef struct {
	char name[8];
	unsigned short int length;
} Command;

extern const Command COMMANDS[];
#define		CMD_PING		(0)
#define		CMD_PONG		(1)
#define		CMD_ERROR		(2)
#define COMMANDS_MAX 		(3)
#define COMMANDS_MAX_LEN	(5)

/*
 * Initializes a new connection structure.
 *
 * timeout	Set timeout for connection in seconds.
 *
 * returns	New Connection structure or NULL on error.
 */
Connection *connection_init(int timeout);

/*
 * Adds a preallocated command buffer to a Connection.
 *
 * c		connection to add buffer to
 * b		buffer to add
 */
void connection_add_buffer(Connection *c, CMDBuffer *b);

/*
 * Attempts to close and free a Connection structure.
 *
 * c		Connection to free.
 */
void connection_free(Connection *c);

/*
 * Attempts to initiate a connection to a server at host and port.  Optional timeout override.
 *
 * c		Connection to use for connection, if c points to NULL, allocate a new Connection; timeout must be set.
 * host		Hostname or IP.
 * port		Port or service to connect to.
 * timeout	If greater than 0, change timeout in seconds.
 *
 * returns	0 on success, -1 on error.
 */
int connection_connect(Connection *c, char *host, char *port, int timeout);

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
 * returns	The number of the connection (index in to connections[]) on new connection, -1 on error, -2 on no new connection, -3 on max connections reached.
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
 * Read data from a socket.  On error, connection is closed.
 *
 * c		Connection to read data from.
 * buf		buffer to write data in to.
 * bytes	amount of bytes to read.
 *
 * returns	amount of bytes read or 0 if nothing to read or -1 on error.
 */
int connection_read(Connection *c, char *buf, int bytes);

/*
 * Write data to a socket. (Wraps write)
 *
 * c		Connection to write data to.
 * buf		buffer to read data from.
 * bytes	amount of bytes to write.
 *
 * returns	amount of bytes read or 0 if nothing to read or -1 on error, -2 on timeout.
 */
int connection_write(Connection *c, const char *buf, const int bytes);

/*
 * Checks a connection for timeout and disconnects it if so.
 *
 * c		Connection to check.
 * timeout	if greater than 0, check for non-default timeout.
 *
 * returns	0 on no timeout, -2 on timeout.
 */
int connection_timeout_check(Connection *c, int timeout);

/*
 * Initializes a new CMDBuffer.
 *
 * bsize	Size of command buffer.
 */
CMDBuffer *cmdbuffer_init(int bsize);

/*
 * Free a CMDBuffer.
 *
 * b		CMDBuffer to free.
 */
void cmdbuffer_free(CMDBuffer *b);

/*
 * Assembles commands from read buffer.
 *
 * c		Connection containing buffer to read from buffer to command.
 *
 * returns	0 on fully assembled command, -1 on error, -2 if no data read
 * 			nonzero if more data is needed (amount of data remaining)
 */
int connection_next_command(Connection *c);

/*
 * Ping a Connection.
 *
 * c		Connection to ping.
 *
 * returns	0 on success, -1 on error.
 */
int connection_ping(Connection *c);

/*
 * Pong a Connection.
 *
 * c		Connection to pong.
 *
 * returns	0 on success, -1 on error.
 */
int connection_pong(Connection *c);

/*
 * Reset CMDBuffer to initial state.
 *
 * b		CMDBuffer to reset.
 */
void cmdbuffer_reset(CMDBuffer *b);

/*
 * Generates a command from parts.
 *
 * buf		Buffer to write in to.
 * bufsize	Space in buffer.
 * cmd		Command block.
 * cmdsize	Command block size.
 * data		Data block, or NULL to exclude.
 * datasize	Data block size, should be 0 if absent.
 *
 * returns	Length of command on success, -1 on error.
 */
int command_generate(char *buf, unsigned short int bufsize, const char *cmd, const unsigned short int cmdsize, const char *data, const unsigned short int datasize);

/*
 * Parses out a command in to parts, inserting result in to arguments.  Data is NOT copied.
 *
 * cmd		Command is written here.
 * cmdsize	Command size is written here.
 * data		Data is written here.
 * datasize	Data size is written here.
 * buf		Source buffer of unparsed command.
 * bufsize	Size of unparsed command buffer (Only used to prevent overrun.).
 *
 * returns	Numerical command value on success, -1 on error.
 */
int command_parse(char **cmd, unsigned short int *cmdsize, char **data, unsigned short int *datasize, char *buf, unsigned short int bufsize);
