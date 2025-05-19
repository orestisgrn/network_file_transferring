#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>
#include "utils.h"
#include "buffer_queue.h"
#include "string.h"

int worker_num = 5;
int32_t port_number = -1;

Buffer_Queue work_queue;

int read_config(FILE *config_file);

int bind_socket(int sock,int32_t addr,uint16_t port);

int main(int argc,char **argv) {
    char opt = '\0';
    char *logname = NULL;
    char *config = NULL;
    int buffer_size=0;
    FILE *config_file=NULL;
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
                    worker_num = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Worker number must be int\n"),ARGS_ERR);
                    }
                    if (worker_num < 1) {
                        CLEAN_AND_EXIT(perror("Worker number must be a positive integer\n"),ARGS_ERR);
                    }
                    break;
                case 'p':
                    wrong_char=NULL;
                    port_number = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Port number must be int\n"),ARGS_ERR);
                    }
                    if (port_number < 0 || port_number > 1 << 16) {
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
    int console_sock=socket(AF_INET,SOCK_STREAM,0);
    if (console_sock==-1) {
        CLEAN_AND_EXIT(perror("Socket couldn't be created\n"),SOCK_ERR);
    }
    if (bind_socket(console_sock,htonl(INADDR_ANY),htons(port_number))==-1) {
        if (errno==EADDRINUSE) {
            CLEAN_AND_EXIT(perror("Address already in use\n"),USED_ADDR);
        }
        else {
            CLEAN_AND_EXIT(perror("Couldn't bind socket\n"),BIND_ERR);
        }
    }
    if (listen(console_sock,1)==-1) {
        CLEAN_AND_EXIT(perror("Couldn't initiate listening\n"),LISTEN_ERR);
    }
    if (config!=NULL) {
        if ((config_file=fopen(config,"r"))==NULL) {
            CLEAN_AND_EXIT(perror("Config file couldn't open\n"),FOPEN_ERR);
        }
        int code;/*   read_config reads config and also inserts its records on sync_info_mem_store   */
        if ((code=read_config(config_file))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    accept(console_sock,NULL,NULL);//
    CLEAN_AND_EXIT( ,0);
}

int skip_white(FILE *file) {
    int ch;
    while(isspace(ch=fgetc(file)));
    return ch;
}

int read_dest(FILE *config_file,String path,String addr,int32_t *port);

int read_config(FILE *config_file) {
    String source,target,source_addr,target_addr;
    int32_t source_port,target_port;
    do {
        if ((source=string_create(10))==NULL)
            return ALLOC_ERR;
        if ((source_addr=string_create(15))==NULL) {
            string_free(source);
            return ALLOC_ERR;
        }
        int read_code=read_dest(config_file,source,source_addr,&source_port);
        if (read_code==ALLOC_ERR) {
            string_free(source);
            string_free(source_addr);
            return ALLOC_ERR;
        }
        if (read_code==EOF) {
            string_free(source);
            string_free(source_addr);
            return 0;
        }
        if ((target=string_create(10))==NULL) {
            string_free(source);
            string_free(source_addr);
            return ALLOC_ERR;
        }
        if ((target_addr=string_create(15))==NULL) {
            string_free(target);
            string_free(source);
            string_free(source_addr);
            return ALLOC_ERR;
        }
        read_code=read_dest(config_file,target,target_addr,&target_port);
        if (read_code==ALLOC_ERR) {
            string_free(source);
            string_free(source_addr);
            string_free(target);
            string_free(target_addr);
            return ALLOC_ERR;
        }
        if (read_code==EOF) {
            string_free(source);
            string_free(source_addr);
            string_free(target);
            string_free(target_addr);
            return 0;
        }
        printf("%s@%s:%d -> %s@%s:%d\n",string_ptr(source),string_ptr(source_addr),source_port,
                                        string_ptr(target),string_ptr(target_addr),target_port);//
        string_free(source);//
        string_free(source_addr);//
        string_free(target);//
        string_free(target_addr);//
    } while (1);
}

int read_dest(FILE *config_file,String path,String addr,int32_t *port) {
    int ch;
    ch=skip_white(config_file);
    if (ch==EOF)
        return EOF; // assumes EOF==-1
    while (!(isspace(ch) || ch=='@')) {  // read path
        if (string_push(path,ch)==-1)
            return ALLOC_ERR;
        ch = fgetc(config_file);
        if (ch==EOF)
            return EOF;
    }
    ch = skip_white(config_file);
    if (ch==EOF)
        return EOF;
    while (ch!=':') {                   // read addr
        if (string_push(addr,ch)==-1)
            return ALLOC_ERR;
        ch = fgetc(config_file);
        if (ch==EOF)
            return EOF;
    }
    int scan_code=fscanf(config_file,"%d",port);
    if (scan_code==1)
        return 0;
    if (scan_code==EOF)
        return EOF;
    *port = -1;
    return 0;
}

int bind_socket(int sock,int32_t addr,uint16_t port) {
    struct sockaddr_in str;
    str.sin_family=AF_INET;
    str.sin_addr.s_addr=addr;
    str.sin_port=port;
    return bind(sock,(struct sockaddr *) &str,sizeof(str));
}