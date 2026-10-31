/*
 * stats.c —— 安防统计
 *
 * 计数来源：
 *   - NNA 线程：绊线方向、周界进入/离开、画面人数
 *   - 网页线程：只读
 *
 * 落盘 /mnt/snap/stats.bin，20 秒最多写一次，避免频繁写 SD。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "stream/include/stats.h"
#include "stream/include/snap_store.h"
#include "stream/include/snap_time.h"
#include "libsample_common/include/libsample_common.h"

#define STATS_FILE          SNAP_DIR "/stats.bin"
#define STATS_MAGIC         0x53544154u   /* "STAT" */
#define STATS_VERSION       1u
#define STATS_DWELL_MAX     32

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t day;               /* 本地 epoch / 86400 */
    uint32_t in_total;
    uint32_t out_total;
    uint32_t area_in_total;
    uint32_t area_out_total;
    uint32_t today_in;
    uint32_t today_out;
    uint32_t area_in_today;
    uint32_t area_out_today;
    uint32_t hour_in[24];
    uint32_t hour_out[24];
    uint32_t max_people;
    uint32_t max_dwell_ms;
    uint32_t last_kind;
    uint32_t last_epoch;
} stats_blob_t;

typedef struct
{
    int      objId;
    unsigned enter_ms;
} dwell_ent_t;

static pthread_mutex_t g_lock      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_file_lock = PTHREAD_MUTEX_INITIALIZER;

/* 录像会话计数：录制开始时清零，录制结束时取出做报告 */
static int      g_sess_active;
static unsigned g_sess_in;
static unsigned g_sess_out;
static unsigned g_sess_peak;

static stats_blob_t g_blob;
static dwell_ent_t  g_dwell[STATS_DWELL_MAX];
static int          g_visible;
static unsigned     g_last_save;
static int          g_inited;

static void stats_save_blob(const stats_blob_t *b)
{
    int fd;

    pthread_mutex_lock(&g_file_lock);
    fd = open(STATS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0)
    {
        (void)write(fd, b, sizeof(*b));
        (void)fsync(fd);
        close(fd);
    }
    pthread_mutex_unlock(&g_file_lock);
}

static void stats_load(void)
{
    int fd;
    ssize_t n;
    stats_blob_t b;

    fd = open(STATS_FILE, O_RDONLY);
    if (fd < 0)
    {
        return;
    }
    n = read(fd, &b, sizeof(b));
    close(fd);

    if (n == (ssize_t)sizeof(b) && b.magic == STATS_MAGIC &&
        b.version == STATS_VERSION)
    {
        g_blob = b;
    }
}

static void stats_roll_locked(unsigned local, int synced)
{
    unsigned day;

    if (!synced)
    {
        return;
    }

    day = local / 86400u;
    if (day == g_blob.day)
    {
        return;
    }

    g_blob.day = day;
    g_blob.today_in = 0;
    g_blob.today_out = 0;
    g_blob.area_in_today = 0;
    g_blob.area_out_today = 0;
    g_blob.max_people = 0;
    g_blob.max_dwell_ms = 0;
    memset(g_blob.hour_in, 0, sizeof(g_blob.hour_in));
    memset(g_blob.hour_out, 0, sizeof(g_blob.hour_out));
}

static void stats_save_if_due(int force)
{
    stats_blob_t b;
    unsigned now;
    int save = 0;

    pthread_mutex_lock(&g_lock);
    now = (unsigned)get_ms();
    if (force || g_last_save == 0 || (now - g_last_save) >= 20000u)
    {
        g_last_save = now;
        b = g_blob;
        save = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (save)
    {
        stats_save_blob(&b);
    }
}

void stats_init(void)
{
    int i;

    if (g_inited)
    {
        return;
    }

    memset(&g_blob, 0, sizeof(g_blob));
    g_blob.magic = STATS_MAGIC;
    g_blob.version = STATS_VERSION;
    stats_load();

    for (i = 0; i < STATS_DWELL_MAX; i++)
    {
        g_dwell[i].objId = -1;
        g_dwell[i].enter_ms = 0;
    }

    g_last_save = 0;
    g_inited = 1;
}

void stats_reset(void)
{
    pthread_mutex_lock(&g_lock);
    g_blob.today_in = 0;
    g_blob.today_out = 0;
    g_blob.area_in_today = 0;
    g_blob.area_out_today = 0;
    g_blob.max_people = 0;
    g_blob.max_dwell_ms = 0;
    g_blob.last_kind = 0;
    g_blob.last_epoch = 0;
    memset(g_blob.hour_in, 0, sizeof(g_blob.hour_in));
    memset(g_blob.hour_out, 0, sizeof(g_blob.hour_out));
    pthread_mutex_unlock(&g_lock);

    stats_save_if_due(1);
}

void stats_on_trip(int in, int rule, int conf, int count)
{
    unsigned local = 0;
    int      synced;
    unsigned h;

    (void)rule;
    (void)conf;
    (void)count;

    synced = snap_time_get(&local);
    h = synced ? ((local % 86400u) / 3600u) : 0u;
    if (h > 23u)
    {
        h = 23u;
    }

    pthread_mutex_lock(&g_lock);
    stats_roll_locked(local, synced);
    if (in)
    {
        g_blob.today_in++;
        g_blob.in_total++;
        g_blob.hour_in[h]++;
        if (g_sess_active)
        {
            g_sess_in++;
        }
    }
    else
    {
        g_blob.today_out++;
        g_blob.out_total++;
        g_blob.hour_out[h]++;
        if (g_sess_active)
        {
            g_sess_out++;
        }
    }
    g_blob.last_kind = 0;       /* 绊线 */
    g_blob.last_epoch = local;
    pthread_mutex_unlock(&g_lock);

    stats_save_if_due(0);
}

void stats_on_area(int enter, int rule, int count)
{
    unsigned local = 0;
    int      synced;

    (void)rule;
    (void)count;

    synced = snap_time_get(&local);

    pthread_mutex_lock(&g_lock);
    stats_roll_locked(local, synced);
    if (enter)
    {
        g_blob.area_in_today++;
        g_blob.area_in_total++;
    }
    else
    {
        g_blob.area_out_today++;
        g_blob.area_out_total++;
    }
    g_blob.last_kind = 1;       /* 周界 */
    g_blob.last_epoch = local;
    pthread_mutex_unlock(&g_lock);

    stats_save_if_due(0);
}

void stats_on_area_obj(int objId, int enter)
{
    unsigned now;
    int i;
    int free_slot = -1;

    if (objId < 0)
    {
        return;
    }

    now = (unsigned)get_ms();

    pthread_mutex_lock(&g_lock);
    for (i = 0; i < STATS_DWELL_MAX; i++)
    {
        if (g_dwell[i].objId == objId)
        {
            break;
        }
        if (g_dwell[i].objId < 0 && free_slot < 0)
        {
            free_slot = i;
        }
    }

    if (enter)
    {
        if (i < STATS_DWELL_MAX)
        {
            g_dwell[i].enter_ms = now;
        }
        else if (free_slot >= 0)
        {
            g_dwell[free_slot].objId = objId;
            g_dwell[free_slot].enter_ms = now;
        }
    }
    else if (i < STATS_DWELL_MAX)
    {
        unsigned dt = now - g_dwell[i].enter_ms;

        if (dt > g_blob.max_dwell_ms)
        {
            g_blob.max_dwell_ms = dt;
        }
        g_dwell[i].objId = -1;
        g_dwell[i].enter_ms = 0;
    }
    pthread_mutex_unlock(&g_lock);

    stats_save_if_due(0);
}

void stats_on_visible(int count)
{
    if (count < 0)
    {
        count = 0;
    }

    pthread_mutex_lock(&g_lock);
    g_visible = count;
    if ((uint32_t)count > g_blob.max_people)
    {
        g_blob.max_people = (uint32_t)count;
    }
    if (g_sess_active && (uint32_t)count > g_sess_peak)
    {
        g_sess_peak = (uint32_t)count;
    }
    pthread_mutex_unlock(&g_lock);
}

void stats_session_begin(void)
{
    pthread_mutex_lock(&g_lock);
    g_sess_active = 1;
    g_sess_in = 0;
    g_sess_out = 0;
    g_sess_peak = 0;
    pthread_mutex_unlock(&g_lock);
}

void stats_session_end(unsigned *in, unsigned *out, unsigned *peak)
{
    pthread_mutex_lock(&g_lock);
    if (in)   *in = g_sess_in;
    if (out)  *out = g_sess_out;
    if (peak) *peak = g_sess_peak;
    g_sess_active = 0;
    pthread_mutex_unlock(&g_lock);
}

int stats_format(char *out, int cap)
{
    stats_blob_t b;
    unsigned     local = 0;
    unsigned     vis;
    int          synced;
    int          off;
    int          h;
    char         date[16] = "--";

    if (out == NULL || cap <= 0)
    {
        return 0;
    }

    synced = snap_time_get(&local);

    pthread_mutex_lock(&g_lock);
    stats_roll_locked(local, synced);
    b = g_blob;
    vis = (unsigned)g_visible;
    pthread_mutex_unlock(&g_lock);

    if (synced)
    {
        snap_time_name_at(local, date, sizeof(date), NULL, 0);
    }

    off = snprintf(out, cap,
                   "ok=1 synced=%d date=%s in_today=%u out_today=%u "
                   "in_total=%u out_total=%u area_in=%u area_out=%u "
                   "area_in_today=%u area_out_today=%u visible=%u "
                   "max_people=%u max_dwell_ms=%u last_kind=%u last_epoch=%u\n",
                   synced, date,
                   (unsigned)b.today_in, (unsigned)b.today_out,
                   (unsigned)b.in_total, (unsigned)b.out_total,
                   (unsigned)b.area_in_total, (unsigned)b.area_out_total,
                   (unsigned)b.area_in_today, (unsigned)b.area_out_today,
                   vis, (unsigned)b.max_people, (unsigned)b.max_dwell_ms,
                   (unsigned)b.last_kind, (unsigned)b.last_epoch);
    if (off < 0)
    {
        return 0;
    }

    for (h = 0; h < 24 && off < cap - 1; h++)
    {
        int n = snprintf(out + off, (size_t)(cap - off),
                         "h=%d in=%u out=%u\n", h,
                         (unsigned)b.hour_in[h], (unsigned)b.hour_out[h]);
        if (n < 0)
        {
            break;
        }
        off += n;
    }

    return (off < cap) ? off : (cap - 1);
}
