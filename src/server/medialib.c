#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <syslog.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "medialib.h"
#include "mytbf.h"
#include <proto.h>
#include "server_conf.h"

#define PATHSIZE 1024
#define LINEBUFSIZE 1024
#define MP3_BITRATE 64*1024 //correct bps:128*1024
struct channel_context_st
{
    chnid_t chnid;
    char* disc;
    glob_t mpg3glob;
    int pos;
    int fd;
    off_t offset;//偏移
    mytbf_t *tbf;//流量控制
     

};

static struct channel_context_st channel[MAXCHNID + 1];


static struct channel_context_st* path2entry(const char *path)
{
    char pathstr[PATHSIZE];
    char linebuf[LINEBUFSIZE];
    FILE* fp;
    struct channel_context_st *me;
    static chnid_t curr_id = MINCHNID;

    strncpy(pathstr, path, PATHSIZE);
    strncat(pathstr, "/desc.txt", PATHSIZE - strlen(pathstr) - 1);

    fp = fopen(pathstr, "r");
    if(fp == NULL)
    {
        syslog(LOG_INFO, "%s is not a channel dir(Can't find desc file)", path);
        return NULL;
    }
    if(fgets(linebuf, LINEBUFSIZE, fp) == NULL)
    {
        syslog(LOG_INFO, "%s is not a channel dir(Can't find desc file)", path);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    me = malloc(sizeof(*me));
    if(me == NULL)
    {
        syslog(LOG_ERR, "malloc() :%s", strerror(errno));
        return NULL;
    }

    me ->tbf = mytbf_init(MP3_BITRATE/8, MP3_BITRATE/8*10);
    if(me->tbf == NULL)
    {
        syslog(LOG_ERR, "mytbf_init():%s", strerror(errno));
        free(me);
        return NULL;
    }

    me->disc = strdup(linebuf);
    strncpy(pathstr, path, PATHSIZE);
    strncat(pathstr, "/*.mp3", PATHSIZE - strlen(pathstr) - 1);
    if(glob(pathstr, 0, NULL, &me->mpg3glob) != 0)
    {
        curr_id ++;
        syslog(LOG_INFO, "%s is not a channel dir(Can't find desc file)", path);
        free(me);
        return NULL;
    }
    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->mpg3glob.gl_pathv[me->pos], O_RDONLY);

    if(me->fd < 0)
    {
        syslog(LOG_WARNING, "%s open() failed", me->mpg3glob.gl_pathv[me->pos]);
        free(me);
        return NULL;
    }
    me->chnid = curr_id;
    curr_id ++;
    return me;
}
int mlib_getchnlist(struct mlib_listentry_st **result, int*resnum)
{
    int i , num = 0 ;
    struct mlib_listentry_st *ptr;
    struct channel_context_st *res;
    char path[PATHSIZE];
    glob_t globres;
    for(i = 0 ;i < MAXCHNID + 1 ;i++)
    {
        channel[i].chnid = -1;
    }
    snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);
    if(glob(path, 0, NULL, &globres) != 0)
    {
        return -1;
    }
    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if(ptr == NULL)
    {
        syslog(LOG_ERR, "malloc() error");
        exit(1);
    }
    for(i = 0 ;i < globres.gl_pathc;i++)
    {
        res = path2entry(globres.gl_pathv[i]);//解析路径
        if(res != NULL)
        {
            syslog(LOG_DEBUG, "path2entry() returned : %d %s.", res->chnid, res->disc);
            memcpy(channel +res->chnid, res, sizeof(*res));
            ptr[num].chnid = res->chnid;
            ptr[num].disc = strdup(res->disc);
            num ++;
        }
        
    }
    *result = realloc(ptr, sizeof(struct mlib_listentry_st) * num);
    if(result == NULL)
    {
        syslog(LOG_ERR, "realloc() failed");
    }
    *resnum = num;
    return 0;

}
int mlib_freechnlist(struct mlib_listentry_st*ptr)
{
    free(ptr);
}


static int open_next(chnid_t id)
{
    for(int i = 0 ;i < channel[id].mpg3glob.gl_pathc;i++)
    {
        channel[id].pos ++;//移动到下一个内容的位置

        //该频道的所有内容都不能播放
        if(channel[id].pos == channel[id].mpg3glob.gl_pathc)
        {
            channel[id].pos = 0;
            break;
        }

        close(channel[id].fd);
        channel[id].fd = open(channel[id].mpg3glob.gl_pathv[channel[id].pos], O_RDONLY);
        if(channel[id].fd < 0)//失败
        {
            syslog(LOG_WARNING, "open(%s) :%s", channel[id].mpg3glob.gl_pathv[channel[id].pos],strerror(errno));
        }
        else
        {
            channel[id].offset = 0;//重新读取；
            return 0;
        }
    }
    syslog(LOG_ERR, "None of mp3s in channel %d is avaliable.", id);
    
}

ssize_t mlib_readchn(chnid_t id, void*buf, size_t size)
{
    int tbfsize;
    tbfsize = mytbf_fetchtoken(channel[id].tbf, size);

    int len;
    while(1)
    {
        len = pread(channel[id].fd, buf, tbfsize, channel[id].offset);//从id为id的频道中的offset开始读tbfsize个字节放到buf
        if(len < 0)//当前读取失败
        {
            syslog(LOG_WARNING, "media file %s pread():%s", channel[id].mpg3glob.gl_pathv[channel[id].pos],strerror(errno));
          open_next(id) < 0;//开始读取下一部分内容，跳过当前
          
        }
        else if(len ==0)//当前播放完毕
        {
            syslog(LOG_DEBUG,"media file %s plays over", channel[id].mpg3glob.gl_pathv[channel[id].pos]);
            open_next(id) < 0;
        }
        else //len > 0,
        {
            channel[id].offset += len;//更新偏移
            break;
        }
    }
    if(tbfsize - len > 0)
        mytbf_returntokne(channel[id].tbf, tbfsize - len);
    return len;
}
