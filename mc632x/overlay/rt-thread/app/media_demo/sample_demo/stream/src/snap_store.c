/*
 * snap_store.c —— 抓拍落盘 + 事件环形缓冲
 *
 * 设计要点：
 *   1. 抓拍数据来自 mjpeg_cache（最新一帧 MJPEG 本身就是完整 JPEG），
 *      不碰 FH_STREAM_JPEG，避免和 MJPEG 并发查询冲突。
 *   2. NNA 线程只做 memcpy + 置 pending，SD 写在独立事件线程里做，
 *      避免绊线触发时阻塞推理。
 *   3. SD 挂载点必须是 /mnt（不是 JFFS2 根分区）。用
 *      dfs_filesystem_lookup() 判断，避免卡不在时把照片写进 flash。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <pthread.h>

#include <dfs_fs.h>
#include <rtdevice.h>

#include "config.h"

/* plat_mmc.c（base OS 库）导出：让 mmcsd 重新识别卡并重建块设备。
 * 卡卡在数据态时只复位控制器是不够的，必须重新走 CMD0/ACMD41。*/
extern int fh_mmc_card_reinit(void);
#include "stream/include/mjpeg_cache.h"
#include "stream/include/snap_store.h"
#include "stream/include/snap_time.h"
#include "stream/include/stats.h"
#include "stream/include/clip_store.h"
#include "stream/include/rec_store.h"
#include "libsample_common/include/libsample_common.h"

static pthread_mutex_t g_sd_lock   = PTHREAD_MUTEX_INITIALIZER;
static int             g_clip_open;       /* 正在流式播放/下载的 AVI 个数，>0 时不能 remount */
static pthread_mutex_t g_ring_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_ev_lock   = PTHREAD_MUTEX_INITIALIZER;

static snap_event_t g_ring[SNAP_EVENT_RING_NUM];
static int          g_ring_count = 0;
static int          g_ring_head  = 0;   /* 下一个写入位置 */
static unsigned     g_seq        = 0;
static unsigned     g_dropped    = 0;
static int          g_sd_ok      = -1;
static int          g_dir_ok     = 0;    /* /mnt/snap 是否已确认存在 */

static unsigned char *g_ev_buf       = NULL;
static unsigned char *g_ev_buf_clean = NULL;
static int            g_ev_len       = 0;
static int            g_ev_len_clean = 0;
static int            g_ev_kind      = 0;
static int            g_ev_rule      = 0;
static int            g_ev_mode      = 0;
static int            g_ev_conf      = 0;
static int            g_ev_count     = 0;
static unsigned       g_ev_ms        = 0;
static unsigned       g_ev_epoch     = 0;
static volatile int   g_ev_pending = 0;
static volatile int   g_ev_busy    = 0;
static volatile int   g_ev_stop    = 0;
static unsigned       g_ev_last_ms = 0;
static volatile int   g_inited     = 0;
static volatile int   g_auto_save  = 0;   /* 默认关闭：上电不再自动存事件图 */
static pthread_t      g_ev_thread;

/* 测试模式：默认关；NNA 线程更新检测值，事件线程负责每秒落一行 CSV。*/
static volatile int   g_test_mode    = 0;
static unsigned       g_test_last_ms = 0;
static volatile int   g_det_count    = 0;
static volatile int   g_det_conf     = 0;

/* ------------------------------------------------------------------ */
/* SD                                                                  */
/* ------------------------------------------------------------------ */

/* 只有真正挂在 /mnt 的 elm(FAT) 才算可用。
 * 这里只查挂载表，不做任何磁盘 I/O：网页每秒轮询 /api/status 会调到这里，
 * 以前每次 mkdir() 都要碰 FAT，一次偶发超时就会把 SD 控制器拖死。*/
static int sd_probe(void)
{
    struct dfs_filesystem *fs = dfs_filesystem_lookup(SNAP_DIR);

    if (fs == NULL || fs->path == NULL || strcmp(fs->path, "/mnt") != 0)
    {
        if (g_sd_ok != 0)
        {
            printf("[snap_store] SD not mounted at /mnt\n");
        }
        g_sd_ok = 0;
        return 0;
    }

    g_sd_ok = 1;
    return 1;
}

/* 写文件前调用一次；调用者必须持有 g_sd_lock。
 * 目录已存在时 mkdir 返回 EEXIST，视为成功。*/
static int sd_ensure_dir_locked(void)
{
    if (!sd_probe())
        return 0;

    if (g_dir_ok)
        return 1;

    if (mkdir(SNAP_DIR, 0777) != 0 && errno != EEXIST)
    {
        printf("[snap_store] mkdir %s failed errno=%d\n", SNAP_DIR, errno);
        return 0;
    }

    if (mkdir(SNAP_DIR_LEFT, 0777) != 0 && errno != EEXIST)
    {
        printf("[snap_store] mkdir %s failed errno=%d\n", SNAP_DIR_LEFT, errno);
    }
    if (mkdir(SNAP_DIR_RIGHT, 0777) != 0 && errno != EEXIST)
    {
        printf("[snap_store] mkdir %s failed errno=%d\n", SNAP_DIR_RIGHT, errno);
    }

    g_dir_ok = 1;
    return g_dir_ok;
}

int snap_store_sd_ready(void)
{
    int ret;

    pthread_mutex_lock(&g_sd_lock);
    ret = sd_probe();
    pthread_mutex_unlock(&g_sd_lock);
    return ret;
}

void snap_store_auto_set(int on)
{
    g_auto_save = on ? 1 : 0;
}

int snap_store_auto_get(void)
{
    return g_auto_save;
}

void snap_store_test_set(int on)
{
    g_test_mode    = on ? 1 : 0;
    g_test_last_ms = 0;
}

int snap_store_test_get(void)
{
    return g_test_mode;
}

void snap_store_test_update(int count, int conf)
{
    g_det_count = count;
    g_det_conf  = conf;
}

/* MJPEG 一帧应当是完整 JPEG：FF D8 开头，FF D9 结尾。*/
static int jpeg_complete(const unsigned char *p, int n)
{
    return p != NULL && n >= 4 &&
           p[0] == 0xFF && p[1] == 0xD8 &&
           p[n - 2] == 0xFF && p[n - 1] == 0xD9;
}

static int write_file(const char *path, const unsigned char *data, int len)
{
    int fd;
    int off = 0;

    /* 调用者（save_frames）已持有 g_sd_lock */
    if (!sd_ensure_dir_locked())
        return -1;

    /* 先看源数据本身是不是完整 JPEG。如果这里就缺 EOI，底部灰条不是 SD
     * 或 HTTP 的问题，而是 MJPEG 帧在落盘前就短了。*/
    if (len < 4)
    {
        printf("[snap_store] jpeg too short: %s len=%d\n", path, len);
    }
    else if (data[0] != 0xFF || data[1] != 0xD8 ||
             data[len - 2] != 0xFF || data[len - 1] != 0xD9)
    {
        printf("[snap_store] jpeg tail missing: %s len=%d head=%02x%02x tail=%02x%02x\n",
               path, len, (unsigned)data[0], (unsigned)data[1],
               (unsigned)data[len - 2], (unsigned)data[len - 1]);
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        printf("[snap_store] open %s failed errno=%d\n", path, errno);
        return -1;
    }

    while (off < len)
    {
        int n = write(fd, data + off, len - off);
        if (n <= 0)
        {
            printf("[snap_store] write %s failed off=%d n=%d errno=%d\n",
                   path, off, n, errno);
            close(fd);
            unlink(path);
            return -1;
        }
        off += n;
    }

    if (fsync(fd) != 0)
    {
        printf("[snap_store] fsync %s failed errno=%d\n", path, errno);
        close(fd);
        unlink(path);
        return -1;
    }
    if (close(fd) != 0)
    {
        printf("[snap_store] close %s failed errno=%d\n", path, errno);
        unlink(path);
        return -1;
    }

    /* 驱动层的数据 CRC 错误以前会被静默吞掉；这里再用文件长度兜底，
     * 避免把缺块的图片记成“已存证”。*/
    {
        struct stat st;

        if (stat(path, &st) != 0)
        {
            printf("[snap_store] stat %s failed errno=%d\n", path, errno);
            unlink(path);
            return -1;
        }
        if (st.st_size != (off_t)len)
        {
            printf("[snap_store] size verify %s failed (%ld/%d)\n",
                   path, (long)st.st_size, len);
            unlink(path);
            return -1;
        }
    }

    /* 长度对不代表内容对：整文件读回逐段比对，能抓住“最后一个簇没写进去”。*/
    {
        unsigned char vbuf[512];
        int           rfd;
        int           n;

        rfd = open(path, O_RDONLY);
        if (rfd < 0)
        {
            printf("[snap_store] verify open %s failed errno=%d\n", path, errno);
            unlink(path);
            return -1;
        }

        off = 0;
        while (off < len)
        {
            int want = len - off;

            if (want > (int)sizeof(vbuf))
                want = (int)sizeof(vbuf);

            n = read(rfd, vbuf, (size_t)want);
            if (n != want || memcmp(vbuf, data + off, (size_t)want) != 0)
            {
                printf("[snap_store] verify %s failed off=%d n=%d\n",
                       path, off, n);
                close(rfd);
                unlink(path);
                return -1;
            }
            off += n;
        }
        close(rfd);
    }

    return 0;
}

static void append_log(unsigned ms, unsigned seq, unsigned epoch,
                       int kind, int mode, int rule, int conf, int count,
                       int ok, int clean_ok, unsigned size,
                       const char *file, const char *clean)
{
    char line[384];
    char tbuf[20];
    int  n;
    int  fd;

    snap_time_text(epoch, tbuf, sizeof(tbuf));
    n = snprintf(line, sizeof(line),
                 "time=%s ms=%u seq=%u kind=%d mode=%d rule=%d conf=%d count=%d "
                 "size=%u ok=%d clean_ok=%d file=%s clean=%s\n",
                 tbuf, ms, seq, kind, mode, rule, conf, count,
                 size, ok, clean_ok, file,
                 (clean != NULL) ? clean : "");
    if (n <= 0)
    {
        return;
    }
    if (n >= (int)sizeof(line))
    {
        n = (int)sizeof(line) - 1;
    }

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_ensure_dir_locked())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return;
    }
    fd = open(SNAP_DIR "/events.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        write(fd, line, n);
        close(fd);
    }
    pthread_mutex_unlock(&g_sd_lock);
}

/* ------------------------------------------------------------------ */
/* 事件环                                                              */
/* ------------------------------------------------------------------ */

static void ring_add(unsigned seq, unsigned ms, unsigned epoch,
                     int kind, int mode, int rule, int conf, int count,
                     int ok, int clean_ok, unsigned size,
                     const char *file, const char *clean)
{
    snap_event_t *e;

    pthread_mutex_lock(&g_ring_lock);

    e = &g_ring[g_ring_head];
    memset(e, 0, sizeof(*e));
    e->seq      = seq;
    e->ms       = ms;
    e->epoch    = epoch;
    e->kind     = (unsigned)kind;
    e->mode     = (unsigned)mode;
    e->rule     = rule;
    e->conf     = (unsigned)conf;
    e->count    = (unsigned)count;
    e->ok       = (unsigned)(ok ? 1 : 0);
    e->clean_ok = (unsigned)(clean_ok ? 1 : 0);
    e->label    = SNAP_LABEL_NONE;
    e->size     = size;
    snap_time_text(epoch, e->time, sizeof(e->time));
    if (file != NULL)
    {
        strncpy(e->file, file, sizeof(e->file) - 1);
    }
    if (clean != NULL)
    {
        strncpy(e->clean, clean, sizeof(e->clean) - 1);
    }

    g_ring_head = (g_ring_head + 1) % SNAP_EVENT_RING_NUM;
    if (g_ring_count < SNAP_EVENT_RING_NUM)
    {
        g_ring_count++;
    }

    pthread_mutex_unlock(&g_ring_lock);
}

/* 删除文件后清掉事件环里对它的引用；AI 图和干净图都要检查。*/
static void ring_forget(const char *path)
{
    int i;

    if (path == NULL || path[0] == '\0')
    {
        return;
    }

    pthread_mutex_lock(&g_ring_lock);
    for (i = 0; i < g_ring_count; i++)
    {
        if (strcmp(g_ring[i].file, path) == 0)
        {
            g_ring[i].ok = 0;
            g_ring[i].file[0] = '\0';
        }
        if (strcmp(g_ring[i].clean, path) == 0)
        {
            g_ring[i].clean[0] = '\0';
        }
    }
    pthread_mutex_unlock(&g_ring_lock);
}

int snap_store_events(snap_event_t *out, int max)
{
    int i;
    int n = 0;

    if (out == NULL || max <= 0)
    {
        return 0;
    }

    pthread_mutex_lock(&g_ring_lock);
    for (i = 0; i < g_ring_count && n < max; i++)
    {
        int idx = (g_ring_head - 1 - i + SNAP_EVENT_RING_NUM) % SNAP_EVENT_RING_NUM;
        out[n++] = g_ring[idx];
    }
    pthread_mutex_unlock(&g_ring_lock);

    return n;
}

int snap_store_event_count(void)
{
    int n;

    pthread_mutex_lock(&g_ring_lock);
    n = g_ring_count;
    pthread_mutex_unlock(&g_ring_lock);
    return n;
}

unsigned snap_store_dropped(void)
{
    unsigned n;

    pthread_mutex_lock(&g_ring_lock);
    n = g_dropped;
    pthread_mutex_unlock(&g_ring_lock);
    return n;
}

void snap_store_set_event_clip(unsigned seq, const char *clip,
                               unsigned size, int ok)
{
    int i;

    pthread_mutex_lock(&g_ring_lock);
    for (i = 0; i < SNAP_EVENT_RING_NUM; i++)
    {
        snap_event_t *e = &g_ring[i];

        if (e->seq != seq)
        {
            continue;
        }

        e->clip_size = size;
        e->clip_ok = (unsigned)(ok ? 1 : 0);
        e->clip[0] = '\0';
        if (clip != NULL && clip[0] != '\0')
        {
            strncpy(e->clip, clip, sizeof(e->clip) - 1);
        }
        break;
    }
    pthread_mutex_unlock(&g_ring_lock);
}

/* ------------------------------------------------------------------ */
/* 写一帧 JPEG                                                        */
/* ------------------------------------------------------------------ */

/* 时间同步过就用 snap_20261024_163012_00012.jpg；
 * 没同步过就退回原来的开机毫秒命名，保证抓拍仍然能用。*/
/* 摄像头 0=左摄(G0)，1=右摄(G1)；落到各自子目录。*/
static const char *cam_dir(int cam)
{
    return (cam == 0) ? SNAP_DIR_LEFT : SNAP_DIR_RIGHT;
}

static void make_snap_path(char *path, int path_sz, int cam, int kind, int rule,
                           unsigned seq, unsigned ms, unsigned epoch,
                           const char *suffix)
{
    char date[9];
    char clk[7];
    int  synced;

    if (suffix == NULL)
    {
        suffix = "";
    }

    synced = snap_time_name_at(epoch, date, sizeof(date), clk, sizeof(clk));

    if (kind == SNAP_EV_MANUAL)
    {
        if (synced)
        {
            snprintf(path, path_sz, "%s/snap_%s_%s_%05u%s.jpg",
                     cam_dir(cam), date, clk, seq, suffix);
        }
        else
        {
            snprintf(path, path_sz, "%s/snap_%010u_%05u%s.jpg",
                     cam_dir(cam), ms, seq, suffix);
        }
    }
    else
    {
        if (synced)
        {
            snprintf(path, path_sz, "%s/evt_%s_%s_k%d_r%d_%05u%s.jpg",
                     cam_dir(cam), date, clk, kind, rule, seq, suffix);
        }
        else
        {
            snprintf(path, path_sz, "%s/evt_%010u_%05u_k%d_r%d%s.jpg",
                     cam_dir(cam), ms, seq, kind, rule, suffix);
        }
    }
}

static void make_clip_path(char *path, int path_sz, int cam, int kind, int rule,
                           unsigned seq, unsigned ms, unsigned epoch)
{
    char date[9];
    char clk[7];

    if (snap_time_name_at(epoch, date, sizeof(date), clk, sizeof(clk)))
    {
        snprintf(path, path_sz, "%s/evt_%s_%s_k%d_r%d_%05u_clip.avi",
                 cam_dir(cam), date, clk, kind, rule, seq);
    }
    else
    {
        snprintf(path, path_sz, "%s/evt_%010u_%05u_k%d_r%d_clip.avi",
                 cam_dir(cam), ms, seq, kind, rule);
    }
}

/*
 * 保存一帧或一对帧（AI + 干净）。clean 为 NULL/0 时只存 AI。
 * 事件双路存证时 ai 路径带 _ai，clean 路径带 _clean。
 */
static int save_frames(const unsigned char *ai, int ai_len,
                       const unsigned char *clean, int clean_len,
                       int kind, int cam, int mode, int rule, int conf, int count,
                       unsigned ms, unsigned epoch,
                       char *ai_path, int ai_cap,
                       char *clean_path, int clean_cap,
                       unsigned *seq_out)
{
    unsigned seq;
    int      ret_ai = -1;
    int      ret_clean = -1;

    if (ai == NULL || ai_len <= 0 || ai_path == NULL || ai_cap <= 0)
    {
        return -4;
    }

    pthread_mutex_lock(&g_ring_lock);
    seq = ++g_seq;
    pthread_mutex_unlock(&g_ring_lock);

    if (seq_out != NULL)
    {
        *seq_out = seq;
    }

    make_snap_path(ai_path, ai_cap, cam, kind, rule, seq, ms, epoch,
                   (clean_len > 0) ? "_ai" : "");

    if (clean_path != NULL && clean_cap > 0)
    {
        clean_path[0] = '\0';
        if (clean_len > 0 && clean != NULL)
        {
            /* 干净/另一路图来自另一只摄像头，落到另一个子目录 */
            make_snap_path(clean_path, clean_cap, cam ^ 1, kind, rule, seq, ms,
                           epoch, "_clean");
        }
    }

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        ring_add(seq, ms, epoch, kind, mode, rule, conf, count,
                 0, 0, (unsigned)ai_len, "", "");
        return -3;
    }

    ret_ai = write_file(ai_path, ai, ai_len);
    if (clean_len > 0 && clean != NULL &&
        clean_path != NULL && clean_path[0] != '\0')
    {
        ret_clean = write_file(clean_path, clean, clean_len);
    }
    g_sd_ok = (ret_ai == 0);
    pthread_mutex_unlock(&g_sd_lock);

    ring_add(seq, ms, epoch, kind, mode, rule, conf, count,
             (ret_ai == 0), (ret_clean == 0), (unsigned)ai_len,
             (ret_ai == 0) ? ai_path : "",
             (ret_clean == 0) ? clean_path : "");

    if (ret_ai == 0)
    {
        append_log(ms, seq, epoch, kind, mode, rule, conf, count,
                   1, (ret_clean == 0), (unsigned)ai_len,
                   ai_path, (ret_clean == 0) ? clean_path : "");
    }

    return (ret_ai == 0) ? 0 : -4;
}

/* 测试模式：每秒一行检测快照，用来统计“有人的帧里模型有没有检出”。*/
static void append_test_line(unsigned ms)
{
    char     line[128];
    char     tbuf[20];
    unsigned epoch = 0;
    int      fd;
    int      n;
    struct stat st;

    snap_time_get(&epoch);
    snap_time_text(epoch, tbuf, sizeof(tbuf));
    n = snprintf(line, sizeof(line), "\"%s\",%u,%d,%d\n",
                 tbuf, ms, g_det_count, g_det_conf);
    if (n <= 0)
        return;

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_ensure_dir_locked())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return;
    }

    fd = open(SNAP_DIR "/detect.csv", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        if (stat(SNAP_DIR "/detect.csv", &st) == 0 && st.st_size == 0)
        {
            const char *hdr = "time,ms,count,conf\n";
            write(fd, hdr, strlen(hdr));
        }
        write(fd, line, n);
        close(fd);
    }
    pthread_mutex_unlock(&g_sd_lock);
}

/* ------------------------------------------------------------------ */
/* 事件线程                                                            */
/* ------------------------------------------------------------------ */

static void *snap_event_thread(void *arg)
{
    (void)arg;

    while (!g_ev_stop)
    {
        int      pending;
        int      len;
        int      len_clean;
        int      kind;
        int      mode;
        int      rule;
        int      conf;
        int      count;
        unsigned ms;
        unsigned epoch;

        pthread_mutex_lock(&g_ev_lock);
        pending = g_ev_pending;
        if (pending)
        {
            g_ev_pending = 0;
            g_ev_busy    = 1;
        }
        len       = g_ev_len;
        len_clean = g_ev_len_clean;
        kind      = g_ev_kind;
        mode      = g_ev_mode;
        rule      = g_ev_rule;
        conf      = g_ev_conf;
        count     = g_ev_count;
        ms        = g_ev_ms;
        epoch     = g_ev_epoch;
        pthread_mutex_unlock(&g_ev_lock);

        if (pending)
        {
            char     path[64];
            char     clean_path[64];
            unsigned seq = 0;
            int      tries;
            int      clip_started;

            /* 触发瞬间如果缓存里的帧刚好不完整（缺 EOI，底部会灰），
             * 等下一帧再取一次；最多等 ~160ms。*/
            if (!jpeg_complete(g_ev_buf, len))
            {
                int idx_ai = mjpeg_cache_index_of(1, 1);

                for (tries = 0; tries < 4 && idx_ai >= 0; tries++)
                {
                    int n;

                    usleep(40 * 1000);
                    n = mjpeg_cache_copy(idx_ai, g_ev_buf, SNAP_JPEG_MAX);
                    if (n > 0)
                        len = n;
                    if (jpeg_complete(g_ev_buf, len))
                        break;
                }
                printf("[snap_store] ai frame retry len=%d complete=%d\n",
                       len, jpeg_complete(g_ev_buf, len));
            }

            if (len_clean > 0 && g_ev_buf_clean != NULL &&
                !jpeg_complete(g_ev_buf_clean, len_clean))
            {
                int idx_cl = mjpeg_cache_index_of(0, 1);

                for (tries = 0; tries < 4 && idx_cl >= 0; tries++)
                {
                    int n;

                    usleep(40 * 1000);
                    n = mjpeg_cache_copy(idx_cl, g_ev_buf_clean, SNAP_JPEG_MAX);
                    if (n > 0)
                        len_clean = n;
                    if (jpeg_complete(g_ev_buf_clean, len_clean))
                        break;
                }
            }

            /* 只有绊线（有人左→右 / 右→左通过）才录前后 5 秒视频；
             * 周界停留（kind=1）只拍照取证，不生成视频。*/
            /* 定时/手动录像期间不再录事件短视频：录像本身已经在录，
             * 两个 AVI 同时写 SD 会互相拖累。截图和播报不受影响。*/
            clip_started = (kind == SNAP_EV_TRIP && !rec_store_is_recording())
                               ? clip_store_begin(ms) : 0;

            save_frames(g_ev_buf, len,
                        (len_clean > 0) ? g_ev_buf_clean : NULL, len_clean,
                        kind, 1, mode, rule, conf, count, ms, epoch,
                        path, sizeof(path),
                        clean_path, sizeof(clean_path), &seq);
            printf("[snap_store] event seq=%u kind=%d mode=%d rule=%d conf=%d count=%d file=%s clean=%s\n",
                   seq, kind, mode, rule, conf, count, path, clean_path);

            if (clip_started)
            {
                if (seq > 0)
                {
                    char     clip_path[64];
                    unsigned clip_size = 0;
                    int      clip_ok;

                    make_clip_path(clip_path, sizeof(clip_path), 1, kind, rule,
                                   seq, ms, epoch);
                    clip_store_wait_end();
                    clip_ok = (clip_store_finish(clip_path, sizeof(clip_path),
                                                 &clip_size, NULL, NULL) == 0);
                    snap_store_set_event_clip(seq, clip_ok ? clip_path : "",
                                              clip_size, clip_ok);
                    printf("[snap_store] clip seq=%u ok=%d size=%u file=%s\n",
                           seq, clip_ok, clip_size, clip_ok ? clip_path : "");
                }
                else
                {
                    clip_store_abort();
                }
            }

            pthread_mutex_lock(&g_ev_lock);
            g_ev_busy = 0;
            pthread_mutex_unlock(&g_ev_lock);
        }

        if (g_test_mode)
        {
            unsigned now = (unsigned)get_ms();
            if (g_test_last_ms == 0 || (now - g_test_last_ms) >= 1000)
            {
                append_test_line(now);
                g_test_last_ms = now;
            }
        }

        usleep(50 * 1000);      /* 50ms 轮询；事件延迟可接受，CPU 几乎为 0 */
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* 对外接口                                                            */
/* ------------------------------------------------------------------ */

int snap_store_init(void)
{
    pthread_attr_t attr;

    if (g_inited)
    {
        return 0;
    }

    memset(g_ring, 0, sizeof(g_ring));
    g_ring_count = 0;
    g_ring_head  = 0;
    g_seq        = 0;
    g_dropped    = 0;
    g_ev_len       = 0;
    g_ev_len_clean = 0;
    g_ev_kind      = 0;
    g_ev_mode      = 0;
    g_ev_rule      = 0;
    g_ev_conf      = 0;
    g_ev_count     = 0;
    g_ev_ms        = 0;
    g_ev_epoch     = 0;
    g_ev_pending   = 0;
    g_ev_busy    = 0;
    g_ev_stop    = 0;
    g_ev_last_ms = 0;
    g_sd_ok      = -1;
    g_dir_ok     = 0;
    g_test_mode    = 0;
    g_test_last_ms = 0;
    g_det_count    = 0;
    g_det_conf     = 0;

    g_ev_buf = (unsigned char *)malloc(SNAP_JPEG_MAX);
    if (g_ev_buf == NULL)
    {
        printf("[snap_store] malloc %d failed\n", SNAP_JPEG_MAX);
        return -1;
    }

    /* 第二路干净画面。分配失败不影响主流程，事件只存 AI 图。*/
    g_ev_buf_clean = (unsigned char *)malloc(SNAP_JPEG_MAX);
    if (g_ev_buf_clean == NULL)
    {
        printf("[snap_store] malloc clean buffer failed, dual save disabled\n");
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#ifdef __RTTHREAD_OS__
    {
        struct sched_param param;
        param.sched_priority = 140;
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setstacksize(&attr, 16 * 1024);
    }
#endif

    if (pthread_create(&g_ev_thread, &attr, snap_event_thread, NULL) != 0)
    {
        printf("[snap_store] create event thread failed\n");
        free(g_ev_buf);
        g_ev_buf = NULL;
        free(g_ev_buf_clean);
        g_ev_buf_clean = NULL;
        return -1;
    }

    stats_init();

    g_inited = 1;
    printf("[snap_store] init, dir=%s, cooldown=%dms\n",
           SNAP_DIR, SNAP_EVENT_COOLDOWN_MS);
    return 0;
}

void snap_store_deinit(void)
{
    if (!g_inited)
    {
        return;
    }

    g_ev_stop = 1;
    pthread_join(g_ev_thread, NULL);
    g_inited = 0;

    if (g_ev_buf != NULL)
    {
        free(g_ev_buf);
        g_ev_buf = NULL;
    }
    if (g_ev_buf_clean != NULL)
    {
        free(g_ev_buf_clean);
        g_ev_buf_clean = NULL;
    }
}

void snap_store_notify_event(int kind, int rule_id, int mode, int conf, int count)
{
    unsigned now;
    unsigned epoch = 0;
    unsigned cooldown;
    int      idx;
    int      n;
    int      idx_clean;
    int      n_clean = 0;

    if (!g_inited)
    {
        pthread_mutex_lock(&g_ring_lock);
        g_dropped++;
        pthread_mutex_unlock(&g_ring_lock);
        return;
    }

    /* 自动抓拍关闭时，检测照常跑，只是不复制帧、不写 SD。
     * 手动抓拍走 snap_store_manual()，不受这里影响。*/
    if (!g_auto_save)
    {
        return;
    }

    now = (unsigned)get_ms();
    (void)snap_time_get(&epoch);

    cooldown = g_test_mode ? SNAP_TEST_COOLDOWN_MS : SNAP_EVENT_COOLDOWN_MS;

    pthread_mutex_lock(&g_ev_lock);
    if (g_ev_busy || g_ev_pending ||
        (g_ev_last_ms != 0 && (now - g_ev_last_ms) < cooldown))
    {
        pthread_mutex_unlock(&g_ev_lock);
        pthread_mutex_lock(&g_ring_lock);
        g_dropped++;
        pthread_mutex_unlock(&g_ring_lock);
        return;
    }
    pthread_mutex_unlock(&g_ev_lock);

    /* 复制触发时刻的最新 AI 帧。NNA 是唯一写者，复制期间不会被覆盖。*/
    idx = mjpeg_cache_index_of(1, 1);
    if (idx < 0)
    {
        goto drop;
    }

    n = mjpeg_cache_copy(idx, g_ev_buf, SNAP_JPEG_MAX);
    if (n <= 0)
    {
        goto drop;
    }

    /* G0 干净画面：拿不到就只存 AI 图，不影响事件本身。*/
    idx_clean = mjpeg_cache_index_of(0, 1);
    if (idx_clean >= 0 && g_ev_buf_clean != NULL)
    {
        n_clean = mjpeg_cache_copy(idx_clean, g_ev_buf_clean, SNAP_JPEG_MAX);
        if (n_clean <= 0)
        {
            n_clean = 0;
        }
    }

    pthread_mutex_lock(&g_ev_lock);
    g_ev_len       = n;
    g_ev_len_clean = n_clean;
    g_ev_kind      = kind;
    g_ev_mode      = mode;
    g_ev_rule      = rule_id;
    g_ev_conf      = conf;
    g_ev_count     = count;
    g_ev_ms        = now;
    g_ev_epoch     = epoch;
    g_ev_last_ms   = now;
    g_ev_pending   = 1;
    pthread_mutex_unlock(&g_ev_lock);
    return;

drop:
    pthread_mutex_lock(&g_ring_lock);
    g_dropped++;
    pthread_mutex_unlock(&g_ring_lock);
}

int snap_store_manual(int cam, unsigned char *out, int cap, int save,
                      char *path, int path_sz, unsigned *seq)
{
    int      idx;
    int      n;
    int      ret;
    unsigned epoch = 0;

    idx = mjpeg_cache_index_of(cam ? 1 : 0, 1);
    if (idx < 0)
    {
        return -1;
    }

    n = mjpeg_cache_copy(idx, out, cap);
    if (n <= 0)
    {
        return n;               /* -1 无帧 / -2 缓冲太小 */
    }

    if (!save)
    {
        return n;
    }

    (void)snap_time_get(&epoch);
    ret = save_frames(out, n, NULL, 0, SNAP_EV_MANUAL, cam, 0, 0, 0, 0,
                      (unsigned)get_ms(), epoch,
                      path, path_sz, NULL, 0, seq);
    if (ret != 0)
    {
        return ret;             /* -3 / -4 */
    }

    return n;
}

int snap_store_read_event(unsigned seq, unsigned char *buf, int cap)
{
    char path[64];
    int  found = 0;
    int  i;
    int  fd;
    int  off = 0;

    if (buf == NULL || cap <= 0)
    {
        return -1;
    }

    pthread_mutex_lock(&g_ring_lock);
    for (i = 0; i < g_ring_count; i++)
    {
        int idx = (g_ring_head - 1 - i + SNAP_EVENT_RING_NUM) % SNAP_EVENT_RING_NUM;
        if (g_ring[idx].seq == seq && g_ring[idx].ok && g_ring[idx].file[0] != '\0')
        {
            strncpy(path, g_ring[idx].file, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_ring_lock);

    if (!found)
    {
        return -1;
    }

    pthread_mutex_lock(&g_sd_lock);
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    while (off < cap)
    {
        int n = read(fd, buf + off, cap - off);
        if (n <= 0)
        {
            break;
        }
        off += n;
    }
    close(fd);
    pthread_mutex_unlock(&g_sd_lock);

    return (off > 0) ? off : -1;
}

/* ------------------------------------------------------------------ */
/* 相册：列目录 / 按文件名读取                                        */
/* ------------------------------------------------------------------ */

#define SNAP_GAL_MAX 256

typedef struct
{
    char               name[SNAP_FILE_NAME_MAX];
    unsigned           size;
    unsigned long long key;     /* 文件名里的时间，越大越新 */
} snap_gal_ent_t;

/* 只在 8080 单线程里用；静态存放，避免 8K 服务线程栈溢出。 */
static snap_gal_ent_t g_gal[SNAP_GAL_MAX];

static int gal_is_jpg(const char *name)
{
    size_t n = strlen(name);

    if (n < 5)
    {
        return 0;
    }

    return name[n - 4] == '.' &&
           tolower((unsigned char)name[n - 3]) == 'j' &&
           tolower((unsigned char)name[n - 2]) == 'p' &&
           tolower((unsigned char)name[n - 1]) == 'g';
}

static int gal_is_avi(const char *name)
{
    size_t n = strlen(name);

    if (n < 5)
    {
        return 0;
    }

    return name[n - 4] == '.' &&
           tolower((unsigned char)name[n - 3]) == 'a' &&
           tolower((unsigned char)name[n - 2]) == 'v' &&
           tolower((unsigned char)name[n - 1]) == 'i';
}

static int gal_is_media(const char *name)
{
    return gal_is_jpg(name) || gal_is_avi(name);
}

/*
 * 从文件名取排序键。
 * 新格式 snap_20261024_163012_00012[_ai].jpg -> 20261024163012
 * 旧格式 snap_0000123456_00001.jpg           -> 123456（开机毫秒）
 * 新格式的键始终大于旧格式，新照片排前面。
 */
static unsigned long long gal_name_key(const char *name)
{
    const char *p = strchr(name, '_');
    const char *q;
    int         nd = 0;

    if (p == NULL)
    {
        return 0;
    }
    p++;

    q = p;
    while (q[nd] >= '0' && q[nd] <= '9')
    {
        nd++;
    }

    if (nd == 8 && q[8] == '_')
    {
        unsigned long long date = 0;
        unsigned long long clk = 0;
        int                i;

        for (i = 0; i < 8; i++)
        {
            date = date * 10 + (unsigned)(q[i] - '0');
        }
        for (i = 0; i < 6; i++)
        {
            char c = q[9 + i];
            if (c < '0' || c > '9')
            {
                return date;
            }
            clk = clk * 10 + (unsigned)(c - '0');
        }
        return date * 1000000ULL + clk;
    }

    return strtoull(q, NULL, 10);
}

/* 文件名时间新的在前；同一秒再按名字倒序。 */
static int gal_cmp(const void *a, const void *b)
{
    const snap_gal_ent_t *x = (const snap_gal_ent_t *)a;
    const snap_gal_ent_t *y = (const snap_gal_ent_t *)b;

    if (x->key != y->key)
    {
        return (x->key < y->key) ? 1 : -1;
    }
    return -strcmp(x->name, y->name);
}

/* 允许 SNAP_DIR 根下的纯文件名，或 left/、right/ 两个子目录里的文件名；
 * 挡掉 ../ 和绝对路径。*/
static int gal_safe_media(const char *name)
{
    const char *base;
    size_t      n;
    const char *p;

    if (name == NULL || name[0] == '\0')
    {
        return 0;
    }

    if (strncmp(name, "left/", 5) == 0)
    {
        base = name + 5;
    }
    else if (strncmp(name, "right/", 6) == 0)
    {
        base = name + 6;
    }
    else if (strchr(name, '/') != NULL)
    {
        return 0;
    }
    else
    {
        base = name;
    }

    n = strlen(base);
    if (n < 5 || n >= SNAP_FILE_NAME_MAX)
    {
        return 0;
    }

    for (p = base; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
        {
            return 0;
        }
    }

    return gal_is_media(base);
}

static int gal_safe_name(const char *name)
{
    return gal_safe_media(name) && gal_is_jpg(name);
}

static int gal_safe_avi(const char *name)
{
    return gal_safe_media(name) && gal_is_avi(name);
}

static int gal_make_path(char *path, size_t cap, const char *name)
{
    size_t nlen = strlen(name);

    /* cap 是 SNAP_FILE_NAME_MAX + 16，调用前已保证 nlen < SNAP_FILE_NAME_MAX */
    if (cap <= sizeof(SNAP_DIR) + nlen)
    {
        path[0] = '\0';
        return 0;
    }

    memcpy(path, SNAP_DIR, sizeof(SNAP_DIR) - 1);
    path[sizeof(SNAP_DIR) - 1] = '/';
    memcpy(path + sizeof(SNAP_DIR), name, nlen + 1);
    return 1;
}

/* 调用前必须持有 g_sd_lock。单位 KB。*/
static void gal_space_locked(unsigned *free_kb, unsigned *total_kb)
{
    struct statfs      st;
    unsigned long long fb;
    unsigned long long tb;

    *free_kb  = 0;
    *total_kb = 0;

    if (statfs(SNAP_DIR, &st) != 0)
    {
        return;
    }

    fb = (unsigned long long)st.f_bfree * st.f_bsize;
    tb = (unsigned long long)st.f_blocks * st.f_bsize;
    *free_kb  = (unsigned)((fb + 1023) / 1024);
    *total_kb = (unsigned)((tb + 1023) / 1024);
}

/* 扫描一个目录；prefix 为空表示 SNAP_DIR 根（老文件），
 * 否则把 "left/" / "right/" 拼进相对文件名。返回新增数量。*/
static int gal_scan_dir_locked(const char *dir_path, const char *prefix, int base)
{
    DIR           *dir;
    struct dirent *de;
    struct stat    st;
    char           path[SNAP_FILE_NAME_MAX + 32];
    char           rel[SNAP_FILE_NAME_MAX];
    size_t         plen = strlen(prefix);
    int            total = 0;

    dir = opendir(dir_path);
    if (dir == NULL)
    {
        return (errno == ENOENT) ? 0 : -1;
    }

    while ((de = readdir(dir)) != NULL)
    {
        size_t nlen = strlen(de->d_name);

        if (nlen < 5 || plen + nlen >= sizeof(rel))
        {
            continue;
        }
        if (!gal_is_media(de->d_name))
        {
            continue;
        }

        snprintf(rel, sizeof(rel), "%s%s", prefix, de->d_name);

        if (!gal_make_path(path, sizeof(path), rel))
        {
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0)
        {
            continue;
        }

        if (base + total < SNAP_GAL_MAX)
        {
            snap_gal_ent_t *e = &g_gal[base + total];

            memcpy(e->name, rel, strlen(rel) + 1);
            e->size = (unsigned)st.st_size;
            e->key  = gal_name_key(rel);
            total++;
        }
    }

    closedir(dir);
    return total;
}

/* 调用前必须持有 g_sd_lock 且 sd_probe() 通过。返回文件数；-1 打开目录失败。*/
static int gal_scan_locked(void)
{
    int total = 0;
    int n;

    n = gal_scan_dir_locked(SNAP_DIR, "", total);
    if (n < 0)
    {
        return n;
    }
    total += n;

    n = gal_scan_dir_locked(SNAP_DIR_LEFT, "left/", total);
    if (n < 0)
    {
        return n;
    }
    total += n;

    n = gal_scan_dir_locked(SNAP_DIR_RIGHT, "right/", total);
    if (n < 0)
    {
        return n;
    }
    total += n;

    return total;
}

int snap_store_list_files(char *out, int cap, int max_files)
{
    int      total;
    int      count;
    int      off;
    int      i;

    if (out == NULL || cap <= 0)
    {
        return -1;
    }

    if (max_files <= 0)
    {
        max_files = 20;
    }
    if (max_files > SNAP_GAL_MAX)
    {
        max_files = SNAP_GAL_MAX;
    }

    pthread_mutex_lock(&g_sd_lock);

    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        off = snprintf(out, cap, "sd=0\n");
        return (off > 0) ? off : 0;
    }

    total = gal_scan_locked();
    pthread_mutex_unlock(&g_sd_lock);

    if (total < 0)
    {
        off = snprintf(out, cap, "sd=0\n");
        return (off > 0) ? off : 0;
    }

    qsort(g_gal, (size_t)total, sizeof(g_gal[0]), gal_cmp);

    count = total;
    if (count > max_files)
    {
        count = max_files;
    }

    off = snprintf(out, cap, "sd=1 total=%d\n", total);
    if (off < 0)
    {
        return 0;
    }

    for (i = 0; i < count && off < cap; i++)
    {
        int wrote = snprintf(out + off, (size_t)(cap - off),
                             "%s %u\n", g_gal[i].name, g_gal[i].size);

        if (wrote < 0 || wrote >= cap - off)
        {
            break;
        }
        off += wrote;
    }

    return off;
}

/* statfs 在 FAT32 上要扫 FAT 统计空闲簇，8GB 卡一次就是几 MB 的读。
 * 卡一旦超时，这次扫描会变成连续几秒/几十秒的 cmd:17 超时刷屏，
 * 把录像和网页全拖住。这里缓存 60 秒，点多次“容量”也只读一次。*/
static unsigned g_space_free_kb;
static unsigned g_space_total_kb;
static unsigned g_space_ms;
static int      g_space_valid;

int snap_store_space(unsigned *free_kb, unsigned *total_kb)
{
    int      ret = 0;
    unsigned now = (unsigned)get_ms();

    if (free_kb == NULL || total_kb == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_sd_lock);

    if (g_space_valid && (unsigned)(now - g_space_ms) < 60000u)
    {
        *free_kb  = g_space_free_kb;
        *total_kb = g_space_total_kb;
        pthread_mutex_unlock(&g_sd_lock);
        return 0;
    }

    if (!sd_probe())
    {
        ret = -1;
    }
    else
    {
        unsigned f = 0;
        unsigned t = 0;

        gal_space_locked(&f, &t);
        if (t == 0)
        {
            ret = -1;
        }
        else
        {
            g_space_free_kb  = f;
            g_space_total_kb = t;
            g_space_ms       = now;
            g_space_valid    = 1;
            *free_kb  = f;
            *total_kb = t;
        }
    }
    pthread_mutex_unlock(&g_sd_lock);

    return ret;
}

int snap_store_read_text(const char *name, char *out, int cap, int tail)
{
    char        path[SNAP_FILE_NAME_MAX + 16];
    struct stat st;
    int         fd;
    int         off = 0;

    if (name == NULL || out == NULL || cap <= 0 ||
        strchr(name, '/') != NULL || strchr(name, '\\') != NULL)
    {
        return -1;
    }

    snprintf(path, sizeof(path), SNAP_DIR "/%s", name);
    out[0] = '\0';

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }

    fd = open(path, O_RDONLY);
    if (fd >= 0)
    {
        if (tail && fstat(fd, &st) == 0 && st.st_size > cap - 1)
        {
            lseek(fd, st.st_size - (cap - 1), SEEK_SET);
        }
        while (off < cap - 1)
        {
            int n = read(fd, out + off, cap - 1 - off);
            if (n <= 0)
                break;
            off += n;
        }
        close(fd);
    }
    pthread_mutex_unlock(&g_sd_lock);

    out[off] = '\0';
    return off;
}

static int label_append(const char *line, int len)
{
    char        path[sizeof(SNAP_DIR) + 16];
    struct stat st;
    int         fd;

    if (line == NULL || len <= 0)
        return -1;

    snprintf(path, sizeof(path), SNAP_DIR "/labels.csv");

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_ensure_dir_locked())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    if (stat(path, &st) == 0 && st.st_size == 0)
    {
        const char *hdr =
            "time,ms,seq,label,kind,mode,rule,conf,count,clean_ok\n";
        write(fd, hdr, strlen(hdr));
    }
    write(fd, line, len);
    close(fd);
    pthread_mutex_unlock(&g_sd_lock);
    return 0;
}

int snap_store_label_event(unsigned seq, int label)
{
    snap_event_t e;
    unsigned     epoch = 0;
    unsigned     ms;
    char         tbuf[20];
    char         line[256];
    int          n;
    int          found = 0;
    int          i;

    label = label ? SNAP_LABEL_TP : SNAP_LABEL_FP;

    memset(&e, 0, sizeof(e));
    pthread_mutex_lock(&g_ring_lock);
    for (i = 0; i < g_ring_count; i++)
    {
        int idx = (g_ring_head - 1 - i + SNAP_EVENT_RING_NUM) % SNAP_EVENT_RING_NUM;
        if (g_ring[idx].seq == seq)
        {
            e = g_ring[idx];
            g_ring[idx].label = label;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_ring_lock);

    (void)found;
    ms = (unsigned)get_ms();
    snap_time_get(&epoch);
    snap_time_text(epoch, tbuf, sizeof(tbuf));

    n = snprintf(line, sizeof(line),
                 "\"%s\",%u,%u,%d,%u,%u,%d,%u,%u,%u\n",
                 tbuf, ms, seq, label,
                 (unsigned)e.kind, (unsigned)e.mode, e.rule,
                 (unsigned)e.conf, (unsigned)e.count, (unsigned)e.clean_ok);
    if (n <= 0)
        return -1;
    return label_append(line, n);
}

int snap_store_label_miss(void)
{
    unsigned epoch = 0;
    unsigned ms = (unsigned)get_ms();
    char     tbuf[20];
    char     line[128];
    int      n;

    snap_time_get(&epoch);
    snap_time_text(epoch, tbuf, sizeof(tbuf));
    n = snprintf(line, sizeof(line), "\"%s\",%u,0,miss,0,0,0,0,0,0\n",
                 tbuf, ms);
    if (n <= 0)
        return -1;
    return label_append(line, n);
}

int snap_store_selftest(char *msg, int cap)
{
    unsigned char buf[1024];
    char          path[sizeof(SNAP_DIR) + 24];
    int           i, j;
    int           fd;
    int           ok = 0;
    int           wrote;

    if (msg == NULL || cap <= 0)
        return -1;
    msg[0] = '\0';

    snprintf(path, sizeof(path), SNAP_DIR "/.selftest.bin");

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_ensure_dir_locked())
    {
        snprintf(msg, cap, "ok=0 err=sd_not_ready");
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        snprintf(msg, cap, "ok=0 err=open errno=%d", errno);
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    for (i = 0; i < 64; i++)
    {
        for (j = 0; j < (int)sizeof(buf); j++)
            buf[j] = (unsigned char)((i + j) & 0xff);
        wrote = 0;
        while (wrote < (int)sizeof(buf))
        {
            int n = write(fd, buf + wrote, sizeof(buf) - wrote);
            if (n <= 0)
            {
                snprintf(msg, cap, "ok=0 err=write errno=%d", errno);
                close(fd);
                unlink(path);
                pthread_mutex_unlock(&g_sd_lock);
                return -1;
            }
            wrote += n;
        }
    }
    if (fsync(fd) != 0)
    {
        snprintf(msg, cap, "ok=0 err=fsync errno=%d", errno);
        close(fd);
        unlink(path);
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }
    close(fd);

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        snprintf(msg, cap, "ok=0 err=reopen errno=%d", errno);
        unlink(path);
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    for (i = 0; i < 64 && ok == 0; i++)
    {
        int off = 0;
        while (off < (int)sizeof(buf))
        {
            int n = read(fd, buf + off, sizeof(buf) - off);
            if (n <= 0)
            {
                ok = -1;
                break;
            }
            off += n;
        }
        for (j = 0; j < (int)sizeof(buf); j++)
        {
            if (buf[j] != (unsigned char)((i + j) & 0xff))
            {
                ok = -1;
                break;
            }
        }
    }
    close(fd);
    unlink(path);
    pthread_mutex_unlock(&g_sd_lock);

    if (ok == 0)
        snprintf(msg, cap, "ok=1 size=%d", 64 * 1024);
    else
        snprintf(msg, cap, "ok=0 err=verify");
    return ok;
}

int snap_store_read_file(const char *name, unsigned char *buf, int cap)
{
    char        path[SNAP_FILE_NAME_MAX + 16];
    struct stat st;
    int         fd;
    int         off = 0;
    int         err = 0;

    if (!gal_safe_name(name) || buf == NULL || cap <= 0)
    {
        return -4;
    }

    if (!gal_make_path(path, sizeof(path), name))
    {
        return -4;
    }

    pthread_mutex_lock(&g_sd_lock);

    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }
    if (st.st_size > (off_t)cap)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -2;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    while (off < cap)
    {
        int n = read(fd, buf + off, (size_t)(cap - off));

        if (n < 0)
        {
            err = 1;
            break;
        }
        if (n == 0)
            break;
        off += n;
    }

    close(fd);
    pthread_mutex_unlock(&g_sd_lock);

    /* 读失败/短读时不要返回半张图，宁可让前端显示 404 */
    if (err || off != (int)st.st_size)
    {
        printf("[snap_store] read %s short/err: off=%d size=%ld err=%d\n",
               name, off, (long)st.st_size, err);
        return -1;
    }

    return off;
}

int snap_store_load_clip(const char *name, unsigned char **out, int *size)
{
    char        path[SNAP_FILE_NAME_MAX + 16];
    struct stat st;
    unsigned char *buf;
    int         fd;
    int         off = 0;
    int         err = 0;

    if (out == NULL || size == NULL || !gal_safe_avi(name))
    {
        return -4;
    }
    if (!gal_make_path(path, sizeof(path), name))
    {
        return -4;
    }

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }
    if ((unsigned long long)st.st_size > SNAP_CLIP_MAX)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -2;
    }

    buf = (unsigned char *)malloc((size_t)st.st_size);
    if (buf == NULL)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -2;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        free(buf);
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    while (off < (int)st.st_size)
    {
        int n = (int)read(fd, buf + off, (size_t)((int)st.st_size - off));
        if (n < 0)
        {
            err = 1;
            break;
        }
        if (n == 0)
        {
            break;
        }
        off += n;
    }
    close(fd);
    pthread_mutex_unlock(&g_sd_lock);

    if (err || off != (int)st.st_size)
    {
        free(buf);
        return -1;
    }

    *out = buf;
    *size = off;
    return off;
}

/*
 * 流式播放用：open 只做 stat + open（拿一次 SD 锁），read 每块拿一次锁。
 * 网页线程一边读一边 send，SD 锁不会在整段传输期间被占住，录像线程
 * 也能按 200ms 节拍插进去写。
 */
int snap_store_clip_open(const char *name, unsigned *size)
{
    char        path[SNAP_FILE_NAME_MAX + 16];
    struct stat st;
    int         fd;

    if (size == NULL || !gal_safe_avi(name))
    {
        return -4;
    }
    if (!gal_make_path(path, sizeof(path), name))
    {
        return -4;
    }

    pthread_mutex_lock(&g_sd_lock);
    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }
    if ((unsigned long long)st.st_size > SNAP_CLIP_STREAM_MAX)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -2;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }
    g_clip_open++;
    pthread_mutex_unlock(&g_sd_lock);

    *size = (unsigned)st.st_size;
    return fd;
}

int snap_store_clip_read(int fd, void *buf, int len)
{
    int n;

    if (fd < 0 || buf == NULL || len <= 0)
    {
        return -4;
    }

    pthread_mutex_lock(&g_sd_lock);
    n = (int)read(fd, buf, (size_t)len);
    pthread_mutex_unlock(&g_sd_lock);
    return n;
}

void snap_store_clip_close(int fd)
{
    if (fd < 0)
    {
        return;
    }

    pthread_mutex_lock(&g_sd_lock);
    if (g_clip_open > 0)
    {
        g_clip_open--;
    }
    close(fd);
    pthread_mutex_unlock(&g_sd_lock);
}

int snap_store_delete_file(const char *name)
{
    char path[SNAP_FILE_NAME_MAX + 16];
    int  ret;

    if (!gal_safe_media(name))
    {
        return -4;
    }

    if (!gal_make_path(path, sizeof(path), name))
    {
        return -4;
    }

    pthread_mutex_lock(&g_sd_lock);

    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }

    ret = unlink(path);
    pthread_mutex_unlock(&g_sd_lock);

    if (ret == 0)
    {
        /* 同步清掉内存事件环里的引用，避免 /api/events 还给出已删文件的链接 */
        ring_forget(path);
    }

    return (ret == 0) ? 0 : -1;
}

int snap_store_clean_files(int keep)
{
    int total;
    int i;
    int deleted = 0;

    if (keep < 1)
    {
        keep = 1;
    }

    pthread_mutex_lock(&g_sd_lock);

    if (!sd_probe())
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -3;
    }

    total = gal_scan_locked();
    if (total < 0)
    {
        pthread_mutex_unlock(&g_sd_lock);
        return -1;
    }

    qsort(g_gal, (size_t)total, sizeof(g_gal[0]), gal_cmp);

    /* g_gal 新的在前，所以从 keep 开始就是旧的 */
    for (i = keep; i < total; i++)
    {
        char path[SNAP_FILE_NAME_MAX + 16];

        if (!gal_make_path(path, sizeof(path), g_gal[i].name))
        {
            continue;
        }
        if (unlink(path) == 0)
        {
            deleted++;
            ring_forget(path);
        }
    }

    pthread_mutex_unlock(&g_sd_lock);
    return deleted;
}

int snap_store_remount(int force)
{
    struct dfs_filesystem *fs;
    int                    ret;
    int                    tries;
    int                    i;

    pthread_mutex_lock(&g_sd_lock);

    if (g_clip_open > 0)
    {
        /* 还有播放器在读文件，拆卷会把它手里的 FIL 弄坏 */
        pthread_mutex_unlock(&g_sd_lock);
        return -2;
    }

    fs = dfs_filesystem_lookup("/mnt");
    if (fs != NULL && fs->path != NULL && strcmp(fs->path, "/mnt") == 0)
    {
        if (!force)
        {
            pthread_mutex_unlock(&g_sd_lock);
            return 0;
        }
        printf("[snap_store] remount: unmount /mnt\n");
        ret = dfs_unmount("/mnt");
        if (ret != 0)
        {
            /* 还有文件开着（录像/播放）时拆不掉，硬拆会把 FatFs 状态搞乱 */
            printf("[snap_store] remount: unmount failed %d (busy)\n", ret);
            pthread_mutex_unlock(&g_sd_lock);
            return -2;
        }
    }

    /*
     * 卡被上一次超时留在数据态时，只复位控制器、重新 mount 是没用的
     * （这正是“重新挂载”以前像摆设的原因）。这里让 mmcsd 把卡当拔插
     * 一次：删掉块设备 → 重新 CMD0/ACMD41 → 重建 mmcblk0[p1]。
     */
    if (fh_mmc_card_reinit() == 0)
    {
        int gone = 0;
        int back = 0;

        /* 先等旧的块设备注销，再等新枚举出来的设备注册好。
         * 顺序反了会拿到马上要被 free 的旧设备指针。*/
        for (i = 0; i < 30; i++)
        {
            if (rt_device_find("mmcblk0p1") == RT_NULL &&
                rt_device_find("mmcblk0") == RT_NULL)
            {
                gone = 1;
                break;
            }
            rt_thread_mdelay(100);
        }
        for (i = 0; i < 50; i++)
        {
            if (rt_device_find("mmcblk0p1") != RT_NULL ||
                rt_device_find("mmcblk0") != RT_NULL)
            {
                back = 1;
                break;
            }
            rt_thread_mdelay(100);
        }
        printf("[snap_store] remount: card re-init, gone=%d back=%d\n",
               gone, back);
    }

    mkdir("/mnt", 0);

    for (tries = 0; tries < 3; tries++)
    {
        ret = dfs_mount("mmcblk0p1", "/mnt", "elm", 0, 0);
        if (ret == 0)
        {
            printf("[snap_store] remount mmcblk0p1 OK\n");
            break;
        }

        /* 有些卡是“超级软盘”格式：FAT32 直接写在整卡上，没有 MBR 分区表，
         * 此时只有 mmcblk0，没有 mmcblk0p1。退回挂载整卡。*/
        ret = dfs_mount("mmcblk0", "/mnt", "elm", 0, 0);
        if (ret == 0)
        {
            printf("[snap_store] remount whole card OK\n");
            break;
        }

        rt_thread_mdelay(300);
    }

    if (ret == 0)
    {
        g_sd_ok  = -1;      /* 让下一次 sd_probe 重新判断 */
        g_dir_ok = 0;       /* 新挂载的文件系统，下次写文件时重建目录 */
    }
    else
    {
        printf("[snap_store] remount failed: %d\n", ret);
    }

    pthread_mutex_unlock(&g_sd_lock);
    return (ret == 0) ? 0 : -1;
}
