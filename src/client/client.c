#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#include "client.h"
#include "../include/proto.h"

/*
*   -M  --mgroup  指定多播组
*   -P  --port    指定接收端口
*   -p  --player  指定播放器
*   -H  --help    显示帮助
**/

//配置接收端参数
struct client_conf_st client_conf = {\
            .rcvport = DEFAULT_RCVPORT,\
            .mgroup = DEFAULT_MULGROUP,\
            .player_cmd = DEFAULT_PLAYERCMD};

static void help_print()
{
    printf("-P  --port    指定接收端口\n\
            -M  --mgroup  指定多播组\n\
            -p  --player  指定播放器\n\
            -H  --help    显示帮助\n");
}

static ssize_t writen(int fd, const char* buf, size_t len)
{
    int ret, pos = 0;
    while (len > 0)
    {
        ret = write(fd, buf+ pos, len);
        if(ret < 0)
        {
            if(errno == EINTR)
                continue;
            perror("write()");
            return -1;
        }
        len -= ret;
        pos += ret;
    }
    return pos;
}

// IPv4 版本地址比较
static int equal_addr(const struct in_addr *a, const struct in_addr *b)
{
    return a->s_addr == b->s_addr;
}

// IPv4 版本端口比较
static int equal_port(in_port_t a, in_port_t b)
{
    return a == b;
}

int main(int argc, char**argv)
{
    int pd[2];
    pid_t pid;
    int index = 0, c;
    int sd;
    int val = 1;
    struct option argarr[] = {{"port", 1, NULL, 'P'},   \
                              {"mgroup", 1, NULL, 'M'}, \
                              {"player", 1, NULL, 'p'}, \
                              {"help", 0, NULL, 'H'},\
                              {NULL, 0, NULL, 0}};
    
    struct ip_mreq mreq;                           // IPv4多播结构体
    struct sockaddr_in laddr, serveraddr, raddr;   // IPv4地址结构体
    socklen_t serveraddr_len = sizeof(serveraddr), raddr_len = sizeof(raddr);
    int len;
    int chosenid;//选择的频道id
    int ret = 0;

    /*
    *   初始化
    *   级别：默认文件，配置文件，环境变量，命令行参数（依次增高）
    * */
   while(1)
   {
        c = getopt_long(argc, argv, "P:M:p:H", argarr, &index);
        if(c < 0)
        {
            break;
        }
        switch (c)
        {
        case 'P':
            client_conf.rcvport = optarg;
            break;
        case 'M':
            client_conf.mgroup = optarg;
            break;
        case 'p':
            client_conf.player_cmd = optarg;
            break;
        case 'H':
            help_print();
            exit(0);
            break;
        default:
            abort();
            break;
        }
   }
    

    // 创建 IPv4 socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0)
    {
        perror("socket()");
        exit(1);
    }

    // 设置多播地址和接口
    if(inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr) < 0)
    {
        perror("inet_pton()");
        exit(1);
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // 加入多播组
    if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt(IP_ADD_MEMBERSHIP)");
        exit(1);
    }

    // 设置多播回环
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0)
    {
        perror("setsockopt(IP_MULTICAST_LOOP)");
        exit(1);
    }

    // 绑定本地地址和端口
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(client_conf.rcvport));
    laddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sd, (void *)&laddr, sizeof(laddr)) < 0)
    {
        perror("bind()");
        exit(1);
    }

    // 创建管道
    if(pipe(pd) <0)
    {
        perror("pipe()");
        exit(1);
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork()");
        exit(1);
    }
    //子进程：调用解码器进行播放
    if(pid == 0)
    {
        close(sd);//子进程不需要使用socket
        close(pd[1]);//关闭管道写端
        dup2(pd[0], 0);//解码器只能读取标准输入的内容，所以将标准输入进行重定向
        if(pd[0] > 0)
            close(pd[0]);
        //调用解码器
        execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
        perror("execl()");
        exit(1);
    }
// 父进程：从网络收包
else
{
    close(pd[0]);  // 先关闭管道读端，但暂时不用管道

    // 收节目单
    struct msg_list_st * msg_list;
    msg_list = malloc(MSG_LIST_MAX);
    if(msg_list == NULL)
    {
        perror("malloc()");
        exit(1);
    }
    while(1)
    {
        len = recvfrom(sd, msg_list, MSG_LIST_MAX, 0, (void *)&serveraddr, &serveraddr_len);
        if(len < sizeof(struct msg_list_st))
        {
            fprintf(stderr, "message is too short\n");
            continue;
        }
        if(msg_list->chnid != LISTCHNID)
        {
            fprintf(stderr, "channel id is not match\n");
            continue;
        }
        break;
    }

    // 打印节目单、选择频道
    struct msg_listentry_st *pos;
    for(pos = msg_list->entry; (char *)pos < (((char *)msg_list) + len); 
        pos = (void *)(((char*)pos) + ntohs(pos->len)))
    {
        printf("channel %d:%s\n", pos->chnid, pos->disc);
    }

    printf("Please choose a channel: ");
    fflush(stdout);
    ret = 0;
    while(ret < 1)
    {
        ret = scanf("%d", &chosenid);
        if(ret != 1) {
            fprintf(stderr, "Invalid input\n");
            exit(1);
        }
    }

    printf("chosenid = %d\n", chosenid);

    // 选完频道后再 fork 启动播放器
    pid = fork();
    if (pid < 0)
    {
        perror("fork()");
        exit(1);
    }

    if(pid == 0)
    {
        // 子进程：调用解码器播放
        close(sd);
        close(pd[1]);
        dup2(pd[0], 0);
        if(pd[0] > 0)
            close(pd[0]);
        execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
        perror("execl()");
        exit(1);
    }
    else
    {
        // 父进程：接收频道数据，传给子进程
        close(pd[0]);

        struct msg_channel_st *msg_channel;
        msg_channel = malloc(MSG_CHNNEL_MAX);
        if(msg_channel == NULL)
        {
            perror("malloc()");
            exit(1);
        }
        while(1)
        {
            len = recvfrom(sd, msg_channel, MSG_CHNNEL_MAX, 0, (void *)&raddr, &raddr_len);
            if(len < 0)
            {
                perror("recvfrom()");
                continue;
            }
            if(!equal_addr(&raddr.sin_addr, &serveraddr.sin_addr) || 
               !equal_port(raddr.sin_port, serveraddr.sin_port))
            {
                fprintf(stderr, "Ignore : address not match\n");
                continue;
            }
            if(len < sizeof(struct msg_channel_st))
            {
                fprintf(stderr, "Ignore : message is too short\n");
                continue;
            }
            if(msg_channel->chnid == chosenid)
            {
                if(writen(pd[1], (char*)msg_channel->data, len - sizeof(chnid_t)) < 0)
                    exit(1);
            }
        }
        free(msg_channel);
        close(sd);
    }

    }
    exit(0);
}