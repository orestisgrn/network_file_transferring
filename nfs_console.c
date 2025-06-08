#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "string.h"
#include "utils.h"

int sockfd;

FILE *log_file;

void read_response(void);
void read_shutdown(void);

int main(int argc, char **argv) {
    char opt='\0';
    char *logname=NULL;
    char *ip_str=NULL;
    int32_t port_number = -1;
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
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd==-1) {
        perror("Socket couldn't be created\n");
        fclose(log_file);
        return SOCK_ERR;
    }
    if (connect(sockfd,(struct sockaddr *) &sock_str,sizeof(sock_str))==-1) {
        printf("Connection couldn't be made\n");
        fclose(log_file);
        return 0;
    }
    /*   Main console loop   */
    while(1) {
        int ch;
        String cmd = string_create(15);
        if (cmd==NULL) {
            perror("Memory allocation error");
            fclose(log_file);
            close(sockfd);
            return ALLOC_ERR;
        }
        int wrote_char=0;
        printf("> ");
        do {    // read and store command in this loop
            ch = getchar();
            if (ch==EOF) {
                string_free(cmd);
                char shutdown[]="shutdown\n";
                write(sockfd,shutdown,sizeof(shutdown)-1);
                putchar('\n');
                char return_code;
                read(sockfd,&return_code,1);
                read_shutdown();
                close(sockfd);
                fclose(log_file);
                return 0;
            }
            if (!isspace(ch))
                wrote_char = 1;
            if (string_push(cmd,ch)==-1) {
                string_free(cmd);
                perror("Memory allocation error");
                close(sockfd);
                fclose(log_file);
                return ALLOC_ERR;
            }
        } while (ch!='\n');
        if (wrote_char) {
            write(sockfd,string_ptr(cmd),string_length(cmd));
            char return_code;
            /*   nfs_manager sends a return code to show what command was inserted or if the command is invalid   */
            read(sockfd,&return_code,sizeof(return_code));
            if (return_code==NO_COMMAND) {
                while (1) {
                    char ch;
                    read(sockfd,&ch,sizeof(ch));
                    putc(ch,stdout);
                    putc(ch,log_file);
                    if (ch=='\n')
                        break;
                }
            }
            else if (return_code==SHUTDOWN) {
                read_shutdown();
                close(sockfd);
                fclose(log_file);
                string_free(cmd);
                return 0;
            }
            else {
                /*   For commands with argument, we wait for another char code   */
                char ch;
                read(sockfd,&ch,sizeof(ch));
                if (ch==INVALID_SOURCE || ch==INVALID_TARGET) {
                    while (1) {
                        read(sockfd,&ch,sizeof(ch));
                        putc(ch,stdout);
                        if (ch=='\n')
                            break;
                    }
                }
                else {
                    read_response();
                }
            }
        }
        string_free(cmd);
    }
}

/*   read_response first writes the command message from nfs_manager to the log, then reads the rest of the response   */
void read_response(void) {
    char ch;
    while (1) {
        read(sockfd,&ch,sizeof(ch));
        putc(ch,log_file);
        if (ch=='\n')
            break;
    }
    while (1) {
        read(sockfd,&ch,sizeof(ch));
        putc(ch,stdout);
        putc(ch,log_file);
        if (ch=='\n')
            break;
    }
}

void read_shutdown(void) {
    char ch;
    while (1) {
        read(sockfd,&ch,sizeof(ch));
        putc(ch,log_file);
        if (ch=='\n')
            break;
    }
    int chars_read;
    char buff[30];
    while((chars_read=read(sockfd,buff,sizeof(buff)))!=0) {
        fwrite(buff,sizeof(buff[0]),chars_read,stdout);
        fwrite(buff,sizeof(buff[0]),chars_read,log_file);
    }
}