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
#include "utils.h"
#include "string.h"

int retval;

int bind_socket(int sock,int32_t addr,uint16_t port);
void *connection_thread(void *void_fd);

int main(int argc, char **argv) {
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

void *connection_thread(void *void_fd) {
    int *fd = void_fd;
    char ch;
    String msg = string_create(15);
    if (msg==NULL) {
        perror("Memory allocation error\n");// no synchro
        free(fd);
        return NULL;   // Think about how to handle in-thread errors
    }
    while(read(*fd,&ch,sizeof(ch))>0) {
        if (string_push(msg,ch)==-1) {
            perror("Memory allocation error\n");
            string_free(msg);
            free(fd);
            return NULL;
        }
    }
    if (string_pos(msg,5)!='/') {   // maybe too much checking...
        string_free(msg);
        free(fd);
        return NULL;
    }
    const char *path=string_ptr(msg)+6;
    if (strncmp(string_ptr(msg),"LIST",4)==0) {
        DIR *dir_ptr=opendir(path);         // iterate through files
        printf("%s\n",path);
        if (dir_ptr!=NULL) {
            struct dirent *direntp;
            int dir_fd = dirfd(dir_ptr);
            while ((direntp=readdir(dir_ptr))!=NULL) {
                struct stat st;
                fstatat(dir_fd,direntp->d_name,&st,0);
                if (S_ISREG(st.st_mode))
                    printf("%s\n",direntp->d_name);
            }
        }
        printf(".\n");
        closedir(dir_ptr);
    }
    else if (strncmp(string_ptr(msg),"PUSH",4)==0) {
        printf("%s\n",string_ptr(msg)); // no synchro
    }
    else if (strncmp(string_ptr(msg),"PULL",4)==0) {
        printf("%s\n",string_ptr(msg)); // no synchro
    }
    else
        printf("Invalid command\n"); // no synchro
    string_free(msg);
    free(fd);
    return NULL;
}