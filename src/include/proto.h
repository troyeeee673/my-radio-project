#ifndef __PROTO_H
#define __PROTO_H

#include "site_type.h"

#define DEFAULT_MULGROUP    "ff15::1"     //默认多播组号
#define DEFAULT_RCVPORT    "1989"

#define CHNNM               100            //总频道数

#define LISTCHNID           0               //节目清单发送频道号
#define MINCHNID            1               //最小频道号
#define MAXCHNID            (MINCHNID + CHNNM - 1) //最大频道号

#define MSG_CHNNEL_MAX      (65536 - 20 - 8) //减去了IP和UDP包的报头，得到最大数据包数据大小
#define MAX_DATA            (MSG_CHNNEL_MAX - sizeof(chnid_t))//data字段的大小

#define MSG_LIST_MAX        (65536 - 20 - 8)
#define MAX_ENTRY           (MSG_LIST_MAX - sizeof(chnid_t))

//节目数据传输内容
struct msg_channel_st
{
    chnid_t chnid;          //频道id
    uint8_t data[1];
}__attribute__((packed));//告诉编译器不进行对齐


//节目单记录
//1 music:xxxxxxxxxxxxxx
struct msg_listentry_st
{
    chnid_t chnid;  //频道号
    uint16_t len;   //当前数据结构体大小
    uint8_t disc[1]; //描述

};

//节目单频道传输内容
struct msg_list_st
{
    chnid_t chnid;      //节目清单传输频道（0）
    struct msg_listentry_st entry[1];
}__attribute__((packed));

#endif