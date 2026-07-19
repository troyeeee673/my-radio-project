#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>  // umask
#include <arpa/inet.h> // inet_pton
#include <net/if.h>

#include "../include/proto.h"
#include "medialib.h"
#include "thr_list.h"
#include "thr_channel.h"
#include "server_conf.h"

/*
 *  -M      指定多播组
 *  -P      指定接收端口
 *  -F      使守护进程前台运行，便于查看输出
 *  -H      显示帮助
 *  -D      指定媒体库位置
 *  -I      网卡选择
 **/

struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,
                                     .mgroup = DEFAULT_MULGROUP,
                                     .media_dir = DEFAULT_MEDIADIR,
                                     .runmod = RUN_DAEMON,
                                     .ifname = DEFAULT_IF};

int serversd;
struct sockaddr_in sndaddr;
static struct mlib_listentry_st *list;
void print_help()
{
    printf("-M      指定多播组\n");
    printf("-P      指定接收端口\n");
    printf("-F      使守护进程前台运行，便于查看输出\n");
    printf("-D      指定媒体库位置\n");
    printf("-I      网卡选择\n");
    printf("-H      显示帮助\n");
}

static void daemon_exit(int s)
{
    thr_list_destroy();
    thr_channel_destroyall();
    mlib_freechnlist(list);

    syslog(LOG_WARNING, "signal %d caught", s);
    closelog();
    exit(0);
}

static int daemonize()
{
    pid_t pid;
    int fd;
    pid = fork();
    if (pid < 0)
    {
        // perror("fork()");
        syslog(LOG_ERR, "fork() : %s", strerror(errno));
        return -1;
    }
    if (pid == 0)
    {
        fd = open("/dev/null", O_RDWR);
        if (fd < 0)
        {
            // perror("open()");
            syslog(LOG_WARNING, "open() : %s", strerror(errno));
            return -2;
        }
        else
        {
            dup2(fd, 0);
            dup2(fd, 1);
            dup2(fd, 2);

            if (fd > 2)
            {
                close(fd);
            }
        }

        setsid();
        chdir("/");
        umask(0);
        return 0;
    }
    else
        exit(0);
}

static int socket_init()
{
    struct ip_mreq mreq;  // IPv4多播结构体
    
    serversd = socket(AF_INET, SOCK_DGRAM, 0);  // AF_INET6 改成 AF_INET
    if (serversd < 0)
    {
        syslog(LOG_ERR, "socket() : %s", strerror(errno));
        exit(1);
    }

    // 设置多播出口接口
    struct in_addr ifaddr;
    ifaddr.s_addr = htonl(INADDR_ANY);  // 或者用实际IP
    if (setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr, sizeof(ifaddr)) < 0)
    {
        syslog(LOG_ERR, "setsockopt(IP_MULTICAST_IF) : %s", strerror(errno));
        exit(1);
    }

    // 加入多播组
    inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(serversd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        syslog(LOG_ERR, "setsockopt(IP_ADD_MEMBERSHIP) : %s", strerror(errno));
        exit(1);
    }

    // 设置发送目标地址
    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr);
    
    return 0;
}

int main(int argc, char **argv)
{
    int c;
    int i;
    struct sigaction sa;
    sa.sa_handler = daemon_exit;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGQUIT);

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    openlog("netradio", LOG_PID | LOG_PERROR, LOG_DAEMON);
    /*命令行分析*/
    while (1)
    {
        c = getopt(argc, argv, "M:P:FD:I:H");
        if (c < 0)
            break;
        switch (c)
        {
        case 'M':
            server_conf.mgroup = optarg;
            break;
        case 'P':
            server_conf.rcvport = optarg;
            break;
        case 'F':
            server_conf.runmod = RUN_FOREGROUND;
            break;
        case 'D':
            server_conf.media_dir = optarg;
            break;
        case 'I':
            server_conf.ifname = optarg;
            break;
        case 'H':
            print_help();
            exit(0);
            break;

        default:
            abort();
            break;
        }
    }

    /*守护进程实现*/
    if (server_conf.runmod == RUN_DAEMON)
        if (daemonize() < 0)
        {
            exit(1);
        }
        else if (server_conf.runmod == RUN_FOREGROUND)
        {
            // 跑在前台，donothing
        }
        else
        {
            // fprintf(stderr, "EINVAL\n");
            syslog(LOG_ERR, "EINVAL server_conf.runmode.");
            exit(1);
        }

    /*SOCKET初始化*/
    socket_init();

    /*获取频道信息，为了组织节目单向外发送*/

    int list_size;
    int err;
    err = mlib_getchnlist(&list, &list_size);
    if (err)
    {
        syslog(LOG_ERR, "mlib_getchnlist():%s", strerror(errno));
        exit(1);
    }

    /*创建节目单线程，向外发送当期有哪些节目（节目单信息）*/
    err = thr_list_create(list, list_size);
    if (err)
        exit(1);

    /*创建频道线程，每个频道向外发送当前频道的内容*/
    for (i = 0; i < list_size; i++)
    {
        thr_channel_create(list + i);
        /*if error*/
        if (err)
        {
            fprintf(stderr, "thr_channel_create():%s\n", strerror(err));
            exit(1);
        }
    }

    syslog(LOG_DEBUG, "%d channel threads created", i);

    while (1)
    {
        pause();
    }
    closelog();
    exit(0);
}