#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"
#include "buffer_queue.h"
#include "string.h"

int worker_num = 5;
int running_workers=0;  // maybe running threads? (for all threads)
pthread_t *workers;

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

int retval;

int32_t port_number = -1;

Buffer_Queue work_queue;

int read_config(FILE *config_file);

int bind_socket(int sock,int32_t addr,uint16_t port);

void *worker_thread(void *args);

void *get_file_list(void *void_args);
struct get_file_list_args {
    struct sockaddr_in sock_tuple[2];
    String source_dir;
    String target_dir;
};


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
                        CLEAN_AND_EXIT(perror("Worker number must be int\n"),return ARGS_ERR);
                    }
                    if (worker_num < 1) {
                        CLEAN_AND_EXIT(perror("Worker number must be a positive integer\n"),return ARGS_ERR);
                    }
                    break;
                case 'p':
                    wrong_char=NULL;
                    port_number = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Port number must be int\n"),return ARGS_ERR);
                    }
                    if (port_number < 0 || port_number > 1 << 16) {
                        CLEAN_AND_EXIT(perror("Port number must be a 16-bit integer\n"),return ARGS_ERR);
                    }
                    break;
                case 'b':
                    wrong_char=NULL;
                    buffer_size = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Port number must be int\n"),return ARGS_ERR);
                    }
                    if (buffer_size < 1) {
                        CLEAN_AND_EXIT(perror("Buffer size must be a positive integer\n"),return ARGS_ERR);
                    }
                    break;
                case '\0':
                    CLEAN_AND_EXIT(perror("Argument without option\n"),return ARGS_ERR);
                default:
                    CLEAN_AND_EXIT(fprintf(stderr,"-%c is not an option\n",opt),return ARGS_ERR);
            }
            opt = 0;
        }
    }
    if (logname==NULL) {
        CLEAN_AND_EXIT(perror("No logfile given\n"),return ARGS_ERR);
    }
    if (buffer_size==0) {
        CLEAN_AND_EXIT(perror("No buffer size given\n"),return ARGS_ERR);
    }
    if (port_number==-1) {
        CLEAN_AND_EXIT(perror("No port number given\n"),return ARGS_ERR);
    }
    if (opt != '\0') {
        CLEAN_AND_EXIT(perror("Option without argument\n"),return ARGS_ERR);
    }
    work_queue = buffer_queue_create(buffer_size);
    if (work_queue==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),return ALLOC_ERR);
    }
    int console_sock=socket(AF_INET,SOCK_STREAM,0);
    if (console_sock==-1) {
        CLEAN_AND_EXIT(perror("Socket couldn't be created\n"),return SOCK_ERR);
    }
    if (bind_socket(console_sock,htonl(INADDR_ANY),htons(port_number))==-1) {
        if (errno==EADDRINUSE) {
            CLEAN_AND_EXIT(perror("Address already in use\n"),return USED_ADDR);
        }
        else {
            CLEAN_AND_EXIT(perror("Couldn't bind socket\n"),return BIND_ERR);
        }
    }
    if (listen(console_sock,1)==-1) {
        CLEAN_AND_EXIT(perror("Couldn't initiate listening\n"),return LISTEN_ERR);
    }
    if ((workers=malloc(worker_num*sizeof(*workers)))==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),return ALLOC_ERR);
    }
    for (int i=0;i<worker_num;i++) {
        if(pthread_create(&workers[i],NULL,worker_thread,NULL)!=0) {
            CLEAN_AND_EXIT(perror("Thread couldn't be created\n"),return PTHREAD_ERR);
        }
    }
    if (config!=NULL) {
        if ((config_file=fopen(config,"r"))==NULL) {
            CLEAN_AND_EXIT(perror("Config file couldn't open\n"),return FOPEN_ERR);
        }
        int code;/*   read_config reads config and also inserts its records on sync_info_mem_store   */
        if ((code=read_config(config_file))!=0) {   // maybe execute this in a thread
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),return code);
        }
    }
    accept(console_sock,NULL,NULL);//
    for (int i=0;i<worker_num;i++) {    // See how to handle pool later...
        pthread_join(workers[i],NULL);
    }
    CLEAN_AND_EXIT( ,{retval=0;pthread_exit(&retval);});
}

int skip_white(FILE *file) {
    int ch;
    while(isspace(ch=fgetc(file)));
    return ch;
}

int read_dest(FILE *config_file,String path,String addr,int32_t *port);

int read_config(FILE *config_file) {
    String source_dir,target_dir,source_addr_str,target_addr_str;
    int32_t source_port,target_port;
    do {
        if ((source_dir=string_create(10))==NULL)
            return ALLOC_ERR;
        if ((source_addr_str=string_create(15))==NULL) {
            string_free(source_dir);
            return ALLOC_ERR;
        }
        int read_code=read_dest(config_file,source_dir,source_addr_str,&source_port);
        if (read_code==ALLOC_ERR) {
            string_free(source_dir);
            string_free(source_addr_str);
            return ALLOC_ERR;
        }
        if (read_code==EOF) {
            string_free(source_dir);
            string_free(source_addr_str);
            return 0;
        }
        if ((target_dir=string_create(10))==NULL) {
            string_free(source_dir);
            string_free(source_addr_str);
            return ALLOC_ERR;
        }
        if ((target_addr_str=string_create(15))==NULL) {
            string_free(target_dir);
            string_free(source_dir);
            string_free(source_addr_str);
            return ALLOC_ERR;
        }
        read_code=read_dest(config_file,target_dir,target_addr_str,&target_port);
        if (read_code==ALLOC_ERR) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            return ALLOC_ERR;
        }
        if (read_code==EOF) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            return 0;
        }
        /* Checks validity of records */
        enum {SOURCE,TARGET};
        struct get_file_list_args *args;
        if ((args=malloc(sizeof(struct get_file_list_args)))==NULL) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            return ALLOC_ERR;
        }
        if (string_pos(source_dir,0)!='/' || string_pos(target_dir,0)!='/' || 
            source_port==-1 || target_port==-1 || 
            inet_aton(string_ptr(source_addr_str),&args->sock_tuple[SOURCE].sin_addr)==0 ||
            inet_aton(string_ptr(target_addr_str),&args->sock_tuple[TARGET].sin_addr)==0) {
            printf("Skipped %s\n",string_ptr(source_dir));//
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            free(args);
            continue;
        }
        args->sock_tuple[SOURCE].sin_family=AF_INET;
        args->sock_tuple[TARGET].sin_family=AF_INET;
        args->sock_tuple[SOURCE].sin_port=htons(source_port);
        args->sock_tuple[TARGET].sin_port=htons(target_port);
        args->source_dir=source_dir;
        /*                              */
        printf("%s@%s:%d -> %s@%s:%d\n",string_ptr(source_dir),string_ptr(source_addr_str),source_port,
                                        string_ptr(target_dir),string_ptr(target_addr_str),target_port);//
        pthread_t thr;
        if (pthread_create(&thr,NULL,get_file_list,args)!=0) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            free(args);
            return PTHREAD_ERR;
        }
        if (pthread_detach(thr)!=0) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            pthread_join(thr,NULL);
            return PTHREAD_ERR;
        }
        //string_free(source_dir);  // freed by get_file_list thread (for now)
        string_free(source_addr_str);//
        string_free(target_dir);
        string_free(target_addr_str);//
    } while (1);
}

int read_dest(FILE *config_file,String path,String addr,int32_t *port) {
    int ch;
    ch=skip_white(config_file);
    if (ch==EOF)
        return EOF; // assumes EOF is negative
    while (ch!='@') {                   // read path
        if (string_push(path,ch)==-1)
            return ALLOC_ERR;
        ch = skip_white(config_file);
        if (ch==EOF)
            return EOF;
    }
    ch = skip_white(config_file);
    if (ch==EOF)
        return EOF;
    while (ch!=':') {                   // read addr
        if (string_push(addr,ch)==-1)
            return ALLOC_ERR;
        ch = skip_white(config_file);
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

void *worker_thread(void *args) {
    return NULL;
}

void *get_file_list(void *void_args) {
    enum {SOURCE,TARGET};
    struct get_file_list_args *args=void_args;
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    if (sockfd==-1) {
        perror("Socket couldn't be created\n");//
        string_free(args->source_dir);
        free(args);
        return NULL;
    }       // think about using non-blocking connect
    if (connect(sockfd,(struct sockaddr *) &args->sock_tuple[SOURCE],sizeof(args->sock_tuple[SOURCE]))==-1) {
        perror("Connection couldn't be made\n");//
        string_free(args->source_dir);
        free(args);
        return NULL;
    }
    char list[] = "LIST ";
    write(sockfd,list,sizeof(list)-1);
    write(sockfd,string_ptr(args->source_dir),string_length(args->source_dir));
    write(sockfd,"\n",1);
    String file;
    while(1) {
        file=string_create(10);
        if (file==NULL) {
            perror("Memory allocation error\n");
            string_free(args->source_dir);
            free(args);
            return NULL;
        }
        while(1) {
            char ch;
            if (read(sockfd,&ch,1)<1) {         // read doesnt timeout
                string_free(args->source_dir);
                string_free(file);
                free(args);
                return NULL;
            }
            if (ch=='\n')
                break;
            if (string_push(file,ch)==-1) {
                perror("Memory allocation error\n");
                string_free(args->source_dir);
                string_free(file);
                free(args);
                return NULL;
            }
        }
        if (strcmp(string_ptr(file),".")==0) {
            string_free(file);
            break;
        }
        printf("%s\n",string_ptr(file)); // no synchro
        string_free(file);
    }
    string_free(args->source_dir);
    close(sockfd);
    free(args);
    return NULL;
}
