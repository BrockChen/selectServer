#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>

#include "protocol.h"

int send_hartbeat(int sockfd);
int read_from_server(int sockfd);

#define MAXLINE 1024
#define IPADDRESS "127.0.0.1"
#define SERV_PORT 8080

#define max(a,b) (a > b) ? a : b

char gszname[60];
int send_login(int sockfd){

        struct pkg_head hart;
        init_head(&hart, htonl(CMD_LOGIN), gszname);
        int nbytes=write(sockfd, (char*)&hart, sizeof(struct pkg_head));
        if(nbytes==-1){
                DEBUG("send hartbead error\n");
        }

        DEBUG("send login %d %d\n", htonl(hart.icmd), hart.icmd);
        return 0;
}

int send_hartbeat(int sockfd){

        //struct pkg_head *hart = {htonl(CMD_HARTBEAT), htonl(1), htonl(0)};
        struct pkg_head hart;
        init_head(&hart, htonl(CMD_HARTBEAT), gszname);
        int nbytes=write(sockfd, (char*)&hart, sizeof(struct pkg_head));
        if(nbytes==-1){
                DEBUG("send hartbead error\n");
        }
        DEBUG("send hartbead \n");
        return 0;
}

static int handle_recv_msg(int sockfd, char *buffer)
{

    int nbytes;

    struct pkg_head *head = (struct pkg_head *)buffer;
    DEBUG("read_from_server -%s -%d -%d\n", head->szname, nbytes, ntohl(head->icmd));
    switch(ntohl(head->icmd)){
        case CMD_TEXTMSG: {
                struct text_msg* msg = (struct text_msg*)buffer;
                printf("%s: %s", msg->szfrom, msg->szmsg);
                //DEBUG(" %d %d\n",  ntohs(login->st_head.si_optid), int(login->ic_dev_type));
                return 0;
        }
        case CMD_ONLINES:{
            struct online_list* onlines = (struct online_list*)buffer;

            DEBUG("online %s %d\n", onlines->st_head.szname, ntohl(onlines->inums));

            struct client *start = buffer + sizeof(struct online_list);
            int i=0;
            for(;i<ntohl(onlines->inums); i++){
                    struct client *one =  start+i;
                    DEBUG("clients %d %s\n", one->fd, one->szname);
            }
            return 0;
        }

    }
    return 0;
}

static void handle_connection(int sockfd)
{
    char sendline[MAXLINE],recvline[MAXLINE];
    int maxfdp,stdineof;
    fd_set readfds;
    int n;
    struct timeval tv;
    int retval = 0;

    while (1) {

        FD_ZERO(&readfds);
        FD_SET(sockfd,&readfds);
        maxfdp = sockfd;

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        retval = select(maxfdp+1,&readfds,NULL,NULL,&tv);

        if (retval == -1) {
            printf("select error.\n");
            return ;
        }

        if (retval == 0) {
            printf("client timeout.\n");
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            n = read(sockfd,recvline,MAXLINE);
            if (n <= 0) {
                fprintf(stderr,"client: server is closed.\n");
                close(sockfd);
                FD_CLR(sockfd,&readfds);
                return;
            }

            handle_recv_msg(sockfd, recvline);
        }
    }
}

int main(int argc,char *argv[])
{

    struct hostent *host;
    int portnumber;
    if(argc!=4)
    {
            fprintf(stderr,"Usage:%s hostname portnumber name\a\n",argv[0]);
            exit(1);
    }

    if((host=gethostbyname(argv[1]))==NULL)
    {
            fprintf(stderr,"Gethostname error\n");
            exit(1);
    }

    if((portnumber=atoi(argv[2]))<0)
    {
            fprintf(stderr,"Usage:%s hostname portnumber\a\n",argv[0]);
            exit(1);
    }

    strncpy(gszname, argv[3], strlen(argv[3])+1);

    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET,SOCK_STREAM,0);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portnumber);
    servaddr.sin_addr=*((struct in_addr *)host->h_addr);
    //inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr);

    int retval = 0;
    retval = connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
    if (retval < 0) {
        fprintf(stderr, "connect fail,error:%s\n", strerror(errno));
        return -1;
    }

    printf("client send to server .\n");
    //write(sockfd, "hello server", 32);
    send_login(sockfd);
    handle_connection(sockfd);

    return 0;
}
