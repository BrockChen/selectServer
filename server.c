#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include "protocol.h"

#define IPADDR      "0.0.0.0"
#define PORT        8080
#define MAXLINE     1024
#define LISTENQ     5
#define SIZE        10

#define MAX_CONNECTIONS 512


typedef struct server_context_st
{
    int cli_cnt;        /*客户端个数*/
    struct client *clifds[MAX_CONNECTIONS];
    fd_set allfds;      /*句柄集合*/
    int maxfd;          /*句柄最大值*/
} server_context_st;


static server_context_st s_srv_ctx;

static int create_server_proc(const char* ip,int port)
{
    int  fd;
    struct sockaddr_in servaddr;
    fd = socket(AF_INET, SOCK_STREAM,0);
    if (fd == -1) {
        fprintf(stderr, "create socket fail,erron:%d,reason:%s\n",
                errno, strerror(errno));
        return -1;
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        return -1;
    }


    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    if (bind(fd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1) {
        perror("bind error: ");
        return -1;
    }

    listen(fd,LISTENQ);

    return fd;
}

static int accept_client_proc(int srvfd)
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    cliaddrlen = sizeof(cliaddr);
    int clifd = -1;

    printf("accpet clint proc is called.\n");

ACCEPT:
    clifd = accept(srvfd,(struct sockaddr*)&cliaddr,&cliaddrlen);

    if (clifd == -1) {
        if (errno == EINTR) {
            goto ACCEPT;
        } else {
            fprintf(stderr, "accept fail,error:%s\n", strerror(errno));
            return -1;
        }
    }

    fprintf(stdout, "accept a new client: %s:%d - %d\n",
                   inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port, clifd);


    if(NULL==s_srv_ctx.clifds[clifd]){
        s_srv_ctx.clifds[clifd] = (struct client*)malloc(sizeof(struct client));
        s_srv_ctx.clifds[clifd]->fd = clifd;
        s_srv_ctx.cli_cnt++;
    }else{
        free(s_srv_ctx.clifds[clifd]);
        s_srv_ctx.clifds[clifd] = NULL;
        s_srv_ctx.cli_cnt--;
    }
    FD_SET(clifd, &s_srv_ctx.allfds);
    s_srv_ctx.maxfd = (clifd > s_srv_ctx.maxfd ? clifd : s_srv_ctx.maxfd);

}

int send_list_online(int conn){
      //struct pkg_head *hart = init_head(htonl(CMD_ONLINES), "helo");
      struct online_list onlines;
      onlines.inums = 0;

      init_head(&onlines.st_head, htonl(CMD_ONLINES), "helo");

      DEBUG("%s %d\n", onlines.st_head.szname, ntohl(onlines.st_head.icmd));

      char buf[1024] = {0};
      memset(buf, 0, sizeof(buf));
      char *pos = buf;
      memcpy(pos, (char*)(&onlines), sizeof(struct online_list));


      pos += sizeof(struct online_list);
      int i=0;
      for (;i<s_srv_ctx.maxfd;i++){
        if(s_srv_ctx.clifds[i]!=NULL){
          onlines.inums ++;
          memcpy(pos, (char*)s_srv_ctx.clifds[i], sizeof(struct client));
          DEBUG("client  %s\n", s_srv_ctx.clifds[i]->szname);
          //strncpy(pos, (char*)gclist[i], sizeof(struct client));
          pos += sizeof(struct client);
        }
      }

      struct online_list *tmp = (struct online_list*)buf;
      tmp->inums = htonl(onlines.inums);

      int mstlen = pos - buf;
      int nbytes=write(conn, (char*)buf, mstlen);
      if(nbytes==-1){
              DEBUG("send send_list_online error\n");
      }
      DEBUG("send send_list_online %d %d\n", mstlen, ntohl(tmp->st_head.icmd));
      //DEBUG("send  %s\n", tmp->onlins->szname);
}

static int handle_client_msg(int conn, char *buffer)
{
    assert(buffer);

    struct pkg_head *head = (struct pkg_head *)buffer;
    DEBUG("CMD %d %d\n", ntohl(head->icmd), head->icmd);
    switch(ntohl(head->icmd)){
        case CMD_TEXTMSG: {
            DEBUG("CMD_TEXTMSG\n");
            struct text_msg* msg = (struct text_msg*)buffer;
            printf("%s: %s", msg->szfrom, msg->szmsg);
            //DEBUG(" %d %d\n",  ntohs(login->st_head.si_optid), int(login->ic_dev_type));
            return 0;
        }
        case CMD_HARTBEAT: {
            DEBUG("CMD_HARTBEAT: %s\n", head->szname);
            // struct text_msg* msg = (struct text_msg*)buffer;
            // printf("%s: %s", msg->szfrom, msg->szmsg);
            //DEBUG(" %d %d\n",  ntohs(login->st_head.si_optid), int(login->ic_dev_type));
            return 0;
        }
        case CMD_LOGIN:{
            s_srv_ctx.clifds[conn] = (struct client*)malloc(sizeof(struct client));
            s_srv_ctx.clifds[conn]->fd = conn;
            strncpy(s_srv_ctx.clifds[conn]->szname, head->szname, strlen(head->szname));
            DEBUG("CMD_login: %d %s\n", conn, head->szname);
            send_list_online(conn);
            return -1;
        }

    }
    return 0;
}

static void recv_client_msg(fd_set *readfds)
{
    int i = 0, n = 0;
    int clifd;
    char buf[MAXLINE] = {0};
    for (i = 0;i <= s_srv_ctx.maxfd;i++) {
        if(s_srv_ctx.clifds[i]==NULL) continue;

        clifd = s_srv_ctx.clifds[i]->fd;
        if (clifd < 0) {
            continue;
        }

        if (FD_ISSET(clifd, readfds)) {
            //接收客户端发送的信息
            n = read(clifd, buf, MAXLINE);
            if (n <= 0) {
                DEBUG("client %d closed\n", clifd);
                FD_CLR(clifd, &s_srv_ctx.allfds);
                close(clifd);
                free(s_srv_ctx.clifds[i]);
                s_srv_ctx.clifds[i] = NULL;
                s_srv_ctx.cli_cnt--;
                continue;
            }
            DEBUG("get msg from client: %d\n", n);
            handle_client_msg(clifd, buf);
        }
    }
}
static void handle_client_proc(int srvfd)
{
    int  clifd = -1;
    int  retval = 0;
    //fd_set *readfds = &s_srv_ctx.allfds;
    struct timeval tv;
    int i = 0;
    fd_set read_fd_set;
    FD_ZERO (&s_srv_ctx.allfds);
    FD_ZERO (&read_fd_set);
    FD_SET (srvfd, &s_srv_ctx.allfds);
    s_srv_ctx.maxfd = srvfd;
    while (1) {

        /*每次调用select前都要重新设置文件描述符和时间，因为事件发生后，文件描述符和时间都被内核修改啦*/
        /*添加监听套接字*/

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        read_fd_set = s_srv_ctx.allfds;


        retval = select(s_srv_ctx.maxfd + 1, &read_fd_set, NULL, NULL, &tv);

        if (retval == -1) {
            fprintf(stderr, "select error:%s.\n", strerror(errno));
            return;
        }

        if (retval == 0) {
            fprintf(stdout, "select is timeout.\n");
            continue;
        }

        if (FD_ISSET(srvfd, &read_fd_set)) {
            /*监听客户端请求*/
            accept_client_proc(srvfd);
        } else {
            /*接受处理客户端消息*/
            recv_client_msg(&read_fd_set);
        }
    }
}


static void server_uninit()
{
    int i = 0;
    for (;i < MAX_CONNECTIONS; i++) {
        if(s_srv_ctx.clifds[i] != NULL)
            free(s_srv_ctx.clifds[i]);
    }

}

static int server_init()
{

    int i = 0;
    for (;i < MAX_CONNECTIONS; i++) {
        s_srv_ctx.clifds[i] = NULL;
    }
    s_srv_ctx.cli_cnt = 0;
    return 0;
}

int main(int argc,char *argv[])
{
    int  srvfd;

    if (server_init() < 0) {
        return -1;
    }

    int port  = 8080;
    if(argc>=2){
      //printf("program <port=8080>\n");
      port = atoi(argv[1]);
    }

    srvfd = create_server_proc(IPADDR, port);
    if (srvfd < 0) {
        fprintf(stderr, "socket create or bind fail.\n");
        goto err;
    }

    handle_client_proc(srvfd);

    return 0;

err:
    server_uninit();
    return -1;
}
