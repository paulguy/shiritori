COMMONOBJS	= net.o
SERVEROBJS	= server_main.o
CLIENTOBJS	= main.o
SERVER		= shiritori_server
CLIENT		= shiritori

CFLAGS		= -pedantic -Wall -Wextra -std=gnu99 -DMAX_COMMAND=\(1024\) -ggdb
#CFLAGS		= -pedantic -Wall -Wextra -std=gnu99 -DMAX_COMMAND=\(1024\)
LDFLAGS		= 

all:		$(SERVER) $(CLIENT)

$(SERVER):	$(COMMONOBJS) $(SERVEROBJS)
	$(CC) $(LDFLAGS) -o $(SERVER) $(SERVEROBJS) $(COMMONOBJS) 

$(CLIENT):	$(COMMONOBJS) $(CLIENTOBJS)
	$(CC) $(LDFLAGS) -o $(CLIENT) $(CLIENTOBJS) $(COMMONOBJS)

clean:
	rm -f $(COMMONOBJS) $(SERVEROBJS) $(CLIENTOBJS) $(SERVER) $(CLIENT)

.PHONY: clean
