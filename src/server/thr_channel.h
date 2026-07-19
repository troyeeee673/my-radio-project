#ifndef __THR_CHANNEL_H
#define __THR_CHANNEL_H

#include "medialib.h"
#include "../include/proto.h"
#include "server_conf.h"
#include "medialib.h"
//拿到当前频道信息进行创建线程
int thr_channel_create(struct mlib_listentry_st*);
int thr_channel_destroy(struct mlib_listentry_st*);
int thr_channel_destroyall();//销毁所有频道线程

#endif