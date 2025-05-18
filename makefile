MANAGER_OBJS = nfs_manager.o buffer_queue.o
MANAGER_OUT = nfs_manager
CONSOLE_OBJS = fss_console.o string.o
CONSOLE_OUT = fss_console
SOURCE	= nfs_manager.c buffer_queue.c
HEADER  = utils.h buffer_queue.h
CC = gcc
FLAGS = -g -c -Wall

all: $(MANAGER_OUT)

$(MANAGER_OUT): $(MANAGER_OBJS)
	$(CC) -g -o $(MANAGER_OUT) $(MANAGER_OBJS)

nfs_manager.o: nfs_manager.c utils.h buffer_queue.h
	$(CC) $(FLAGS) nfs_manager.c

buffer_queue.o: buffer_queue.c buffer_queue.h
	$(CC) $(FLAGS) buffer_queue.c


clean:
	rm -f $(MANAGER_OBJS) $(MANAGER_OUT)


count:
	wc $(SOURCE) $(HEADER)