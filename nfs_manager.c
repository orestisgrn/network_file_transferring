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
pthread_t *workers;
pthread_t *file_producers;

#define FILE_PRODUCER_NUM ((worker_num+1)/2)

int32_t port_number = -1;

int console_fd;

Buffer_Queue work_queue;
Buffer_Queue producers_queue;

int read_config(FILE *config_file);

int bind_socket(int sock,int32_t addr,uint16_t port);

void *worker_thread(void *args);
void *get_file_list(void *args);

int read_commands(void);
int process_command(String cmd,char *cmd_code);

void terminate_threads(void);

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
                    wrong_char=NULL;    // do we need that set?
                    worker_num = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {    // check errno for overflow
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
                    if (port_number < 0 || port_number >= 1 << 16) {
                        CLEAN_AND_EXIT(perror("Port number must be a 16-bit unsigned integer\n"),return ARGS_ERR);
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
    producers_queue = buffer_queue_create((buffer_size+1)/2);
    if (producers_queue==NULL) {
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
    if ((file_producers=malloc(FILE_PRODUCER_NUM*sizeof(*file_producers)))==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),return ALLOC_ERR);
    }
    for (int i=0;i<FILE_PRODUCER_NUM;i++) {
        if(pthread_create(&file_producers[i],NULL,get_file_list,NULL)!=0) {
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
    console_fd=accept(console_sock,NULL,NULL);
    int retval;
    if (console_fd!=-1) {
        retval = read_commands();
        if (retval==0) {
            time_t t = time(NULL);
            char cur_time_str[30];
            strftime(cur_time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
            printf("[%s] Shutting down manager...\n",cur_time_str);
            dprintf(console_fd,"[%s] Shutting down manager...\n",cur_time_str);
            printf("[%s] Waiting for all active workers to finish.\n",cur_time_str);
            dprintf(console_fd,"[%s] Waiting for all active workers to finish.\n",cur_time_str);
            printf("[%s] Processing remaining queued tasks.\n",cur_time_str);
            dprintf(console_fd,"[%s] Processing remaining queued tasks.\n",cur_time_str);
            terminate_threads();
            t = time(NULL);
            strftime(cur_time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
            printf("[%s] Manager shutdown complete.\n",cur_time_str);
            dprintf(console_fd,"[%s] Manager shutdown complete.\n",cur_time_str);
            close(console_fd);
        }
    }
    else {
        retval = ACCEPT_ERR;
        perror("Error on accept\n");
        terminate_threads();
    }
    CLEAN_AND_EXIT( ,return retval);
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
        struct work_record *rec;
        if ((rec=malloc(sizeof(struct work_record)))==NULL) {
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            return ALLOC_ERR;
        }
        if (string_pos(source_dir,0)!='/' || string_pos(target_dir,0)!='/' || 
            source_port==-1 || target_port==-1 || 
            inet_aton(string_ptr(source_addr_str),&rec->sock_tuple[SOURCE].sin_addr)==0 ||
            inet_aton(string_ptr(target_addr_str),&rec->sock_tuple[TARGET].sin_addr)==0) {
            printf("Skipped %s\n",string_ptr(source_dir));//
            string_free(source_dir);
            string_free(source_addr_str);
            string_free(target_dir);
            string_free(target_addr_str);
            free(rec);
            continue;
        }
        rec->sock_tuple[SOURCE].sin_family=AF_INET;
        rec->sock_tuple[TARGET].sin_family=AF_INET;
        rec->sock_tuple[SOURCE].sin_port=htons(source_port);
        rec->sock_tuple[TARGET].sin_port=htons(target_port);
        rec->source_dir=source_dir;
        rec->target_dir=target_dir;
        rec->file=NULL;
        /*                              */
        printf("%s@%s:%d -> %s@%s:%d\n",string_ptr(source_dir),string_ptr(source_addr_str),source_port,
                                        string_ptr(target_dir),string_ptr(target_addr_str),target_port);//
        buffer_queue_push(producers_queue,rec);
        string_free(source_addr_str);
        string_free(target_addr_str);
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
    int scan_code=fscanf(config_file,"%d",port);// maybe do it safely with strtol (need to store to string)
    if (scan_code==1)
        return 0;
    if (scan_code==EOF)
        return EOF;
    *port = -1;
    return 0;
}

int read_commands(void) {
    char ch;
    while(1) {
        String cmd = string_create(15);
        if (cmd==NULL) {
            perror("Memory allocation error\n");
            return ALLOC_ERR;
        }
        while(1) {
            if (read(console_fd,&ch,sizeof(ch))<1) {
                string_free(cmd);
                perror("Console read error\n");
                return CONSOLE_ERR;
            }
            if (string_push(cmd,ch)==-1) {
                string_free(cmd);
                perror("Memory allocation error\n");
                return ALLOC_ERR;
            }
            if (ch=='\n')
                break;
        }
        char cmd_code;
        int err_code;
        if ((err_code=process_command(cmd,&cmd_code))!=0) {
            fprintf(stderr,"Error while executing command: %s\n",string_ptr(cmd));
            string_free(cmd);
            return err_code;
        }
        string_free(cmd);
        if (cmd_code==SHUTDOWN)
            return 0;
    }
}

int bind_socket(int sock,int32_t addr,uint16_t port) {
    struct sockaddr_in str;
    str.sin_family=AF_INET;
    str.sin_addr.s_addr=addr;
    str.sin_port=port;
    return bind(sock,(struct sockaddr *) &str,sizeof(str));
}

void *get_file_list(void *args) {
    enum {SOURCE,TARGET};
    struct work_record *rec;
    while ((rec=buffer_queue_pop(producers_queue))!=NULL) {
        int sockfd=socket(AF_INET,SOCK_STREAM,0);
        if (sockfd==-1) {
            perror("Socket couldn't be created\n");//
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            free(rec);
            return NULL;    // think about how to handle errors
        }       // think about using non-blocking connect
        if (connect(sockfd,(struct sockaddr *) &rec->sock_tuple[SOURCE],sizeof(rec->sock_tuple[SOURCE]))==-1) {
            perror("Connection couldn't be made\n");//
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            free(rec);
            continue;
        }
        char list[] = "LIST ";
        write(sockfd,list,sizeof(list)-1);
        write(sockfd,string_ptr(rec->source_dir),string_length(rec->source_dir));
        write(sockfd,"\n",1);
        String file;
        while(1) {
            file=string_create(10);
            if (file==NULL) {
                perror("Memory allocation error\n");
                string_free(rec->source_dir);
                string_free(rec->target_dir);
                free(rec);
                return NULL;
            }
            while(1) {
                char ch;
                if (read(sockfd,&ch,1)<1) {
                    string_free(rec->source_dir);
                    string_free(rec->target_dir);
                    string_free(file);
                    free(rec);
                    return NULL;
                }
                if (ch=='\n')
                    break;
                if (string_push(file,ch)==-1) {
                    perror("Memory allocation error\n");
                    string_free(rec->source_dir);
                    string_free(rec->target_dir);
                    string_free(file);
                    free(rec);
                    return NULL;
                }
            }
            if (strcmp(string_ptr(file),".")==0) {
                string_free(file);
                break;
            }
            struct work_record *file_rec;
            if ((file_rec=malloc(sizeof(struct work_record)))==NULL) {
                perror("Memory allocation error\n");
                string_free(rec->source_dir);
                string_free(rec->target_dir);
                string_free(file);
                free(rec);
                return NULL;
            }
            file_rec->sock_tuple[SOURCE]=rec->sock_tuple[SOURCE];   // maybe also make it reference
            file_rec->sock_tuple[TARGET]=rec->sock_tuple[TARGET];
            file_rec->source_dir=string_create(10);
            file_rec->target_dir=string_create(10);
            file_rec->file=file;
            if (file_rec->source_dir==NULL || file_rec->target_dir==NULL ||
                string_cpy(file_rec->source_dir,string_ptr(rec->source_dir))==-1 ||
                string_cpy(file_rec->target_dir,string_ptr(rec->target_dir))==-1 ) {
                perror("Memory allocation error\n");
                string_free(file_rec->source_dir);
                string_free(file_rec->target_dir);
                string_free(rec->source_dir);
                string_free(rec->target_dir);
                string_free(file);
                free(rec);
                return NULL;
            }
            buffer_queue_push(work_queue,file_rec);
        }
        string_free(rec->source_dir);
        string_free(rec->target_dir);
        close(sockfd);
        free(rec);//
    }
    return NULL;
}

void *worker_thread(void *args) {
    enum {SOURCE,TARGET};
    struct work_record *rec;
    while ((rec=buffer_queue_pop(work_queue))!=NULL) {
        int sourcefd=socket(AF_INET,SOCK_STREAM,0);
        if (sourcefd==-1) {
            perror("Socket couldn't be created\n");// use strerror
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            string_free(rec->file);
            free(rec);
            return NULL;    // think about how to handle errors
        }       // think about using non-blocking connect
        if (connect(sourcefd,(struct sockaddr *) &rec->sock_tuple[SOURCE],sizeof(rec->sock_tuple[SOURCE]))==-1) {
            perror("Connection couldn't be made\n");//
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            string_free(rec->file);
            free(rec);
            close(sourcefd);
            continue;
        }
        int targetfd=socket(AF_INET,SOCK_STREAM,0);
        if (targetfd==-1) {
            perror("Socket couldn't be created\n");//
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            string_free(rec->file);
            free(rec);
            close(sourcefd);
            return NULL;
        }
        if (connect(targetfd,(struct sockaddr *) &rec->sock_tuple[TARGET],sizeof(rec->sock_tuple[TARGET]))==-1) {
            perror("Connection couldn't be made\n");//
            string_free(rec->source_dir);
            string_free(rec->target_dir);
            string_free(rec->file);
            free(rec);
            close(sourcefd);
            close(targetfd);
            continue;
        }
        char pull[]="PULL ";    // skip strerror for now
        write(sourcefd,pull,sizeof(pull)-1);
        write(sourcefd,string_ptr(rec->source_dir),string_length(rec->source_dir));
        write(sourcefd,"/",1);
        write(sourcefd,string_ptr(rec->file),string_length(rec->file));
        write(sourcefd,"\n",1);
        char push[]="PUSH ";
        int nread;
        char nread_str[20] = "-1";
        char ch,first;
        int code,error;
        if ((code=read(sourcefd,&ch,sizeof(char)))==-1) {
            error=errno;
            printf("error -1 %d\n",error);
            // strerror needs synchro
        }
        else if (code==0) {
            error=errno;
            printf("error 0 %d\n",error);
            // strerror needs synchro
        }
        else {
            first=ch;
            while(ch!=' ') {
                code=read(sourcefd,&ch,sizeof(char));   // check here
            }
            if (first=='-') {
                printf("Error on %s\n",string_ptr(rec->source_dir));//
            }
            else {
                char buffer[BUFFSIZE];
                while(1) {
                    write(targetfd,push,sizeof(pull)-1);
                    write(targetfd,string_ptr(rec->target_dir),string_length(rec->target_dir));
                    write(targetfd,"/",1);
                    write(targetfd,string_ptr(rec->file),string_length(rec->file));
                    write(targetfd," ",1);
                    write(targetfd,nread_str,strlen(nread_str));
                    write(targetfd," ",1);
                    nread=read(sourcefd,buffer,sizeof(buffer)); // check here
                    write(targetfd,buffer,nread);
                    write(targetfd,"\n",1);
                    break;//
                }
            }
        }
        string_free(rec->source_dir);
        string_free(rec->target_dir);
        string_free(rec->file);
        free(rec);
        close(sourcefd);
        close(targetfd);
    }
    return NULL;
}

void terminate_threads(void) {
    for (int i=0;i<FILE_PRODUCER_NUM;i++) {
        buffer_queue_push(producers_queue,NULL); // poison pill
    }
    for (int i=0;i<FILE_PRODUCER_NUM;i++) {
        pthread_join(file_producers[i],NULL);
    }
    for (int i=0;i<worker_num;i++) {
        buffer_queue_push(work_queue,NULL); // poison pill
    }
    for (int i=0;i<worker_num;i++) {
        pthread_join(workers[i],NULL);
    }
}

int separate_destination_args(String argv,String *path,String *ip_addr,String *port_str);
int handle_cmd(String argv);

int process_command(String cmd,char *cmd_code) {           // Command ends in \n
    *cmd_code = NO_COMMAND;
    const char *cmd_ptr = string_ptr(cmd);
    String argv = string_create(15);
    String source_argv;
    struct work_record *rec;
    int argc=0;
    if (argv==NULL) {
        return ALLOC_ERR;
    }
    /*   Main loop: iterate the command character by character, while skipping spaces   */
    while (1) {
        while (!isspace(*cmd_ptr)) {    // arguments are fetched here
            if (string_push(argv,*cmd_ptr)==-1) {
                string_free(argv);
                return ALLOC_ERR;
            }
            cmd_ptr++;
        }
        if (string_length(argv)!=0) {   // If argument is found, act depending on the arg num (argc)
            if (argc==0) {
                *cmd_code=handle_cmd(argv);
                write(console_fd,cmd_code,sizeof(*cmd_code));
                if (*cmd_code==SHUTDOWN) {
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    dprintf(console_fd,"[%s] Command shutdown\n",time_str);
                    string_free(argv);
                    return 0;
                }
                if (*cmd_code==NO_COMMAND) {
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Invalid command: %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"[%s] Invalid command: %s\n",time_str,string_ptr(argv));
                    string_free(argv);
                    return 0;
                }
                argc++;
            }
            else if (argc==1) {
                String path;
                String ip_addr;
                String port_str;
                int code=separate_destination_args(argv,&path,&ip_addr,&port_str);
                if (code==ALLOC_ERR) {
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    return ALLOC_ERR;
                }
                if (code==EOF) {
                    *cmd_code=INVALID_SOURCE;
                    write(console_fd,cmd_code,sizeof(*cmd_code));
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Invalid source: %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"[%s] Invalid source: %s\n",time_str,string_ptr(argv));
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    return 0;
                }
                enum {SOURCE,TARGET};
                if ((rec=malloc(sizeof(struct work_record)))==NULL) {
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    return ALLOC_ERR;
                }
                char *wrong_char=NULL;
                long port = strtol(string_ptr(port_str),&wrong_char,10);
                int strtol_err=errno;
                if (string_pos(path,0)!='/' || 
                    inet_aton(string_ptr(ip_addr),&rec->sock_tuple[SOURCE].sin_addr)==0 ||
                    *wrong_char!='\0' || strtol_err!=0 || port < 0 || port >= 1 << 16) {
                    *cmd_code=INVALID_SOURCE;
                    write(console_fd,cmd_code,sizeof(*cmd_code));
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Invalid source: %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"[%s] Invalid source: %s\n",time_str,string_ptr(argv));
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    free(rec);
                    return 0;
                }
                rec->sock_tuple[SOURCE].sin_family=AF_INET;
                rec->sock_tuple[SOURCE].sin_port=htons(port);
                rec->source_dir=path;
                rec->file=NULL;
                string_free(ip_addr);
                string_free(port_str);
                if (*cmd_code==CANCEL) {
                    write(console_fd,cmd_code,sizeof(*cmd_code));
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    dprintf(console_fd,"[%s] Command cancel %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"Got that cancel\n");//
                    string_free(path);//
                    string_free(argv);
                    free(rec);
                    return 0;
                }
                source_argv=argv;
                argc++;
            }
            else if (argc==2) {
                String path;
                String ip_addr;
                String port_str;
                int code=separate_destination_args(argv,&path,&ip_addr,&port_str);
                if (code==ALLOC_ERR) {
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    string_free(source_argv);
                    string_free(rec->source_dir);
                    free(rec);
                    return ALLOC_ERR;
                }
                if (code==EOF) {
                    *cmd_code=INVALID_TARGET;
                    write(console_fd,cmd_code,sizeof(*cmd_code));
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Invalid target: %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"[%s] Invalid target: %s\n",time_str,string_ptr(argv));
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    string_free(source_argv);
                    string_free(rec->source_dir);
                    free(rec);
                    return 0;
                }
                enum {SOURCE,TARGET};
                char *wrong_char=NULL;
                long port = strtol(string_ptr(port_str),&wrong_char,10);
                int strtol_err=errno;
                if (string_pos(path,0)!='/' || 
                    inet_aton(string_ptr(ip_addr),&rec->sock_tuple[TARGET].sin_addr)==0 ||
                    *wrong_char!='\0' || strtol_err!=0 || port < 0 || port >= 1 << 16) {
                    *cmd_code=INVALID_TARGET;
                    write(console_fd,cmd_code,sizeof(*cmd_code));
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Invalid target: %s\n",time_str,string_ptr(argv));
                    dprintf(console_fd,"[%s] Invalid target: %s\n",time_str,string_ptr(argv));
                    string_free(path);
                    string_free(ip_addr);
                    string_free(port_str);
                    string_free(argv);
                    string_free(source_argv);
                    string_free(rec->source_dir);
                    free(rec);
                    return 0;
                }
                rec->sock_tuple[TARGET].sin_family=AF_INET;
                rec->sock_tuple[TARGET].sin_port=htons(port);
                rec->target_dir=path;
                string_free(ip_addr);
                string_free(port_str);
                write(console_fd,cmd_code,sizeof(*cmd_code));
                char time_str[30];
                time_t t = time(NULL);
                strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                dprintf(console_fd,"[%s] Command add %s -> %s\n",time_str,string_ptr(source_argv),string_ptr(argv));
                dprintf(console_fd,"Got that add\n");//
                string_free(rec->source_dir);
                string_free(rec->target_dir);
                string_free(argv);
                string_free(source_argv);
                free(rec);
                return 0;
            }
            if (argc!=2)
                string_free(argv);
            argv = string_create(15);
            if (argv==NULL) {
                return ALLOC_ERR;
            }
        }
        if (*(++cmd_ptr)=='\0')
            break;
    }
    string_free(argv);
    if (argc==1) {
        *cmd_code = INVALID_SOURCE;
        write(console_fd,cmd_code,sizeof(*cmd_code));
        char time_str[30];
        time_t t = time(NULL);
        strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
        dprintf(console_fd,"[%s] Source directory not given.\n",time_str);
    }
    if (argc==2) {
        *cmd_code = INVALID_TARGET;
        write(console_fd,cmd_code,sizeof(*cmd_code));
        char time_str[30];
        time_t t = time(NULL);
        strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
        dprintf(console_fd,"[%s] Target directory not given.\n",time_str);
        string_free(rec->source_dir);
        free(rec);
    }
    return 0;
}

int handle_cmd(String argv) {
    if (!strcmp(string_ptr(argv),"shutdown")) {
        return SHUTDOWN;
    }
    else if (!strcmp(string_ptr(argv),"add")) {
        return ADD;
    }
    else if (!strcmp(string_ptr(argv),"cancel")) {
        return CANCEL;
    }
    else {
        return NO_COMMAND;
    }
}

int separate_destination_args(String argv,String *path,String *ip_addr,String *port_str) {
    const char *argv_ptr=string_ptr(argv);
    *path=string_create(10);
    *ip_addr=string_create(15);
    *port_str=string_create(6);
    if (*path==NULL || *ip_addr==NULL || *port_str==NULL)
        return ALLOC_ERR;
    while (*argv_ptr!='@') {
        if (string_push(*path,*argv_ptr)==-1)
            return ALLOC_ERR;
        argv_ptr++;
        if (*argv_ptr=='\0')
            return EOF;
    }
    argv_ptr++;
    if (*argv_ptr=='\0')
        return EOF;
    while (*argv_ptr!=':') {
        if (string_push(*ip_addr,*argv_ptr)==-1)
            return ALLOC_ERR;
        argv_ptr++;
        if (*argv_ptr=='\0')
            return EOF;
    }
    argv_ptr++;
    if (*argv_ptr=='\0')
        return EOF;
    while (*argv_ptr!='\0') {
        if (string_push(*port_str,*argv_ptr)==-1)
            return ALLOC_ERR;
        argv_ptr++;
    }
    return 0;
}