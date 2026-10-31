/*
 * mjpeg_cache.c —— 缓存每路 MJPEG 的最新一帧
 *
 * 为什么需要它：
 *   VEU 文档明确禁止 JPEG 与 MJPEG 同时查询流，所以抓拍不能另起线程去
 *   轮询 FH_STREAM_JPEG。MJPEG 的每一帧本身就是完整的 JPEG，直接在 DMC
 *   回调里复制一帧下来，抓拍/事件留证都从这里取，既没有并发查询问题，
 *   也不需要额外的 VENC 抓拍通道。
 *
 * 线程：
 *   - 写入：get_stream 线程（dmc_http_mjpeg 的回调）
 *   - 读取：HTTP 8080 线程、snap_store 事件线程
 *   读写用每槽一把 mutex；复制完立即释放，不在锁内做网络/SD I/O。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "config.h"
#include "jpeg_config.h"
#include "stream/include/mjpeg_cache.h"

#define MJPEG_CACHE_STREAMS (MAX_GRP_NUM * MAX_VPU_CHN_NUM)

typedef struct
{
    pthread_mutex_t lock;
    unsigned char  *buf;
    int             len;
    unsigned        seq;
    unsigned        drops;
} mjpeg_cache_slot_t;

static mjpeg_cache_slot_t g_slots[MJPEG_CACHE_STREAMS];
static volatile int       g_ready = 0;

static int idx_valid(int idx)
{
    return (idx >= 0 && idx < MJPEG_CACHE_STREAMS);
}

int mjpeg_cache_init(void)
{
    int i;

    memset(g_slots, 0, sizeof(g_slots));
    for (i = 0; i < MJPEG_CACHE_STREAMS; i++)
    {
        pthread_mutex_init(&g_slots[i].lock, NULL);
    }

    g_ready = 1;
    printf("[mjpeg_cache] ready, %d slots x %dKB (lazy alloc)\n",
           MJPEG_CACHE_STREAMS, MJPEG_CACHE_BUF_SIZE / 1024);
    return 0;
}

void mjpeg_cache_deinit(void)
{
    int i;

    g_ready = 0;

    for (i = 0; i < MJPEG_CACHE_STREAMS; i++)
    {
        pthread_mutex_lock(&g_slots[i].lock);
        if (g_slots[i].buf)
        {
            free(g_slots[i].buf);
            g_slots[i].buf = NULL;
        }
        g_slots[i].len = 0;
        pthread_mutex_unlock(&g_slots[i].lock);
    }
}

void mjpeg_cache_on_frame(int server_index, const unsigned char *data, int len)
{
    mjpeg_cache_slot_t *s;

    if (!g_ready || !idx_valid(server_index) || data == NULL || len <= 0)
    {
        return;
    }
    if (len > MJPEG_CACHE_BUF_SIZE)
    {
        g_slots[server_index].drops++;
        return;
    }

    s = &g_slots[server_index];

    if (s->buf == NULL)
    {
        s->buf = (unsigned char *)malloc(MJPEG_CACHE_BUF_SIZE);
        if (s->buf == NULL)
        {
            s->drops++;
            return;
        }
    }

    pthread_mutex_lock(&s->lock);
    memcpy(s->buf, data, len);
    s->len = len;
    s->seq++;
    pthread_mutex_unlock(&s->lock);
}

int mjpeg_cache_copy(int server_index, unsigned char *dst, int cap)
{
    mjpeg_cache_slot_t *s;
    int ret = -1;

    if (!g_ready || !idx_valid(server_index) || dst == NULL || cap <= 0)
    {
        return -1;
    }

    s = &g_slots[server_index];

    pthread_mutex_lock(&s->lock);
    if (s->buf != NULL && s->len > 0)
    {
        if (s->len > cap)
        {
            ret = -2;
        }
        else
        {
            memcpy(dst, s->buf, s->len);
            ret = s->len;
        }
    }
    pthread_mutex_unlock(&s->lock);

    return ret;
}

int mjpeg_cache_index_of(int grp_id, int chn_id)
{
    ENC_INFO *info;
    int       idx;

    if (grp_id < 0 || grp_id >= MAX_GRP_NUM || chn_id < 0 || chn_id >= MAX_VPU_CHN_NUM)
    {
        return -1;
    }

    info = get_mjpeg_config(grp_id, chn_id);
    if (info == NULL || !info->enable || info->channel == 0)
    {
        return -1;
    }

    /* MJPEG 通道号从 MAX_MJPEG_CHN(30) 递减，和 libdmc_http_mjpeg.c
     * 里的 MAX_MJPEG_CHN - media_chn 是同一套映射。*/
    idx = MAX_MJPEG_CHN - info->channel;
    return idx_valid(idx) ? idx : -1;
}

unsigned mjpeg_cache_seq(int server_index)
{
    unsigned seq = 0;

    if (g_ready && idx_valid(server_index))
    {
        pthread_mutex_lock(&g_slots[server_index].lock);
        seq = g_slots[server_index].seq;
        pthread_mutex_unlock(&g_slots[server_index].lock);
    }
    return seq;
}

int mjpeg_cache_is_ready(int server_index)
{
    int ready = 0;

    if (g_ready && idx_valid(server_index))
    {
        pthread_mutex_lock(&g_slots[server_index].lock);
        ready = (g_slots[server_index].len > 0);
        pthread_mutex_unlock(&g_slots[server_index].lock);
    }
    return ready;
}
