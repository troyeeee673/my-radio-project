//创建频道线程
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "thr_channel.h"

struct thr_channel_ent_st
{
    chnid_t chnid;
    pthread_t tid;
};

struct thr_channel_ent_st thr_channel[CHNNM];
static int tid_nextpos = 0;

static void *thr_channel_snder(void * ptr)
{
    int len;
    struct msg_channel_st *sndbufp;
    struct mlib_listentry_st *ent = ptr;
    
    sndbufp = malloc(MSG_CHNNEL_MAX);
    if(sndbufp == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        exit(1);
    }
    sndbufp->chnid = ent->chnid;

    while(1)
    {
        len = mlib_readchn(ent->chnid, sndbufp->data, MAX_DATA);
        
        if(len <= 0)
        {
            // 没有读到数据，等待一下再重试
            syslog(LOG_WARNING, "thr_channel[%d]: mlib_readchn returned %d, sleeping...", ent->chnid, len);
            sleep(1);  // 等待1秒，避免疯狂循环
            continue;
        }

        if(sendto(serversd, sndbufp, sizeof(chnid_t) + len, 0, (void *)&sndaddr, sizeof(sndaddr)) < 0)
        {
            syslog(LOG_ERR, "thr_channel [%d] sendto():%s", ent->chnid, strerror(errno));
        }
        else
        {
            syslog(LOG_DEBUG, "thr_channel(%d): sendto() succeed", ent->chnid);
        }
        sched_yield();
    }
    
    pthread_exit(NULL);
}
int thr_channel_create(struct mlib_listentry_st* ptr)
{

    int err;
    err = pthread_create(&thr_channel[tid_nextpos].tid, NULL, thr_channel_snder, ptr);
    if(err)
    {
        syslog(LOG_ERR, "pthread_create():%s", strerror(errno));
        return -err;
    }
    thr_channel[tid_nextpos].chnid = ptr->chnid;
    tid_nextpos++;
    return 0;

}

int thr_channel_destroy(struct mlib_listentry_st* ptr)
{
    for(int i = 0 ;i < CHNNM ;i ++)
    {
        if(thr_channel[i].chnid == ptr->chnid)
        {
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR, "pthread_cancel() : %s", strerror(errno));
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;  // 标记为无效，避免destroyall时重复操作
            return 0;        
        }
    }
    return -1;  // 没找到对应的频道
}

int thr_channel_destroyall(void)
{
    for(int i = 0 ;i < CHNNM ;i++)
    {
        if(thr_channel[i].chnid > 0)
        {
            if(pthread_cancel(thr_channel[i].tid) < 0)
            {
                syslog(LOG_ERR, "pthread_cancel() : %s", strerror(errno));
                return -ESRCH;
            }
            pthread_join(thr_channel[i].tid, NULL);
            thr_channel[i].chnid = -1;
        }
    }
    return 0; 
}