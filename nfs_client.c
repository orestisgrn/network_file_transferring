#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include "utils.h"
#include "string.h"

int retval;

int bind_socket(int sock,int32_t addr,uint16_t port);
void *connection_thread(void *void_fd);

int main(int argc, char **argv) {
    signal(SIGPIPE,SIG_IGN);
    char opt='\0';
    int32_t port_number=-1;
    while (*(++argv) != NULL) {                 // Command line arguments handle
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            char *wrong_char;
            switch (opt) {
                case 'p':
                    wrong_char=NULL;
                    port_number = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        perror("Port number must be int\n");
                        return ARGS_ERR;
                    }
                    if (port_number < 0 || port_number > 1 << 16) {
                        perror("Port number must be a 16-bit integer\n");
                        return ARGS_ERR;
                    }
                    break;
                case '\0':
                    perror("Argument without option\n");
                    return ARGS_ERR;
                default:
                    fprintf(stderr,"-%c is not an option\n",opt);
                    return ARGS_ERR;
            }
            opt = 0;
        }
    }
    if (port_number==-1) {
        perror("No port number given\n");
        return ARGS_ERR;
    }
    int client_sock=socket(AF_INET,SOCK_STREAM,0);
    if (client_sock==-1) {
        perror("Socket couldn't be created\n");
        return SOCK_ERR;
    }
    if (bind_socket(client_sock,htonl(INADDR_ANY),htons(port_number))==-1) {
        if (errno==EADDRINUSE) {
            perror("Address already in use\n");
            return USED_ADDR;
        }
        else {
            perror("Couldn't bind socket\n");
            return BIND_ERR;
        }
    }
    if (listen(client_sock,SOMAXCONN)==-1) {
        perror("Couldn't initiate listening\n");
        return LISTEN_ERR;
    }
    int *fd;
    struct sockaddr_in manager;//
    socklen_t managerlen=sizeof(manager);//
    while (1) {
        if ((fd=malloc(sizeof(int)))==NULL) {
            perror("Memory allocation error\n");    // Think about printing this...
            retval=ALLOC_ERR;
            pthread_exit(&retval);
        }
        if ((*fd=accept(client_sock,(struct sockaddr *) &manager,&managerlen))==-1) {
            perror("Error on accept\n");
            retval=ACCEPT_ERR;
            pthread_exit(&retval);
        }
        printf("%s %d\n",inet_ntoa(manager.sin_addr),ntohs(manager.sin_port));// no synchro
        pthread_t thr;
        if (pthread_create(&thr,NULL,connection_thread,fd)!=0) {
            perror("Thread couldn't be created\n");
            retval=PTHREAD_ERR;
            pthread_exit(&retval);
        }
        printf("Created thread\n");// no synchro
        if (pthread_detach(thr)!=0) {
            perror("Thread couldn't be detached\n");
            retval=PTHREAD_ERR;
            pthread_exit(&retval);
        }
    }
}

int bind_socket(int sock,int32_t addr,uint16_t port) {
    struct sockaddr_in str;
    str.sin_family=AF_INET;
    str.sin_addr.s_addr=addr;
    str.sin_port=port;
    return bind(sock,(struct sockaddr *) &str,sizeof(str));
}

void list(const char *path,int fd);
void pull(const char *path,int fd);
void push(const char *path,int fd);

void *connection_thread(void *void_fd) {
    int *fd = void_fd;
    char ch;
    String msg = string_create(15);
    if (msg==NULL) {
        perror("Memory allocation error\n");// no synchro
        free(fd);
        return NULL;   // Think about how to handle in-thread errors
    }
    while(1) {
        if (read(*fd,&ch,sizeof(ch))<1) {
            string_free(msg);
            close(*fd);
            free(fd);
            return NULL;
        }
        if (ch=='\n')
            break;
        if (string_push(msg,ch)==-1) {
            perror("Memory allocation error\n");
            string_free(msg);
            close(*fd);
            free(fd);
            return NULL;
        }
    }
    if ((string_length(msg)<=5) || (string_pos(msg,5)!='/')) {   // maybe too much checking...
        string_free(msg);
        close(*fd);
        free(fd);
        return NULL;
    }
    const char *path=string_ptr(msg)+6;
    if (strncmp(string_ptr(msg),"LIST",4)==0) {
        list(path,*fd);
    }
    else if (strncmp(string_ptr(msg),"PUSH",4)==0) {
        push(path,*fd);
    }
    else if (strncmp(string_ptr(msg),"PULL",4)==0) {
        pull(path,*fd);
    }
    else
        printf("Invalid command\n"); // no synchro
    string_free(msg);
    close(*fd);
    free(fd);
    return NULL;
}

void list(const char *path,int fd) {
    DIR *dir_ptr=opendir(path);         // iterate through files
    printf("%s\n",path);
    if (dir_ptr!=NULL) {
        struct dirent *direntp;
        int dir_fd = dirfd(dir_ptr);
        while ((direntp=readdir(dir_ptr))!=NULL) {  // readdir -> not thread-safe
            struct stat st;             // doesn't check if file is accessible
            fstatat(dir_fd,direntp->d_name,&st,0);
            if (S_ISREG(st.st_mode)) {
                write(fd,direntp->d_name,strlen(direntp->d_name));
                write(fd,"\n",1);
            }
        }
    }
    write(fd,".\n",2);  // warning: has to send \n
    closedir(dir_ptr);
}

void pull(const char *path,int fd) {
    int file,error;
    static char minus_one[]="-1 ";
    if ((file=open(path,O_RDONLY))==-1) {
        error=errno;
        write(fd,minus_one,sizeof(minus_one)-1);
        write(fd,strerror(error),strlen(strerror(error)));  // strerror -> not thread-safe
        return;
    }
    struct stat in_stat;
    if (fstat(file,&in_stat)==-1) {
        error=errno;
        write(fd,minus_one,sizeof(minus_one)-1);
        write(fd,strerror(error),strlen(strerror(error)));  // strerror -> not thread-safe
        return;
    }
    if ((in_stat.st_mode & S_IFMT) != S_IFREG) {
        error=errno;
        write(fd,minus_one,sizeof(minus_one)-1);
        write(fd,strerror(error),strlen(strerror(error)));  // strerror -> not thread-safe
        return;
    }
    off_t filesize=in_stat.st_size;
    char filesize_str[20];          // beware of the max off_t length
    sprintf(filesize_str,"%ld ",filesize);
    write(fd,filesize_str,strlen(filesize_str));
    int nread;
    char buffer[BUFFSIZE];
    for (int bytes_sent=0;(nread=read(file,buffer,BUFFSIZE))>0;bytes_sent=0) {
        while (bytes_sent<nread) {
            int nwrite;
            if ((nwrite=write(fd,buffer,nread))<nread) {
                error=errno;//
                printf("write error %d %d\n",error,nwrite);//
                if (nwrite==-1) {//
                    close(file);
                    return;
                }
            }
            //printf("%d\n",nread);
            bytes_sent+=nwrite;
        }
    }
    close(file);
}

void push(const char *path,int fd) {
    int file;
    char ch;
    do {
        if (read(fd,&ch,1)<1) // reads -1'\n'
            return;
    } while(ch!='\n');
    if ((file=open(path,O_WRONLY | O_CREAT | O_TRUNC,0644))==-1)
        return;
    int msg_size;
    do {
        do {
            if (read(fd,&ch,1)<1) // reads PUSH message
                return;
        } while(ch!='\n');
        String msg_size_str = string_create(5);
        if (msg_size_str==NULL) {
            perror("Memory allocation error\n");// no synchro
            return;
        }
        while(1) {
            if (read(fd,&ch,sizeof(ch))<1) {
                string_free(msg_size_str);
                return;
            }
            if (isspace(ch))
                break;
            if (string_push(msg_size_str,ch)==-1) {
                perror("Memory allocation error\n");
                string_free(msg_size_str);
                return;
            }
        }
        sscanf(string_ptr(msg_size_str),"%d",&msg_size);
        string_free(msg_size_str);
        char *buffer=malloc(msg_size*sizeof(char));
        if (buffer==NULL) {
            perror("Memory allocation error\n");
            return;
        }
        char *buff_ptr=buffer;
        int bytes_read=0;
        while(1) {
            int nread=read(fd,buff_ptr,msg_size-bytes_read);
            if (nread==-1)
                break;
            bytes_read+=nread;
            buff_ptr+=nread;
            if (nread==0 || bytes_read==msg_size)
                break;
        }
        write(file,buffer,msg_size);
        free(buffer);
    } while(msg_size!=0);
}