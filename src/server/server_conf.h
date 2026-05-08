#ifndef __SERVERCONF_H
#define __SERVERCONF_H

#define DEFAULT_MEDIADIR    "/var/media"
#define DEFAULT_IF          "ens33"

enum{
    RUN_DAEMON = 1, //后台运行
    RUN_FOREGROUND  //前台运行
};

struct server_conf_st
{
    char* rcvport;
    char* mgroup;
    char* media_dir;//媒体库位置
    char runmod;//运行模式（前台、后台）
    char* ifname;//指定网卡
};
extern struct server_conf_st server_conf;
extern int serversd;
extern struct sockaddr_in6 sndaddr;
#endif