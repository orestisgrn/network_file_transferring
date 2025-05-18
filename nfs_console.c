#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(void) {
    char addr[20];
    int port;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    scanf("%s",addr);
    scanf("%d",&port);
    struct sockaddr_in str;
    str.sin_family = AF_INET;
    str.sin_port = htons(port);
    inet_aton(addr,&str.sin_addr);
    if (connect(sockfd,(struct sockaddr *) &str,sizeof(str))==-1) {
        printf("%d\n",errno);
    }
    return 0;
}