#ifndef __MEDIALIB_H
#define __MEDIALIB_H
#include "site_type.h"
#include "proto.h"
#include "mytbf.h"
#include "server_conf.h"

//每一个频道的信息结构体
//这里由于获取频道信息，数据还在本机上，没有进行网络传输，所以可以使用指针进行数据传递
//在实际需要网络传输的情况下，绝不允许使用指针进行数据传递
struct mlib_listentry_st
{
    chnid_t chnid;//频道id
    char* disc;//频道描述
};

//使用结构体数组回填节目单，并传递数组长度
int mlib_getchnlist(struct mlib_listentry_st **, int*);
int mlib_freechnlist(struct mlib_listentry_st*);//上面会申请空间，要自己释放

//从chnid 为id的频道读取size个字节到目标obj中
ssize_t mlib_readchn(chnid_t id, void* obj, size_t size);//返回实际读到字节数

#endif