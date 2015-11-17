
#ifndef __GLOBAL_STRUCT_DEF
#define __GLOBAL_STRUCT_DEF

#define DEBUG(format,...) printf("["__FILE__":%d] [%s] "format, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define CMD_HARTBEAT 1
#define CMD_TEXTMSG 2
#define CMD_ONLINES 3
#define CMD_OFFLINES 4
#define CMD_LOGIN 5




struct pkg_head{
    int icmd;
    int iver;
    int imsglen;
    char szname[60];
};

struct text_msg
{
    struct pkg_head st_head;
    char szfrom[60];
    char szmsg[521];
};


struct client{
  int fd;
  char szname[60];
};

struct online_list{
  struct pkg_head st_head;
  int inums;
};



void init_head(struct pkg_head*head, int cmd, char*name){
    head->icmd = cmd;
    memset(head->szname, 0 , 59);

    strncpy(head->szname, name, strlen(name)+1);
    //DEBUG("name len: %s %d\n", head->szname, strlen(name) );
    head->iver = 0;


}

#endif


