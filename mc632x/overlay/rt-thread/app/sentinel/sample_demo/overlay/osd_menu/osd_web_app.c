/*
 * osd_web_app.c —— 网页抓拍 / AI 叠加开关 / 事件列表
 *
 * 抓拍数据来自 mjpeg_cache 的最新帧，不新增 VENC 抓拍通道，
 * 因此不会和 MJPEG 的 GetStream 并发冲突。
 *
 * 所有处理都在 8080 的单线程里，g_snap_buf / g_ev_text 是静态的，
 * 不需要加锁；跨线程的只有 overlay 的 volatile 标志和 snap_store 的环。
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "osd_web_key.h"
#include "osd_web_app.h"
#include "rule_layer.h"
#include "nna_app/include/sample_common_nna.h"
#include "stream/include/mjpeg_cache.h"
#include "stream/include/snap_store.h"
#include "stream/include/snap_time.h"
#include "stream/include/rec_store.h"

static unsigned char g_snap_buf[SNAP_JPEG_MAX];
static char          g_ev_text[6144];
static char          g_files_text[12288];
static char          g_text_buf[8192];

/* 布防状态：布防时自动存图 + 允许小智播报；撤防时只检测不存证。*/
static volatile int  g_armed = 0;

static int overlay_get(void)
{
#ifdef FH_NN_ENABLE
    return sample_nna_overlay_get();
#else
    return 1;
#endif
}

static void overlay_set(int on)
{
#ifdef FH_NN_ENABLE
    sample_nna_overlay_set(on);
#else
    (void)on;
#endif
    /* 绊线/周界框只画在 LCD 图形层上，跟着"显示叠加"一起开关 */
    rule_layer_set_overlay(on);
}

/* /api/snap?cam=0|1[&save=1][&ret=1] */
static void snap_api(int fd, const char *query)
{
    int      cam  = osd_web_query_get_int(query, "cam", 1);
    int      save = osd_web_query_get_int(query, "save", 0);
    int      ret_img = osd_web_query_get_int(query, "ret", 0);
    int      n;
    char     path[64] = "";
    unsigned seq = 0;

    cam = (cam == 0) ? 0 : 1;       /* 0=G0 干净, 1=G1 AI */

    if (save)
    {
        n = snap_store_manual(cam, g_snap_buf, (int)sizeof(g_snap_buf),
                              1, path, (int)sizeof(path), &seq);
        if (n <= 0)
        {
            const char *err = (n == -1) ? "no_frame" :
                              (n == -2) ? "too_large" :
                              (n == -3) ? "sd_not_ready" : "write_failed";
            char body[64];

            snprintf(body, sizeof(body), "ok=0 err=%s\n", err);
            osd_web_send(fd, "503 Service Unavailable",
                         "text/plain; charset=utf-8", body);
            return;
        }

        if (ret_img)
        {
            char extra[128];

            snprintf(extra, sizeof(extra),
                     "X-Snap-Cam: %d\r\nX-Snap-Seq: %u\r\nX-Snap-File: %s",
                     cam, seq, path);
            osd_web_send_bin_ex(fd, "200 OK", "image/jpeg",
                                g_snap_buf, n, extra);
        }
        else
        {
            char body[192];

            snprintf(body, sizeof(body),
                     "ok=1 cam=%d size=%d seq=%u file=%s\n",
                     cam, n, seq, path);
            osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
        }
        return;
    }

    /* 只回图，不落盘 */
    n = snap_store_manual(cam, g_snap_buf, (int)sizeof(g_snap_buf),
                          0, NULL, 0, NULL);
    if (n <= 0)
    {
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", "ok=0 err=no_frame\n");
        return;
    }

    osd_web_send_bin(fd, "200 OK", "image/jpeg", g_snap_buf, n);
}

/* /api/overlay[?on=0|1] */
static void overlay_api(int fd, const char *query)
{
    int  on = osd_web_query_get_int(query, "on", -1);
    char body[32];

    if (on == 0 || on == 1)
    {
        overlay_set(on);
    }

    snprintf(body, sizeof(body), "overlay=%d\n", overlay_get());
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/auto[?on=0|1] —— 绊线/周界自动抓拍开关 */
static void auto_api(int fd, const char *query)
{
    int  on = osd_web_query_get_int(query, "on", -1);
    char body[16];

    if (on == 0 || on == 1)
    {
        snap_store_auto_set(on);
        if (on == 0)
        {
            g_armed = 0;        /* 单独关自动抓拍也视为撤防 */
        }
    }

    snprintf(body, sizeof(body), "auto=%d\n", snap_store_auto_get());
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/arm[?on=0|1] —— 布防/撤防。布防 = 自动存图 + 允许小智播报。*/
static void arm_api(int fd, const char *query)
{
    int  on = osd_web_query_get_int(query, "on", -1);
    char body[32];

    if (on == 0 || on == 1)
    {
        g_armed = on;
        snap_store_auto_set(on);
        if (on)
        {
            overlay_set(1);
        }
    }

    snprintf(body, sizeof(body), "armed=%d\n", g_armed);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/status —— 给网页看板和小智查询的一行状态。*/
static void status_api(int fd, const char *query)
{
    int      people = 0;
    int      conf   = 0;
    unsigned model  = 0;
    char     body[256];

    (void)query;

#ifdef FH_NN_ENABLE
    people = sample_nna_person_count();
    conf   = sample_nna_last_conf();
    model  = nna_get_model_case();
#endif

    snprintf(body, sizeof(body),
             "armed=%d auto=%d overlay=%d sd=%d people=%d conf=%d "
             "model=%u count=%d test=%d\n",
             g_armed, snap_store_auto_get(), overlay_get(),
             snap_store_sd_ready(), people, conf, model,
             snap_store_event_count(), snap_store_test_get());
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/time[?t=<utc_epoch>&tz=<分钟>] —— 网页把浏览器时间同步给板子 */
static void time_api(int fd, const char *query)
{
    int      t  = osd_web_query_get_int(query, "t", 0);
    int      tz = osd_web_query_get_int(query, "tz", 480);
    unsigned local = 0;
    int      synced;
    char     body[64];

    if (t > 0)
    {
        snap_time_set((unsigned)t, tz);
    }

    synced = snap_time_get(&local);
    snprintf(body, sizeof(body), "synced=%d t=%u\n", synced, local);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/clean[?keep=100] —— 只保留最新 keep 个 JPEG */
static void clean_api(int fd, const char *query)
{
    int  keep = osd_web_query_get_int(query, "keep", 100);
    int  n;
    char body[64];

    if (keep < 1)
    {
        keep = 1;
    }
    if (keep > 10000)
    {
        keep = 10000;
    }

    n = snap_store_clean_files(keep);
    if (n < 0)
    {
        snprintf(body, sizeof(body), "ok=0 err=%s\n",
                 (n == -3) ? "sd_not_ready" : "scan_failed");
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", body);
        return;
    }

    snprintf(body, sizeof(body), "ok=1 deleted=%d keep=%d\n", n, keep);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/space —— 按需查询 SD 容量（f_getfree 会扫 FAT，别放在相册刷新里） */
static void space_api(int fd, const char *query)
{
    unsigned free_kb = 0;
    unsigned total_kb = 0;
    char     body[96];

    (void)query;

    if (snap_store_space(&free_kb, &total_kb) != 0)
    {
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", "ok=0\n");
        return;
    }

    snprintf(body, sizeof(body), "ok=1 freekb=%u totalkb=%u\n",
             free_kb, total_kb);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/sd/remount[?force=1] —— 重新挂载 /mnt */
static void remount_api(int fd, const char *query)
{
    int force = osd_web_query_get_int(query, "force", 1);
    int ret;
    int i;
    char body[48];

    /*
     * 录像线程从开始到结束一直开着 AVI 文件，FatFs 有打开的文件时拆不掉卷，
     * 以前 remount 失败多半就是这个原因（按钮看起来像摆设）。先停录像，
     * 等 rec 线程 finalize + close 完成再重新挂载。
     */
    if (rec_store_is_recording())
    {
        rec_store_manual_stop();
        for (i = 0; i < 50 && rec_store_is_recording(); i++)
        {
            usleep(100 * 1000);
        }
        if (rec_store_is_recording())
        {
            osd_web_send(fd, "503 Service Unavailable",
                         "text/plain; charset=utf-8", "ok=0 err=recording\n");
            return;
        }
        usleep(300 * 1000);     /* 给 close/目录项落盘留一点时间 */
    }

    ret = snap_store_remount(force ? 1 : 0);
    if (ret == 0)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=1\n");
        return;
    }

    snprintf(body, sizeof(body), "ok=0 err=%s\n",
             (ret == -2) ? "busy" : "mount");
    osd_web_send(fd, "503 Service Unavailable",
                 "text/plain; charset=utf-8", body);
}

/* "/mnt/snap/left/xxx.jpg" -> "left/xxx.jpg"；不是 /mnt/snap 下的原样返回。*/
static const char *snap_rel_path(const char *p)
{
    size_t n = sizeof(SNAP_DIR) - 1;

    if (p != NULL && strncmp(p, SNAP_DIR "/", n + 1) == 0)
    {
        return p + n + 1;
    }

    return (p != NULL) ? p : "";
}

/* /api/events[?n=5] */
static void events_api(int fd, const char *query)
{
    snap_event_t ev[SNAP_EVENT_RING_NUM];
    int          n = osd_web_query_get_int(query, "n", 5);
    int          count;
    int          i;
    int          off;

    if (n < 1)
        n = 1;
    if (n > SNAP_EVENT_RING_NUM)
        n = SNAP_EVENT_RING_NUM;

    off = snprintf(g_ev_text, sizeof(g_ev_text),
                   "overlay=%d sd=%d auto=%d armed=%d test=%d count=%d dropped=%u\n",
                   overlay_get(), snap_store_sd_ready(), snap_store_auto_get(),
                   g_armed, snap_store_test_get(),
                   snap_store_event_count(), snap_store_dropped());
    if (off < 0)
        off = 0;

    count = snap_store_events(ev, n);
    for (i = 0; i < count; i++)
    {
        /* 文件名输出相对 SNAP_DIR 的路径（如 left/evt_...jpg），
         * 浏览器才能按摄像头子目录取图。*/
        const char *file = snap_rel_path(ev[i].file);
        const char *clean = snap_rel_path(ev[i].clean);
        const char *clip = snap_rel_path(ev[i].clip);
        int         wrote;

        if (off >= (int)sizeof(g_ev_text) - 1)
            break;

        wrote = snprintf(g_ev_text + off, sizeof(g_ev_text) - off,
                         "ev seq=%u time=%s kind=%u mode=%u rule=%d "
                         "conf=%u count=%u size=%u ok=%u clean_ok=%u label=%d "
                         "clip_ok=%u clip_size=%u file=%s clean=%s clip=%s\n",
                         ev[i].seq, ev[i].time, ev[i].kind, ev[i].mode,
                         ev[i].rule, ev[i].conf, ev[i].count,
                         ev[i].size, ev[i].ok, ev[i].clean_ok, ev[i].label,
                         ev[i].clip_ok, ev[i].clip_size,
                         file, clean, clip);
        if (wrote < 0)
            break;
        off += wrote;
    }

    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_ev_text);
}

/* /api/event.jpg[?id=N] */
static void event_jpg_api(int fd, const char *query)
{
    int          id = osd_web_query_get_int(query, "id", -1);
    int          n;
    snap_event_t newest;

    if (id < 0)
    {
        if (snap_store_events(&newest, 1) != 1)
        {
            osd_web_send(fd, "404 Not Found",
                         "text/plain; charset=utf-8", "no event\n");
            return;
        }
        id = (int)newest.seq;
    }

    n = snap_store_read_event((unsigned)id, g_snap_buf, (int)sizeof(g_snap_buf));
    if (n <= 0)
    {
        osd_web_send(fd, "404 Not Found",
                     "text/plain; charset=utf-8", "event image not found\n");
        return;
    }

    osd_web_send_bin(fd, "200 OK", "image/jpeg", g_snap_buf, n);
}

/* /api/files[?n=20] —— 列 SD 卡相册（不依赖内存事件环，重启后也能看） */
static void files_api(int fd, const char *query)
{
    int n = osd_web_query_get_int(query, "n", 20);
    int len;

    if (n < 1)
    {
        n = 1;
    }
    if (n > 256)
    {
        n = 256;
    }

    len = snap_store_list_files(g_files_text, (int)sizeof(g_files_text), n);
    if (len < 0)
    {
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", "sd=0\n");
        return;
    }

    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_files_text);
}

/* /api/file?name=snap_xxx.jpg —— 按文件名回原图 */
static void file_api(int fd, const char *query)
{
    char name[SNAP_FILE_NAME_MAX];
    char extra[SNAP_FILE_NAME_MAX + 32];
    int  n;

    if (!osd_web_query_get_str(query, "name", name, sizeof(name)))
    {
        osd_web_send(fd, "400 Bad Request",
                     "text/plain; charset=utf-8", "missing name\n");
        return;
    }

    n = snap_store_read_file(name, g_snap_buf, (int)sizeof(g_snap_buf));
    if (n == -2)
    {
        osd_web_send(fd, "413 Payload Too Large",
                     "text/plain; charset=utf-8", "file too large\n");
        return;
    }
    if (n == -3)
    {
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", "sd not ready\n");
        return;
    }
    if (n <= 0)
    {
        osd_web_send(fd, "404 Not Found",
                     "text/plain; charset=utf-8", "file not found\n");
        return;
    }

    /* 文件长度可能对但缺 EOI；日志里记一笔，区分是 SD 内容坏还是 HTTP 截断 */
    if (n >= 4 &&
        (g_snap_buf[0] != 0xFF || g_snap_buf[1] != 0xD8 ||
         g_snap_buf[n - 2] != 0xFF || g_snap_buf[n - 1] != 0xD9))
    {
        printf("[osd_web] serve incomplete jpeg %s n=%d tail=%02x%02x\n",
               name, n, (unsigned)g_snap_buf[n - 2], (unsigned)g_snap_buf[n - 1]);
    }

    snprintf(extra, sizeof(extra), "X-Snap-File: %s", name);
    osd_web_send_bin_ex(fd, "200 OK", "image/jpeg", g_snap_buf, n, extra);
}

/* /api/delete?name=snap_xxx.jpg —— 删除 SD 卡上的一张照片 */
static void delete_api(int fd, const char *query)
{
    char name[SNAP_FILE_NAME_MAX];
    char body[SNAP_FILE_NAME_MAX + 32];
    int  ret;

    if (!osd_web_query_get_str(query, "name", name, sizeof(name)))
    {
        osd_web_send(fd, "400 Bad Request",
                     "text/plain; charset=utf-8", "missing name\n");
        return;
    }

    ret = snap_store_delete_file(name);
    if (ret == -4)
    {
        osd_web_send(fd, "400 Bad Request",
                     "text/plain; charset=utf-8", "bad name\n");
        return;
    }
    if (ret == -3)
    {
        osd_web_send(fd, "503 Service Unavailable",
                     "text/plain; charset=utf-8", "sd not ready\n");
        return;
    }
    if (ret != 0)
    {
        osd_web_send(fd, "404 Not Found",
                     "text/plain; charset=utf-8", "file not found\n");
        return;
    }

    snprintf(body, sizeof(body), "ok=1 name=%s\n", name);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/test[?on=0|1] —— 测试模式：冷却 300ms + detect.csv */
static void test_api(int fd, const char *query)
{
    char body[48];
    int  on = osd_web_query_get_int(query, "on", -1);

    if (on == 0 || on == 1)
        snap_store_test_set(on);

    snprintf(body, sizeof(body), "test=%d cooldown=%d\n",
             snap_store_test_get(),
             snap_store_test_get() ? SNAP_TEST_COOLDOWN_MS : SNAP_EVENT_COOLDOWN_MS);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/label?seq=N&v=1|0  或  /api/label?fn=1 */
static void label_api(int fd, const char *query)
{
    char body[64];
    int  fn  = osd_web_query_get_int(query, "fn", 0);
    int  seq = osd_web_query_get_int(query, "seq", -1);
    int  v   = osd_web_query_get_int(query, "v", -1);
    int  ret;

    if (fn == 1)
    {
        ret = snap_store_label_miss();
    }
    else if (seq > 0 && (v == 0 || v == 1))
    {
        ret = snap_store_label_event((unsigned)seq, v);
    }
    else
    {
        osd_web_send(fd, "400 Bad Request", "text/plain; charset=utf-8",
                     "use seq&v or fn=1\n");
        return;
    }

    snprintf(body, sizeof(body), "ok=%d ret=%d\n", ret == 0 ? 1 : 0, ret);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

/* /api/sd/selftest —— 写/读/校验/删除 64KB 测试文件 */
static void sd_selftest_api(int fd, const char *query)
{
    char msg[96];

    (void)query;
    snap_store_selftest(msg, sizeof(msg));
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", msg);
}

/* /api/testlog  /api/labels —— 下载测试记录 */
static void text_log_api(int fd, const char *name, const char *filename, int tail)
{
    char extra[96];
    int  n;

    n = snap_store_read_text(name, g_text_buf, sizeof(g_text_buf), tail);
    if (n < 0)
    {
        osd_web_send(fd, "404 Not Found", "text/plain; charset=utf-8",
                     n == -3 ? "sd not ready\n" : "no log\n");
        return;
    }

    snprintf(extra, sizeof(extra),
             "Content-Disposition: attachment; filename=\"%s\"", filename);
    osd_web_send_bin_ex(fd, "200 OK", "text/csv; charset=utf-8",
                        g_text_buf, n, extra);
}

int osd_web_app_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/snap"))
    {
        snap_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/overlay"))
    {
        overlay_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/events"))
    {
        events_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/event.jpg"))
    {
        event_jpg_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/files"))
    {
        files_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/file"))
    {
        file_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/delete"))
    {
        delete_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/auto"))
    {
        auto_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/arm"))
    {
        arm_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/status"))
    {
        status_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/time"))
    {
        time_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/clean"))
    {
        clean_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/space"))
    {
        space_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/sd/remount"))
    {
        remount_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/sd/selftest"))
    {
        sd_selftest_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/test"))
    {
        test_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/label"))
    {
        label_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/testlog"))
    {
        text_log_api(fd, "detect.csv", "detect.csv", 1);
        return 1;
    }
    if (!strcmp(path, "/api/labels"))
    {
        text_log_api(fd, "labels.csv", "labels.csv", 0);
        return 1;
    }

    return 0;
}
