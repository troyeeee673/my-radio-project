#ifndef __THRLIST_H
#define __THRLIST_H

#include "medialib.h"
//创建节目单线程
//拿到节目单list,其中有size个节目
int thr_list_create(struct mlib_listentry_st*list, int size);
static void *thr_list(void *p);
int thr_list_destroy();

#endif