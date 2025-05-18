#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "buffer_queue.h"

int worker_limit = 5;
short port_number = -1;    // may need to change type later

Buffer_Queue work_queue;

int main(int argc,char **argv) {
    char opt = '\0';
    char *logname = NULL;
    char *config = NULL;
    int buffer_size=0;
    while (*(++argv) != NULL) {                 // Command line arguments handle
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            char *wrong_char;
            switch (opt) {
                case 'l':
                    logname = *argv;
                    break;
                case 'c':
                    config = *argv;
                    break;
                case 'n':
                    wrong_char=NULL;
                    worker_limit = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Worker limit must be int\n"),ARGS_ERR);
                    }
                    if (worker_limit < 1) {
                        CLEAN_AND_EXIT(perror("Worker limit must be a positive integer\n"),ARGS_ERR);
                    }
                    break;
                case 'p':
                    wrong_char=NULL;
                    port_number = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Port number must be int\n"),ARGS_ERR);
                    }
                    if (port_number < 0) {
                        CLEAN_AND_EXIT(perror("Port number must be a 16-bit integer\n"),ARGS_ERR);
                    }
                    break;
                case 'b':
                    wrong_char=NULL;
                    buffer_size = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Port number must be int\n"),ARGS_ERR);
                    }
                    if (buffer_size < 1) {
                        CLEAN_AND_EXIT(perror("Buffer size must be a positive integer\n"),ARGS_ERR);
                    }
                    break;
                case '\0':
                    CLEAN_AND_EXIT(perror("Argument without option\n"),ARGS_ERR);
                default:
                    CLEAN_AND_EXIT(fprintf(stderr,"-%c is not an option\n",opt),ARGS_ERR);
            }
            opt = 0;
        }
    }
    if (logname==NULL) {
        CLEAN_AND_EXIT(perror("No logfile given\n"),ARGS_ERR);
    }
    if (buffer_size==0) {
        CLEAN_AND_EXIT(perror("No buffer size given\n"),ARGS_ERR);
    }
    if (port_number==-1) {
        CLEAN_AND_EXIT(perror("No port number given\n"),ARGS_ERR);
    }
    if (opt != '\0') {
        CLEAN_AND_EXIT(perror("Option without argument\n"),ARGS_ERR);
    }
    work_queue = buffer_queue_create(buffer_size);
    if (work_queue==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
    }
    printf("%d\n",port_number);
    CLEAN_AND_EXIT( ,0);
}