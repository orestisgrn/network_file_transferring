MANAGER_OBJS = nfs_manager.o buffer_queue.o
MANAGER_OUT = nfs_manager
CONSOLE_OBJS = nfs_console.o
CONSOLE_OUT = nfs_console
SOURCE	= nfs_manager.c buffer_queue.c nfs_console.c
HEADER  = utils.h buffer_queue.h
CC = gcc
FLAGS = -g -c -Wall

all: $(MANAGER_OUT) $(CONSOLE_OUT)

$(MANAGER_OUT): $(MANAGER_OBJS)
	$(CC) -g -o $(MANAGER_OUT) $(MANAGER_OBJS)

$(CONSOLE_OUT): $(CONSOLE_OBJS)
	$(CC) -g -o $(CONSOLE_OUT) $(CONSOLE_OBJS)

nfs_manager.o: nfs_manager.c utils.h buffer_queue.h
	$(CC) $(FLAGS) nfs_manager.c

buffer_queue.o: buffer_queue.c buffer_queue.h
	$(CC) $(FLAGS) buffer_queue.c

nfs_console.o: nfs_console.c
	$(CC) $(FLAGS) nfs_console.c

clean:
	rm -f $(MANAGER_OBJS) $(MANAGER_OUT) $(CONSOLE_OBJS) $(CONSOLE_OUT) 


count:
	wc $(SOURCE) $(HEADER)