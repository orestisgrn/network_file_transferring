#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

FILE *log_file;

int main(int argc, char **argv) {
    char opt='\0';
    char *logname=NULL;
    char *ip_str=NULL;
    int32_t port_number = -1;
    while (*(++argv) != NULL) {
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            char *wrong_char;
            switch (opt) {
                case 'l':
                    logname = *argv;
                    break;
                case 'h':
                    ip_str = *argv;
                    break;
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
    if (logname==NULL) {
        perror("No logfile given\n");
        return ARGS_ERR;
    }
    if ((log_file=fopen(logname,"a"))==NULL) {
        perror("Logfile couldn't open\n");
        return FOPEN_ERR;
    }
    if (ip_str==NULL) {
        perror("No host IP given\n");
        fclose(log_file);
        return ARGS_ERR;
    }
    if (port_number==-1) {
        perror("No host port given\n");
        fclose(log_file);
        return ARGS_ERR;
    }
    if (opt != '\0') {
        perror("Option without argument\n");
        fclose(log_file);
        return ARGS_ERR;
    }
    struct sockaddr_in sock_str;
    sock_str.sin_family = AF_INET;
    sock_str.sin_port = htons(port_number);
    if (inet_aton(ip_str,&sock_str.sin_addr)==0) {
        perror("Invalid IP given\n");
        fclose(log_file);
        return INVALID_IP;
    }
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd==-1) {
        perror("Socket couldn't be created\n");
        fclose(log_file);
        return SOCK_ERR;
    }
    /*   Main console loop   */
    fclose(log_file);
    return 0;
}