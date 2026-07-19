//创建节目单线程
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "thr_list.h"

static pthread_t tid_list;
static int nr_list_ent;
static struct mlib_listentry_st* list_ent;

static void *thr_list(void *p)
{
    int totalsize, size;
    struct msg_list_st *entlistp;
    struct msg_listentry_st *entryp;
    totalsize = sizeof(chnid_t);
    int ret;

    for(int i = 0 ;i < nr_list_ent;i++)
    {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent[i].disc);
    }
    entlistp = malloc(totalsize);
    if(entlistp == NULL)
    {
        syslog(LOG_ERR, "malloc():%s", strerror(errno));
        exit(1);
    }
    entlistp->chnid = LISTCHNID;
    entryp = entlistp->entry;//这里会报内存对齐警告
    

    for(int i = 0 ;i < nr_list_ent;i++)
    {

        size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].disc);

        entryp->chnid = list_ent[i].chnid;
        entryp->len = htons(size);
        strcpy(entryp->disc, list_ent[i].disc);
        entryp = (void*)(((char*)entryp) + size);

    }
    while(1)
    {
        ret = sendto(serversd, entlistp, totalsize, 0, (void *)&sndaddr, sizeof(sndaddr));
        if(ret < 0)
        {
            syslog(LOG_WARNING, "sendto(serversd, entlistp...:%s", strerror(errno));

        }
        else
        {
            syslog(LOG_DEBUG, "sendto(serversd, entlistp...):successfully");
        }
        sleep(1);
    }

}
//创建节目单线程
int thr_list_create(struct mlib_listentry_st *listp, int nr_ent)
{
    int err;
    list_ent = listp;
    nr_list_ent = nr_ent;
    
    // 添加空列表检查
    if (nr_ent == 0 || listp == NULL) {
        syslog(LOG_WARNING, "No channels found, thr_list will not be created");
        return 0;  // 或者返回错误码
    }
    
    syslog(LOG_DEBUG, "list content: chnid:%d, desc:%s\n", listp->chnid, listp->disc);
    err = pthread_create(&tid_list, NULL, thr_list, NULL);
    if(err)
    {
        syslog(LOG_ERR, "pthread_create():%s", strerror(errno));
        return -1;
    }
    return 0;
}

int thr_list_destroy(void)
{
    pthread_cancel(tid_list);
    pthread_join(tid_list, NULL);
    return 0;
}