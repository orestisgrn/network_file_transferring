MANAGER_OBJS = nfs_manager.o buffer_queue.o string.o
MANAGER_OUT = nfs_manager
CONSOLE_OBJS = nfs_console.o string.o
CONSOLE_OUT = nfs_console
CLIENT_OBJS = nfs_client.o string.o
CLIENT_OUT = nfs_client
SOURCE	= nfs_manager.c buffer_queue.c nfs_console.c string.c nfs_client.c 
HEADER  = utils.h buffer_queue.h string.h
CC = gcc
FLAGS = -g -c -Wall

all: $(MANAGER_OUT) $(CONSOLE_OUT) $(CLIENT_OUT)

$(MANAGER_OUT): $(MANAGER_OBJS)
	$(CC) -g -o $(MANAGER_OUT) $(MANAGER_OBJS) -lpthread

$(CONSOLE_OUT): $(CONSOLE_OBJS)
	$(CC) -g -o $(CONSOLE_OUT) $(CONSOLE_OBJS)

$(CLIENT_OUT): $(CLIENT_OBJS)
	$(CC) -g -o $(CLIENT_OUT) $(CLIENT_OBJS) -lpthread

nfs_manager.o: nfs_manager.c utils.h buffer_queue.h string.h
	$(CC) $(FLAGS) nfs_manager.c -lpthread

buffer_queue.o: buffer_queue.c buffer_queue.h
	$(CC) $(FLAGS) buffer_queue.c

string.o: string.c string.h
	$(CC) $(FLAGS) string.c

nfs_console.o: nfs_console.c utils.h string.h
	$(CC) $(FLAGS) nfs_console.c

nfs_client.o: nfs_client.c utils.h string.h
	$(CC) $(FLAGS) nfs_client.c -lpthread

clean:
	rm -f $(MANAGER_OBJS) $(MANAGER_OUT) nfs_console.o nfs_console nfs_client.o nfs_client


count:
	wc $(SOURCE) $(HEADER)