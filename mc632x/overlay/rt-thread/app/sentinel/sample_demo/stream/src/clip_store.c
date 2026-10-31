/*
 * clip_store.c —— 事件前后 5 秒 MJPEG AVI
 *
 * pre 环在没事件时一直收；事件线程 begin 后冻结 pre、开始收 post，
 * 等 5 秒把 pre+post 写成 AVI。观察者只 memcpy，不碰 SD。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "stream/include/clip_store.h"
#include "stream/include/mjpeg_cache.h"
#include "libsample_common/include/libsample_common.h"

#define CLIP_SLOTS          25
#define CLIP_FRAME_MAX      (128 * 1024)
#define CLIP_FRAME_MS       200
#define CLIP_POST_MS        5000
#define CLIP_AVI_HDR        224

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned char *g_pre_buf;
static unsigned char *g_post_buf;
static int            g_pre_len[CLIP_SLOTS];
static int            g_post_len[CLIP_SLOTS];
static unsigned       g_pre_ms[CLIP_SLOTS];
static unsigned       g_post_ms[CLIP_SLOTS];
static int            g_pre_head;
static int            g_pre_count;
static int            g_post_count;

static int            g_ai_idx = -1;
static int            g_active;
static int            g_finishing;
static unsigned       g_end_ms;
static unsigned       g_last_store_ms;
static int            g_inited;

static void w32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void w16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
}

static void wtag(unsigned char *p, const char *t)
{
    memcpy(p, t, 4);
}

static int write_all(int fd, const void *buf, int len)
{
    const unsigned char *p = (const unsigned char *)buf;
    int left = len;

    while (left > 0)
    {
        int n = (int)write(fd, p, (size_t)left);
        if (n <= 0)
        {
            return -1;
        }
        p += n;
        left -= n;
    }
    return 0;
}

/* 从 JPEG 的 SOF 里取宽高；失败保持默认。*/
static void jpeg_size(const unsigned char *p, int len, int *w, int *h)
{
    int i;

    for (i = 0; i + 9 < len;)
    {
        if (p[i] != 0xFF)
        {
            i++;
            continue;
        }

        unsigned char m = p[i + 1];
        if (m == 0xD8 || m == 0xD9)
        {
            i += 2;
            continue;
        }
        if (m == 0xDA)
        {
            break;      /* 到 SOS，后面是压缩数据 */
        }

        int seg = (p[i + 2] << 8) | p[i + 3];
        if (seg < 2)
        {
            break;
        }
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)
        {
            *h = (p[i + 5] << 8) | p[i + 6];
            *w = (p[i + 7] << 8) | p[i + 8];
            return;
        }
        i += 2 + seg;
    }
}

int clip_store_init(void)
{
    if (g_inited)
    {
        return 0;
    }

    g_ai_idx = mjpeg_cache_index_of(1, 1);
    g_pre_buf = (unsigned char *)malloc(CLIP_SLOTS * CLIP_FRAME_MAX);
    g_post_buf = (unsigned char *)malloc(CLIP_SLOTS * CLIP_FRAME_MAX);
    if (g_pre_buf == NULL || g_post_buf == NULL)
    {
        free(g_pre_buf);
        free(g_post_buf);
        g_pre_buf = NULL;
        g_post_buf = NULL;
        printf("[clip_store] malloc %dKB x2 failed\n",
               (CLIP_SLOTS * CLIP_FRAME_MAX) / 1024);
        return -1;
    }

    memset(g_pre_len, 0, sizeof(g_pre_len));
    memset(g_post_len, 0, sizeof(g_post_len));
    g_pre_head = 0;
    g_pre_count = 0;
    g_post_count = 0;
    g_active = 0;
    g_finishing = 0;
    g_last_store_ms = 0;
    g_inited = 1;

    printf("[clip_store] ready, idx=%d, %d slots x %dKB, %dms/frame\n",
           g_ai_idx, CLIP_SLOTS, CLIP_FRAME_MAX / 1024, CLIP_FRAME_MS);
    return 0;
}

void clip_store_deinit(void)
{
    pthread_mutex_lock(&g_lock);
    free(g_pre_buf);
    free(g_post_buf);
    g_pre_buf = NULL;
    g_post_buf = NULL;
    g_inited = 0;
    g_active = 0;
    g_finishing = 0;
    g_pre_count = 0;
    g_post_count = 0;
    pthread_mutex_unlock(&g_lock);
}

void clip_store_on_frame(int server_index, const unsigned char *data, int len)
{
    unsigned now;
    int slot;

    if (!g_inited || data == NULL || len <= 0 || len > CLIP_FRAME_MAX)
    {
        return;
    }

    if (g_ai_idx < 0)
    {
        /* 初始化时 MJPEG 映射可能还没设好；第一帧再查一次。
         * 两路 MJPEG 里 AI 是第二路（1112），兜底用 index 1。*/
        g_ai_idx = mjpeg_cache_index_of(1, 1);
        if (g_ai_idx < 0)
        {
            g_ai_idx = 1;
        }
        printf("[clip_store] ai index resolved to %d\n", g_ai_idx);
    }

    if (server_index != g_ai_idx)
    {
        return;
    }

    now = (unsigned)get_ms();

    pthread_mutex_lock(&g_lock);
    if (g_finishing)
    {
        pthread_mutex_unlock(&g_lock);
        return;
    }

    if (g_active)
    {
        if (now <= g_end_ms && g_post_count < CLIP_SLOTS &&
            (now - g_last_store_ms) >= CLIP_FRAME_MS)
        {
            slot = g_post_count++;
            memcpy(g_post_buf + (size_t)slot * CLIP_FRAME_MAX, data, (size_t)len);
            g_post_len[slot] = len;
            g_post_ms[slot] = now;
            g_last_store_ms = now;
        }
    }
    else if ((now - g_last_store_ms) >= CLIP_FRAME_MS)
    {
        slot = g_pre_head;
        memcpy(g_pre_buf + (size_t)slot * CLIP_FRAME_MAX, data, (size_t)len);
        g_pre_len[slot] = len;
        g_pre_ms[slot] = now;
        g_pre_head = (g_pre_head + 1) % CLIP_SLOTS;
        if (g_pre_count < CLIP_SLOTS)
        {
            g_pre_count++;
        }
        g_last_store_ms = now;
    }
    pthread_mutex_unlock(&g_lock);
}

int clip_store_begin(unsigned event_ms)
{
    unsigned now = (unsigned)get_ms();

    (void)event_ms;

    pthread_mutex_lock(&g_lock);
    if (!g_inited || g_active || g_finishing || g_pre_count == 0)
    {
        pthread_mutex_unlock(&g_lock);
        return 0;
    }

    g_active = 1;
    g_end_ms = now + CLIP_POST_MS;
    g_post_count = 0;
    g_last_store_ms = now - CLIP_FRAME_MS;
    pthread_mutex_unlock(&g_lock);

    return 1;
}

int clip_store_wait_end(void)
{
    while (1)
    {
        unsigned now;
        int active;
        unsigned end;

        pthread_mutex_lock(&g_lock);
        now = (unsigned)get_ms();
        active = g_active;
        end = g_end_ms;
        pthread_mutex_unlock(&g_lock);

        if (!active || now >= end)
        {
            break;
        }
        usleep(50 * 1000);
    }
    return 1;
}

int clip_store_finish(const char *path, int cap, unsigned *size, int *w, int *h)
{
    unsigned char hdr[CLIP_AVI_HDR];
    unsigned char *p;
    int      pre;
    int      post;
    int      frames;
    int      pre_start;
    int      width = 800;
    int      height = 480;
    int      maxlen = 0;
    unsigned sum = 0;
    unsigned total_size;
    int      fd;
    int      i;
    int      ret = 0;

    (void)cap;

    pthread_mutex_lock(&g_lock);
    if (!g_active && !g_finishing)
    {
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    g_finishing = 1;
    pre = g_pre_count;
    post = g_post_count;
    pre_start = (g_pre_head - pre + CLIP_SLOTS) % CLIP_SLOTS;
    pthread_mutex_unlock(&g_lock);

    frames = pre + post;
    if (frames <= 0)
    {
        clip_store_abort();
        return -1;
    }

    /* 取第一帧的尺寸。*/
    {
        int slot = (pre > 0) ? pre_start : 0;
        const unsigned char *first = (pre > 0)
            ? (g_pre_buf + (size_t)slot * CLIP_FRAME_MAX)
            : (g_post_buf + 0);
        int        first_len = (pre > 0) ? g_pre_len[slot] : g_post_len[0];

        jpeg_size(first, first_len, &width, &height);
    }
    if (width <= 0 || height <= 0)
    {
        width = 800;
        height = 480;
    }

    for (i = 0; i < pre; i++)
    {
        int slot = (pre_start + i) % CLIP_SLOTS;
        int n = g_pre_len[slot];
        sum += 8u + (unsigned)n + ((n & 1) ? 1u : 0u);
        if (n > maxlen)
        {
            maxlen = n;
        }
    }
    for (i = 0; i < post; i++)
    {
        int n = g_post_len[i];
        sum += 8u + (unsigned)n + ((n & 1) ? 1u : 0u);
        if (n > maxlen)
        {
            maxlen = n;
        }
    }
    total_size = CLIP_AVI_HDR + sum;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        clip_store_abort();
        return -1;
    }

    memset(hdr, 0, sizeof(hdr));
    p = hdr;
    wtag(p, "RIFF"); w32(p + 4, 216u + sum); p += 8;
    wtag(p, "AVI "); p += 4;
    wtag(p, "LIST"); w32(p + 4, 192); p += 8;
    wtag(p, "hdrl"); p += 4;
    wtag(p, "avih"); w32(p + 4, 56); p += 8;
    w32(p + 0, CLIP_FRAME_MS * 1000u);      /* dwMicroSecPerFrame */
    w32(p + 4, (uint32_t)maxlen * 5u);      /* dwMaxBytesPerSec */
    w32(p + 8, 0);                          /* dwPaddingGranularity */
    w32(p + 12, 0);                         /* dwFlags: no index */
    w32(p + 16, (uint32_t)frames);          /* dwTotalFrames */
    w32(p + 20, 0);                         /* dwInitialFrames */
    w32(p + 24, 1);                         /* dwStreams */
    w32(p + 28, (uint32_t)maxlen);          /* dwSuggestedBufferSize */
    w32(p + 32, (uint32_t)width);           /* dwWidth */
    w32(p + 36, (uint32_t)height);          /* dwHeight */
    p += 56;
    wtag(p, "LIST"); w32(p + 4, 116); p += 8;
    wtag(p, "strl"); p += 4;
    wtag(p, "strh"); w32(p + 4, 56); p += 8;
    wtag(p + 0, "vids");
    wtag(p + 4, "MJPG");
    w32(p + 8, 0);                          /* dwFlags */
    w16(p + 12, 0);                         /* wPriority */
    w16(p + 14, 0);                         /* wLanguage */
    w32(p + 16, 0);                         /* dwInitialFrames */
    w32(p + 20, 1);                         /* dwScale */
    w32(p + 24, 5);                         /* dwRate */
    w32(p + 28, 0);                         /* dwStart */
    w32(p + 32, (uint32_t)frames);          /* dwLength */
    w32(p + 36, (uint32_t)maxlen);          /* dwSuggestedBufferSize */
    w32(p + 40, 0xFFFFFFFFu);               /* dwQuality */
    w32(p + 44, 0);                         /* dwSampleSize */
    w16(p + 48, 0);                         /* rcFrame.left */
    w16(p + 50, 0);                         /* rcFrame.top */
    w16(p + 52, (uint16_t)width);           /* rcFrame.right */
    w16(p + 54, (uint16_t)height);          /* rcFrame.bottom */
    p += 56;
    wtag(p, "strf"); w32(p + 4, 40); p += 8;
    w32(p + 0, 40);                         /* biSize */
    w32(p + 4, (uint32_t)width);
    w32(p + 8, (uint32_t)height);
    w16(p + 12, 1);                         /* biPlanes */
    w16(p + 14, 24);                        /* biBitCount */
    wtag(p + 16, "MJPG");                   /* biCompression */
    w32(p + 20, (uint32_t)(width * height * 3));
    w32(p + 24, 0);                         /* biXPelsPerMeter */
    w32(p + 28, 0);                         /* biYPelsPerMeter */
    w32(p + 32, 0);                         /* biClrUsed */
    w32(p + 36, 0);                         /* biClrImportant */
    p += 40;
    wtag(p, "LIST"); w32(p + 4, 4u + sum); p += 8;
    wtag(p, "movi"); p += 4;

    if ((int)(p - hdr) != CLIP_AVI_HDR ||
        write_all(fd, hdr, CLIP_AVI_HDR) != 0)
    {
        ret = -1;
        goto out;
    }

    for (i = 0; i < pre; i++)
    {
        int slot = (pre_start + i) % CLIP_SLOTS;
        int n = g_pre_len[slot];
        unsigned char tag[8];

        wtag(tag, "00dc");
        w32(tag + 4, (uint32_t)n);
        if (write_all(fd, tag, 8) != 0 ||
            write_all(fd, g_pre_buf + (size_t)slot * CLIP_FRAME_MAX, n) != 0 ||
            ((n & 1) && write_all(fd, "\0", 1) != 0))
        {
            ret = -1;
            goto out;
        }
    }
    for (i = 0; i < post; i++)
    {
        int n = g_post_len[i];
        unsigned char tag[8];

        wtag(tag, "00dc");
        w32(tag + 4, (uint32_t)n);
        if (write_all(fd, tag, 8) != 0 ||
            write_all(fd, g_post_buf + (size_t)i * CLIP_FRAME_MAX, n) != 0 ||
            ((n & 1) && write_all(fd, "\0", 1) != 0))
        {
            ret = -1;
            goto out;
        }
    }

out:
    if (ret == 0)
    {
        (void)fsync(fd);
    }
    close(fd);
    if (ret != 0)
    {
        unlink(path);
    }
    else if (size != NULL)
    {
        *size = total_size;
    }
    if (w != NULL)
    {
        *w = width;
    }
    if (h != NULL)
    {
        *h = height;
    }

    pthread_mutex_lock(&g_lock);
    g_active = 0;
    g_finishing = 0;
    g_pre_count = 0;
    g_pre_head = 0;
    g_post_count = 0;
    pthread_mutex_unlock(&g_lock);

    return ret;
}

void clip_store_abort(void)
{
    pthread_mutex_lock(&g_lock);
    g_active = 0;
    g_finishing = 0;
    g_pre_count = 0;
    g_pre_head = 0;
    g_post_count = 0;
    pthread_mutex_unlock(&g_lock);
}
