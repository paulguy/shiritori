OBJS   = net.o server_main.o
TARGET = shiritori_server
CFLAGS = -pedantic -Wall -Wextra -std=gnu99 -ggdb
#CFLAGS = -pedantic -Wall -Wextra -std=gnu99
LDFLAGS = 

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

all: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
