/*
 * rec_store.c —— 手动 / 定时录像 + 人流量报告
 *
 * 录像格式沿用事件短视频那套 MJPEG AVI（800x480，5fps），网页播放器
 * 不用改。观察者线程只把选中的帧 memcpy 进环形队列，SD 写在 rec 线程，
 * 避免卡顿拖住 MJPEG 发送。
 *
 * 与布防的协调：录制期间 snap_store 会跳过事件短视频（clip_store），
 * 截图和小智播报照常；详见 snap_store.c 的 clip_started 判断。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include "config.h"
#include "stream/include/mjpeg_cache.h"
#include "stream/include/snap_store.h"
#include "stream/include/snap_time.h"
#include "stream/include/stats.h"
#include "stream/include/rec_store.h"
#include "libsample_common/include/libsample_common.h"

/* plat_mmc.c 导出：累计的控制器复位次数。一次录制期间只要涨过，就说明
 * 这块卡在当前条件下出过错，录完自动重新挂载一次，免得用户手动操作。*/
extern unsigned fh_mmc_recover_count(void);

#define REC_FRAME_MAX    (128 * 1024)
#define REC_SLOTS        12
#define REC_FRAME_MS     200            /* 5fps，和事件短视频一致 */
#define REC_AVI_HDR      224
#define REC_THREAD_STACK (32 * 1024)

#define REC_SCHED_FILE  SNAP_DIR "/rec_sched.txt"
#define REC_REPORT_LOG  SNAP_DIR "/reports.log"

/* ---------------- 帧队列 ---------------- */
static unsigned char *g_qbuf[REC_SLOTS];
static unsigned char *g_wbuf;           /* 出队后拷到这里再写 SD */
static int            g_qlen[REC_SLOTS];
static int            g_qhead, g_qtail, g_qcount;
static pthread_mutex_t g_q_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------------- 录制状态 ---------------- */
static volatile int g_ready;            /* 队列分配/线程都就绪 */
static volatile int g_thread_running;
static volatile int g_thread_stop;
static volatile int g_rec;              /* 1=正在录 */
static volatile int g_start_req, g_stop_req;
static int          g_start_cam_req;
static int          g_start_dur_req;    /* 秒；0=手动停止 */
static unsigned     g_manual_end_ms;    /* 限时手动录制的截止（get_ms） */
static int          g_rec_cam;
static int          g_fd = -1;
static unsigned     g_frames, g_sum_bytes, g_maxlen;
static unsigned     g_start_epoch;
static unsigned     g_last_frame_ms;
static unsigned     g_dropped;
static int          g_w = 800, g_h = 480;
static int          g_sched_rec;        /* 当前录制由定时触发 */
static unsigned     g_recover_at_start; /* rec_begin 时的复位计数 */
static int          g_io_error;         /* 本次录制出现过写失败 */
static int          g_need_remount;     /* 需要自动重挂 SD（录制错误或空闲期复位） */
static int          g_remount_tries;    /* 连续失败次数，超过上限就放弃 */
static unsigned     g_remount_try_ms;   /* 上次尝试时间，失败时 3s 后重试 */
static unsigned     g_remount_ok_ms;    /* 上次成功重挂时间，避免短时间内反复重枚举 */
static unsigned     g_last_recover_seen;
static char         g_file_rel[80];

/* ---------------- 定时 ---------------- */
static int      g_sched_enabled;
static int      g_sched_daily;
static int      g_sched_date;           /* YYYYMMDD，daily=0 时有效 */
static int      g_sched_start;          /* 当天第几分钟 */
static int      g_sched_end;
static int      g_sched_cam = 1;        /* 定时录制哪路：0=左摄 1=右摄/AI */
static int      g_sched_window;         /* 本次时间窗已经启动过 */
static unsigned g_sched_end_epoch;
static int      g_sched_loaded;         /* 启动时 SD 可能没好，稍后再读一次 */

/* ---------------- 报告环 ---------------- */
static rec_report_t g_reports[REC_REPORT_MAX];
static int          g_report_head;
static int          g_report_count;
static unsigned     g_report_seq = 1;
static pthread_mutex_t g_rep_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile unsigned g_voice_id;
static volatile unsigned g_voice_req;   /* 每次点播报都 +1，允许重复播同一条 */

/* ------------------------------------------------------------------ */
/* 小块工具                                                            */
/* ------------------------------------------------------------------ */
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
            break;
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

/* 和 clip_store 完全一致的 AVI 头，字段在 finalize 时回填。*/
static void avi_header(unsigned char *hdr, unsigned frames, unsigned sum,
                       unsigned maxlen, int width, int height)
{
    unsigned char *p = hdr;

    memset(hdr, 0, REC_AVI_HDR);

    wtag(p, "RIFF"); w32(p + 4, 216u + sum); p += 8;
    wtag(p, "AVI "); p += 4;
    wtag(p, "LIST"); w32(p + 4, 192); p += 8;
    wtag(p, "hdrl"); p += 4;
    wtag(p, "avih"); w32(p + 4, 56); p += 8;
    w32(p + 0, REC_FRAME_MS * 1000u);
    w32(p + 4, maxlen * 5u);
    w32(p + 8, 0);
    w32(p + 12, 0);
    w32(p + 16, frames);
    w32(p + 20, 0);
    w32(p + 24, 1);
    w32(p + 28, maxlen);
    w32(p + 32, (uint32_t)width);
    w32(p + 36, (uint32_t)height);
    p += 56;
    wtag(p, "LIST"); w32(p + 4, 116); p += 8;
    wtag(p, "strl"); p += 4;
    wtag(p, "strh"); w32(p + 4, 56); p += 8;
    wtag(p + 0, "vids");
    wtag(p + 4, "MJPG");
    w32(p + 8, 0);
    w16(p + 12, 0);
    w16(p + 14, 0);
    w32(p + 16, 0);
    w32(p + 20, 1);
    w32(p + 24, 5);
    w32(p + 28, 0);
    w32(p + 32, frames);
    w32(p + 36, maxlen);
    w32(p + 40, 0xFFFFFFFFu);
    w32(p + 44, 0);
    w16(p + 48, 0);
    w16(p + 50, 0);
    w16(p + 52, (uint16_t)width);
    w16(p + 54, (uint16_t)height);
    p += 56;
    wtag(p, "strf"); w32(p + 4, 40); p += 8;
    w32(p + 0, 40);
    w32(p + 4, (uint32_t)width);
    w32(p + 8, (uint32_t)height);
    w16(p + 12, 1);
    w16(p + 14, 24);
    wtag(p + 16, "MJPG");
    w32(p + 20, (uint32_t)(width * height * 3));
    w32(p + 24, 0);
    w32(p + 28, 0);
    w32(p + 32, 0);
    w32(p + 36, 0);
    p += 40;
    wtag(p, "LIST"); w32(p + 4, 4u + sum); p += 8;
    wtag(p, "movi"); p += 4;
}

static void patch32(int fd, off_t off, uint32_t v)
{
    unsigned char b[4];

    w32(b, v);
    (void)lseek(fd, off, SEEK_SET);
    (void)write(fd, b, 4);
}

/* ------------------------------------------------------------------ */
/* 文件 / 目录                                                         */
/* ------------------------------------------------------------------ */
static int rec_ensure_dirs(void)
{
    if (!snap_store_sd_ready())
    {
        return 0;
    }

    (void)mkdir(SNAP_DIR, 0777);
    (void)mkdir(SNAP_DIR_LEFT, 0777);
    (void)mkdir(SNAP_DIR_RIGHT, 0777);
    return 1;
}

/* 生成相对路径（right/rec_...avi）和完整路径。*/
static void rec_make_path(int cam, unsigned epoch, unsigned ms,
                          char *rel, int rel_cap, char *full, int full_cap)
{
    char date[9];
    char clk[7];
    const char *dir = cam ? "right" : "left";

    if (snap_time_name_at(epoch, date, sizeof(date), clk, sizeof(clk)))
    {
        snprintf(rel, rel_cap, "%s/rec_%s_%s.avi", dir, date, clk);
    }
    else
    {
        snprintf(rel, rel_cap, "%s/rec_%010u.avi", dir, ms);
    }

    snprintf(full, full_cap, SNAP_DIR "/%s", rel);
}

/* ------------------------------------------------------------------ */
/* AVI 写                                                              */
/* ------------------------------------------------------------------ */
static int avi_open(const char *full, const char *rel, int cam)
{
    unsigned char hdr[REC_AVI_HDR];
    unsigned epoch = 0;
    int fd;

    (void)cam;

    avi_header(hdr, 0, 0, 0, 800, 480);

    fd = open(full, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        printf("[rec] open %s failed errno=%d\n", full, errno);
        return -1;
    }
    if (write_all(fd, hdr, REC_AVI_HDR) != 0)
    {
        close(fd);
        unlink(full);
        return -1;
    }

    (void)snap_time_get(&epoch);
    printf("[rec] start %s (cam %d)\n", rel, cam);
    return fd;
}

static int avi_write_frame(int fd, const unsigned char *data, int len)
{
    unsigned char tag[8];

    if (g_frames == 0)
    {
        jpeg_size(data, len, &g_w, &g_h);
        if (g_w <= 0 || g_h <= 0)
        {
            g_w = 800;
            g_h = 480;
        }
    }

    wtag(tag, "00dc");
    w32(tag + 4, (uint32_t)len);

    if (write_all(fd, tag, 8) != 0 ||
        write_all(fd, data, len) != 0 ||
        ((len & 1) && write_all(fd, "\0", 1) != 0))
    {
        return -1;
    }

    g_frames++;
    g_sum_bytes += 8u + (unsigned)len + ((len & 1) ? 1u : 0u);
    if ((unsigned)len > g_maxlen)
    {
        g_maxlen = (unsigned)len;
    }

    return 0;
}

static void avi_finalize(int fd)
{
    if (fd < 0)
    {
        return;
    }

    /* 回填 AVI 头里的总长、帧数、幅面 */
    patch32(fd, 4, 216u + g_sum_bytes);
    patch32(fd, 36, g_maxlen * 5u);
    patch32(fd, 48, g_frames);
    patch32(fd, 60, g_maxlen);
    patch32(fd, 64, (uint32_t)g_w);
    patch32(fd, 68, (uint32_t)g_h);
    patch32(fd, 140, g_frames);
    patch32(fd, 144, g_maxlen);
    patch32(fd, 176, (uint32_t)g_w);
    patch32(fd, 180, (uint32_t)g_h);
    patch32(fd, 216, 4u + g_sum_bytes);

    (void)fsync(fd);
    (void)lseek(fd, 0, SEEK_END);
    close(fd);
}

/* ------------------------------------------------------------------ */
/* 报告                                                                */
/* ------------------------------------------------------------------ */
static void report_log(const rec_report_t *r)
{
    char     line[256];
    int      n;
    int      fd;

    n = snprintf(line, sizeof(line),
                 "id=%u cam=%d start=%s end=%s in=%u out=%u peak=%u "
                 "frames=%u bytes=%u file=%s\n",
                 r->id, r->cam, r->t_start, r->t_end, r->in, r->out,
                 r->peak, r->frames, r->bytes, r->file);
    if (n <= 0)
    {
        return;
    }

    fd = open(REC_REPORT_LOG, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        (void)write(fd, line, n);
        close(fd);
    }
}

static void report_add(int cam, unsigned frames, unsigned bytes)
{
    rec_report_t r;
    unsigned     in = 0, out = 0, peak = 0;
    unsigned     epoch = 0;
    const rec_report_t *e;

    memset(&r, 0, sizeof(r));
    (void)snap_time_get(&epoch);

    stats_session_end(&in, &out, &peak);

    r.valid       = 1;
    r.id          = g_report_seq++;
    r.cam         = cam;
    r.start_epoch = g_start_epoch;
    r.end_epoch   = epoch;
    r.in          = in;
    r.out         = out;
    r.peak        = peak;
    r.frames      = frames;
    r.bytes       = bytes;
    snprintf(r.file, sizeof(r.file), "%s", g_file_rel);
    snap_time_text(g_start_epoch, r.t_start, sizeof(r.t_start));
    snap_time_text(r.end_epoch, r.t_end, sizeof(r.t_end));

    pthread_mutex_lock(&g_rep_lock);
    g_reports[g_report_head] = r;
    g_report_head = (g_report_head + 1) % REC_REPORT_MAX;
    if (g_report_count < REC_REPORT_MAX)
    {
        g_report_count++;
    }
    e = &g_reports[(g_report_head - 1 + REC_REPORT_MAX) % REC_REPORT_MAX];
    r = *e;     /* 锁内取一份，日志用 */
    pthread_mutex_unlock(&g_rep_lock);

    report_log(&r);

    printf("[rec] report id=%u cam=%d in=%u out=%u peak=%u frames=%u "
           "bytes=%u file=%s\n", r.id, r.cam, r.in, r.out, r.peak,
           r.frames, r.bytes, r.file);
}

/* ------------------------------------------------------------------ */
/* 录制开始 / 停止                                                     */
/* ------------------------------------------------------------------ */
static int rec_begin(int cam, int from_sched)
{
    char     full[96];
    char     rel[80];
    unsigned epoch = 0;
    unsigned ms = (unsigned)get_ms();
    int      fd;

    if (g_rec)
    {
        return -1;
    }
    if (!rec_ensure_dirs())
    {
        printf("[rec] start failed: SD not ready\n");
        return -2;
    }

    (void)snap_time_get(&epoch);
    rec_make_path(cam, epoch, ms, rel, sizeof(rel), full, sizeof(full));

    fd = avi_open(full, rel, cam);
    if (fd < 0)
    {
        return -3;
    }

    snprintf(g_file_rel, sizeof(g_file_rel), "%s", rel);
    g_rec_cam     = cam;
    g_start_epoch = epoch;
    g_frames      = 0;
    g_sum_bytes   = 0;
    g_maxlen      = 0;
    g_w = 800;
    g_h = 480;
    g_dropped     = 0;
    g_sched_rec   = from_sched;
    g_io_error    = 0;
    g_recover_at_start = fh_mmc_recover_count();

    pthread_mutex_lock(&g_q_lock);
    g_qhead = g_qtail = g_qcount = 0;
    pthread_mutex_unlock(&g_q_lock);

    stats_session_begin();
    g_last_frame_ms = ms;
    g_fd = fd;
    g_rec = 1;

    return 0;
}

static void rec_end(void)
{
    unsigned frames, bytes;

    if (!g_rec)
    {
        return;
    }

    /* 先停止收帧，再把队列里剩下的写完 */
    g_rec = 0;

    pthread_mutex_lock(&g_q_lock);
    while (g_qcount > 0)
    {
        int slot = g_qhead;
        int len  = g_qlen[slot];

        pthread_mutex_unlock(&g_q_lock);
        if (avi_write_frame(g_fd, g_qbuf[slot], len) != 0)
        {
            g_io_error = 1;
        }
        pthread_mutex_lock(&g_q_lock);
        g_qhead = (g_qhead + 1) % REC_SLOTS;
        g_qcount--;
    }
    pthread_mutex_unlock(&g_q_lock);

    frames = g_frames;
    bytes  = g_sum_bytes;

    avi_finalize(g_fd);
    g_fd = -1;
    g_manual_end_ms = 0;

    report_add(g_rec_cam, frames, bytes);

    printf("[rec] stop cam=%d frames=%u bytes=%u dropped=%u\n",
           g_rec_cam, frames, bytes, g_dropped);

    /*
     * 这块板 1-bit SD 长时间写入偶尔会把卡留在数据态：写失败或者控制器
     * 复位过，就意味着文件系统/卡状态可能已经不可靠。等文件关掉后自动
     * 重新枚举并挂载一次，用户不用再手动点“重新挂载”才能看视频。
     */
    if (g_io_error || fh_mmc_recover_count() != g_recover_at_start)
    {
        g_need_remount   = 1;
        g_remount_tries  = 0;
        g_remount_try_ms = 0;
        printf("[rec] SD had errors during recording (io=%d, recover=%u->%u)\n",
               g_io_error, g_recover_at_start, fh_mmc_recover_count());
    }
}

/* ------------------------------------------------------------------ */
/* 帧入口（get_stream 线程）                                            */
/* ------------------------------------------------------------------ */
void rec_store_on_frame(int server_index, const unsigned char *data, int len)
{
    int idx;
    int slot;
    unsigned now;

    if (!g_ready || !g_rec || data == NULL || len <= 0 ||
        len > REC_FRAME_MAX)
    {
        return;
    }

    idx = mjpeg_cache_index_of(g_rec_cam ? 1 : 0, 1);
    if (idx < 0 || server_index != idx)
    {
        return;
    }

    now = (unsigned)get_ms();
    if ((now - g_last_frame_ms) < REC_FRAME_MS)
    {
        return;
    }
    g_last_frame_ms = now;

    pthread_mutex_lock(&g_q_lock);
    if (g_qcount >= REC_SLOTS)
    {
        g_dropped++;
        pthread_mutex_unlock(&g_q_lock);
        return;
    }

    slot = g_qtail;
    memcpy(g_qbuf[slot], data, (size_t)len);
    g_qlen[slot] = len;
    g_qtail = (g_qtail + 1) % REC_SLOTS;
    g_qcount++;
    pthread_mutex_unlock(&g_q_lock);
}

/* ------------------------------------------------------------------ */
/* 定时                                                                */
/* ------------------------------------------------------------------ */
static void sched_save(void)
{
    char line[96];
    int  n;
    int  fd;

    if (!rec_ensure_dirs())
    {
        return;
    }

    n = snprintf(line, sizeof(line), "%d %d %d %d %d %d\n",
                 g_sched_enabled, g_sched_daily, g_sched_date,
                 g_sched_start, g_sched_end, g_sched_cam);
    if (n <= 0)
    {
        return;
    }

    fd = open(REC_SCHED_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        (void)write(fd, line, n);
        close(fd);
    }
}

static void sched_load(void)
{
    char line[96];
    int  fd;
    int  n;
    int  a, b, c, d, e, f = 1;

    fd = open(REC_SCHED_FILE, O_RDONLY);
    if (fd < 0)
    {
        return;
    }
    n = (int)read(fd, line, sizeof(line) - 1);
    close(fd);
    if (n <= 0)
    {
        return;
    }
    line[n] = '\0';

    /* 老文件只有 5 个字段，读不到 cam 时按右摄/AI 处理 */
    if (sscanf(line, "%d %d %d %d %d %d", &a, &b, &c, &d, &e, &f) >= 5)
    {
        g_sched_enabled = a ? 1 : 0;
        g_sched_daily   = b ? 1 : 0;
        g_sched_date    = c;
        g_sched_start   = d;
        g_sched_end     = e;
        g_sched_cam     = (f == 0) ? 0 : 1;
        printf("[rec] schedule loaded: en=%d daily=%d date=%d %d-%d cam=%d\n",
               g_sched_enabled, g_sched_daily, g_sched_date,
               g_sched_start, g_sched_end, g_sched_cam);
    }
}

/* 秒 -> 当天分钟；同时给出 YYYYMMDD。返回 0 表示时间未同步。*/
static int sched_now(int *ymd, int *minute, unsigned *nowp)
{
    unsigned now = 0;
    char     date[9];
    char     clk[7];
    int      synced;

    synced = snap_time_get(&now);
    if (!synced || now == 0)
    {
        return 0;
    }
    if (!snap_time_name_at(now, date, sizeof(date), clk, sizeof(clk)))
    {
        return 0;
    }

    *ymd    = atoi(date);
    *minute = (int)((now % 86400) / 60);
    if (nowp)
    {
        *nowp = now;
    }
    return 1;
}

static void sched_tick(void)
{
    int      ymd, minute;
    unsigned now = 0;
    int      in_window;

    /* 开机时 SD 可能还没挂上，等就绪后再读一次持久化的定时 */
    if (!g_sched_loaded)
    {
        if (!snap_store_sd_ready())
        {
            return;
        }
        sched_load();
        g_sched_loaded = 1;
    }

    if (!g_sched_enabled)
    {
        return;
    }
    if (!sched_now(&ymd, &minute, &now))
    {
        return;
    }
    if (g_sched_date > 0 && !g_sched_daily && ymd != g_sched_date)
    {
        return;
    }

    in_window = (minute >= g_sched_start && minute < g_sched_end);

    if (in_window && !g_sched_window)
    {
        if (!g_rec && !g_start_req && !g_stop_req)
        {
            g_sched_window = 1;
            /* 定时录哪路由设置页面选：0=左摄原画，1=右摄/AI */
            if (rec_begin(g_sched_cam, 1) == 0)
            {
                int dur = g_sched_end - minute;

                if (dur < 1)
                {
                    dur = 1;
                }
                g_sched_end_epoch = now + (unsigned)dur * 60u;
                printf("[rec] scheduled start cam=%d, end in %d min\n",
                       g_sched_cam, dur);
            }
        }
    }
    else if (!in_window && g_sched_window)
    {
        /* 窗口结束：如果还在录且是定时触发的，停掉 */
        g_sched_window = 0;
        if (g_rec && g_sched_rec)
        {
            rec_end();
        }
    }

    if (g_sched_rec && g_rec && g_sched_end_epoch != 0 &&
        now >= g_sched_end_epoch)
    {
        rec_end();
    }
}

/* ------------------------------------------------------------------ */
/* 线程                                                                */
/* ------------------------------------------------------------------ */
static void *rec_thread(void *arg)
{
    (void)arg;

    g_thread_running = 1;

    while (!g_thread_stop)
    {
        int start_req, stop_req, start_cam, start_dur;

        /* 1. 把队列里的帧写掉 */
        if (g_fd >= 0)
        {
            for (;;)
            {
                int slot;
                int len;

                pthread_mutex_lock(&g_q_lock);
                if (g_qcount == 0)
                {
                    pthread_mutex_unlock(&g_q_lock);
                    break;
                }
                slot = g_qhead;
                len  = g_qlen[slot];
                memcpy(g_wbuf, g_qbuf[slot], (size_t)len);
                g_qhead = (g_qhead + 1) % REC_SLOTS;
                g_qcount--;
                pthread_mutex_unlock(&g_q_lock);

                if (avi_write_frame(g_fd, g_wbuf, len) != 0)
                {
                    printf("[rec] write failed, stop recording\n");
                    g_io_error = 1;
                    rec_end();
                    break;
                }
            }
        }

        /* 2. 手动开始/停止请求 */
        stop_req = g_stop_req;
        start_req = g_start_req;
        start_cam = g_start_cam_req;
        start_dur = g_start_dur_req;

        if (stop_req)
        {
            /* 连按开始/停止时，停止要能把还没执行的开始请求也取消掉 */
            g_stop_req = 0;
            g_start_req = 0;
            if (g_rec)
            {
                rec_end();
            }
        }
        else if (start_req)
        {
            g_start_req = 0;
            if (!g_rec && rec_begin(start_cam, 0) == 0)
            {
                g_manual_end_ms = (start_dur > 0)
                                  ? (unsigned)get_ms() + (unsigned)start_dur * 1000u
                                  : 0;
                g_sched_rec = 0;
            }
        }

        /*
         * 2.4 空闲自愈：只要驱动复位过（超时/错误）就说明卡可能停在数据态，
         * 之后所有读写都会超时，事件存证也就全部失败。不在录像中时直接
         * 安排一次重挂；录像中的复位由 rec_end 统一统计。
         */
        {
            unsigned rc = fh_mmc_recover_count();

            if (g_last_recover_seen == 0)
            {
                g_last_recover_seen = rc;
            }
            else if (rc != g_last_recover_seen)
            {
                g_last_recover_seen = rc;
                if (!g_rec && !g_need_remount &&
                    (g_remount_ok_ms == 0 ||
                     (int)((unsigned)get_ms() - g_remount_ok_ms) >= 30000))
                {
                    g_need_remount   = 1;
                    g_remount_tries  = 0;
                    g_remount_try_ms = 0;
                    printf("[rec] SD recovery seen (total=%u), schedule remount\n", rc);
                }
            }
        }

        /*
         * 2.5 自动重挂。
         * 放在定时检查之前：卡恢复干净了再让定时窗口开录。正在播放视频
         * （g_clip_open>0）时 snap_store_remount 会返回 busy，3 秒后重试。
         */
        if (g_need_remount && !g_rec)
        {
            unsigned now = (unsigned)get_ms();

            if (g_remount_try_ms == 0 ||
                (int)(now - g_remount_try_ms) >= 3000)
            {
                int ret;

                g_remount_try_ms = now;
                printf("[rec] auto remount SD\n");
                ret = snap_store_remount(1);
                if (ret == 0)
                {
                    g_need_remount   = 0;
                    g_remount_tries  = 0;
                    g_remount_ok_ms  = now;
                    g_last_recover_seen = fh_mmc_recover_count();
                    printf("[rec] auto remount OK\n");
                }
                else
                {
                    /* 播放器占用（-2）时多等一会儿；真正挂不上则不要无限重试，
                     * 否则每 3 秒卡一次重枚举，新录像也起不来。*/
                    if (ret != -2 && ++g_remount_tries >= 3)
                    {
                        g_need_remount  = 0;
                        g_remount_tries = 0;
                        g_remount_ok_ms = (unsigned)get_ms();  /* 30s 内别再试 */
                        g_last_recover_seen = fh_mmc_recover_count();
                        printf("[rec] auto remount gave up (%d)\n", ret);
                    }
                    else
                    {
                        printf("[rec] auto remount failed (%d), retry later\n", ret);
                    }
                }
            }
        }

        /* 3. 定时窗口 */
        sched_tick();

        /* 4. 限时手动录制到点自动停 */
        if (g_rec && !g_sched_rec && g_manual_end_ms != 0)
        {
            unsigned now = (unsigned)get_ms();

            if ((int)(g_manual_end_ms - now) <= 0)
            {
                rec_end();
            }
        }

        usleep(100 * 1000);
    }

    if (g_rec)
    {
        rec_end();
    }

    g_thread_running = 0;
    return NULL;
}

/* ------------------------------------------------------------------ */
/* 对外接口                                                            */
/* ------------------------------------------------------------------ */
int rec_store_init(void)
{
    pthread_t      th;
    pthread_attr_t attr;
    int            i;

    g_qbuf[0] = (unsigned char *)malloc(REC_SLOTS * REC_FRAME_MAX);
    g_wbuf    = (unsigned char *)malloc(REC_FRAME_MAX);
    if (g_qbuf[0] == NULL || g_wbuf == NULL)
    {
        free(g_qbuf[0]);
        free(g_wbuf);
        g_qbuf[0] = NULL;
        g_wbuf = NULL;
        printf("[rec] malloc %dKB failed\n",
               (REC_SLOTS * REC_FRAME_MAX) / 1024);
        return -1;
    }

    for (i = 0; i < REC_SLOTS; i++)
    {
        g_qbuf[i] = g_qbuf[0] + (size_t)i * REC_FRAME_MAX;
    }

    if (snap_store_sd_ready())
    {
        sched_load();
        g_sched_loaded = 1;
    }

    /* 用开机毫秒做种子：重启后 req 不会和上一次重复，小智不会误判成同一次播报 */
    g_voice_req = (unsigned)get_ms() + 1;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#ifdef __RTTHREAD_OS__
    pthread_attr_setstacksize(&attr, REC_THREAD_STACK);
#endif
    if (pthread_create(&th, &attr, rec_thread, NULL) != 0)
    {
        printf("[rec] pthread_create failed\n");
        return -1;
    }

    g_ready = 1;

    printf("[rec] ready, %d slots x %dKB, %dms/frame\n",
           REC_SLOTS, REC_FRAME_MAX / 1024, REC_FRAME_MS);
    return 0;
}

void rec_store_deinit(void)
{
    int i;

    g_thread_stop = 1;
    for (i = 0; i < 40 && g_thread_running; i++)
    {
        usleep(50 * 1000);
    }

    if (g_thread_running)
    {
        printf("[rec] thread still running, skip free\n");
        return;
    }

    if (g_fd >= 0)
    {
        rec_end();
    }

    free(g_qbuf[0]);
    free(g_wbuf);
    g_qbuf[0] = NULL;
    g_wbuf = NULL;
    g_ready = 0;
}

int rec_store_manual_start(int cam, int duration_sec)
{
    if (cam != 0 && cam != 1)
    {
        return -1;
    }
    if (!g_ready)
    {
        return -3;
    }
    if (g_rec || g_start_req || g_stop_req)
    {
        return -1;
    }
    if (!rec_ensure_dirs())
    {
        return -2;
    }

    /* 手动开录也算用掉了当前定时窗口，避免窗口内又自动开一次 */
    if (g_sched_enabled)
    {
        int ymd, minute;

        if (sched_now(&ymd, &minute, NULL) &&
            (g_sched_daily || g_sched_date == ymd) &&
            minute >= g_sched_start && minute < g_sched_end)
        {
            g_sched_window = 1;
        }
    }

    if (duration_sec < 0)
    {
        duration_sec = 0;
    }
    if (duration_sec > 3 * 60)
    {
        duration_sec = 3 * 60;   /* 手动录制上限 3 分钟 */
    }

    g_start_cam_req = cam;
    g_start_dur_req = duration_sec;
    g_manual_end_ms = 0;
    g_start_req = 1;
    return 0;
}

int rec_store_manual_stop(void)
{
    if ((!g_rec && !g_start_req) || g_stop_req)
    {
        return -1;
    }
    g_stop_req = 1;
    return 0;
}

int rec_store_is_recording(void)
{
    return g_rec;
}

int rec_store_active_cam(void)
{
    return g_rec ? g_rec_cam : -1;
}

int rec_sched_set(int daily, int date, int start_min, int end_min, int cam)
{
    if (start_min < 0 || end_min > 24 * 60 || start_min >= end_min)
    {
        return -1;
    }
    if (!daily && (date < 20200101 || date > 20991231))
    {
        return -1;
    }

    g_sched_daily   = daily ? 1 : 0;
    g_sched_date    = daily ? 0 : date;
    g_sched_start   = start_min;
    g_sched_end     = end_min;
    g_sched_cam     = (cam == 0) ? 0 : 1;
    g_sched_enabled = 1;
    g_sched_window  = 0;
    sched_save();

    printf("[rec] schedule set: daily=%d date=%d %02d:%02d-%02d:%02d cam=%d\n",
           g_sched_daily, g_sched_date, start_min / 60, start_min % 60,
           end_min / 60, end_min % 60, g_sched_cam);
    return 0;
}

int rec_sched_clear(void)
{
    g_sched_enabled = 0;
    g_sched_window  = 0;
    sched_save();
    return 0;
}

void rec_sched_get(int *enabled, int *daily, int *date,
                   int *start_min, int *end_min, int *cam)
{
    if (enabled)   *enabled = g_sched_enabled;
    if (daily)     *daily = g_sched_daily;
    if (date)      *date = g_sched_date;
    if (start_min) *start_min = g_sched_start;
    if (end_min)   *end_min = g_sched_end;
    if (cam)       *cam = g_sched_cam;
}

int rec_report_latest(void)
{
    int id = -1;
    int head;
    int count;

    pthread_mutex_lock(&g_rep_lock);
    head = g_report_head;
    count = g_report_count;
    if (count > 0)
    {
        const rec_report_t *e =
            &g_reports[(head - 1 + REC_REPORT_MAX) % REC_REPORT_MAX];

        if (e->valid)
        {
            id = (int)e->id;
        }
    }
    pthread_mutex_unlock(&g_rep_lock);
    return id;
}

int rec_report_get(unsigned id, rec_report_t *out)
{
    int i;

    if (out == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_rep_lock);
    for (i = 0; i < g_report_count; i++)
    {
        const rec_report_t *e =
            &g_reports[(g_report_head - 1 - i + REC_REPORT_MAX * 2) % REC_REPORT_MAX];

        if (e->valid && e->id == id)
        {
            *out = *e;
            pthread_mutex_unlock(&g_rep_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_rep_lock);
    return -1;
}

int rec_report_list(rec_report_t *out, int max)
{
    int i;
    int n = 0;

    if (out == NULL || max <= 0)
    {
        return 0;
    }

    pthread_mutex_lock(&g_rep_lock);
    for (i = 0; i < g_report_count && n < max; i++)
    {
        const rec_report_t *e =
            &g_reports[(g_report_head - 1 - i + REC_REPORT_MAX * 2) % REC_REPORT_MAX];

        if (e->valid)
        {
            out[n++] = *e;
        }
    }
    pthread_mutex_unlock(&g_rep_lock);
    return n;
}

int rec_report_play(unsigned id)
{
    rec_report_t r;

    if (rec_report_get(id, &r) != 0)
    {
        return -1;
    }
    g_voice_id = id;
    g_voice_req++;
    return 0;
}

int rec_report_pending(unsigned *id, unsigned *in, unsigned *out, unsigned *peak,
                       unsigned *req)
{
    rec_report_t r;

    if (g_voice_id == 0)
    {
        return 0;
    }
    if (rec_report_get(g_voice_id, &r) != 0)
    {
        g_voice_id = 0;
        return 0;
    }

    if (id)   *id = r.id;
    if (in)   *in = r.in;
    if (out)  *out = r.out;
    if (peak) *peak = r.peak;
    if (req)  *req = g_voice_req;
    return 1;
}

void rec_report_ack(void)
{
    g_voice_id = 0;
}

int rec_status_format(char *out, int cap)
{
    int en = 0, daily = 0, date = 0, smin = 0, emin = 0, scam = 1;
    unsigned epoch = 0;
    int synced;
    int latest;

    if (out == NULL || cap <= 0)
    {
        return -1;
    }

    unsigned left = 0;

    rec_sched_get(&en, &daily, &date, &smin, &emin, &scam);
    synced = snap_time_get(&epoch);
    latest = rec_report_latest();

    if (g_rec && g_manual_end_ms != 0)
    {
        unsigned now = (unsigned)get_ms();

        if ((int)(g_manual_end_ms - now) > 0)
        {
            left = (g_manual_end_ms - now + 999u) / 1000u;
        }
    }

    return snprintf(out, cap,
                    "rec=%d cam=%d left=%u file=%s frames=%u bytes=%u dropped=%u\n"
                    "sched=%d daily=%d date=%d start=%d end=%d window=%d scam=%d\n"
                    "synced=%d now=%u report=%d\n",
                    g_rec ? 1 : 0, g_rec ? g_rec_cam : -1, left, g_file_rel,
                    g_frames, g_sum_bytes, g_dropped,
                    en, daily, date, smin, emin, g_sched_window, scam,
                    synced ? 1 : 0, epoch, latest);
}
