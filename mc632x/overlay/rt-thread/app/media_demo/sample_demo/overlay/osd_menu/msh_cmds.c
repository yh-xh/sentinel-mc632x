/*
 * msh_cmds.c —— 给 msh / 网页终端用的简单命令
 *
 *   cam [0|1]         切换 LCD 画面源；0=G1 AI 叠加，1=G0 原始
 *   snap [0|1]        抓拍并存 SD；1=AI 叠加，0=干净原图（默认 1）
 *
 * 两个命令都只调用现成的 cam_switch / snap_store 接口，不直接碰 VOU 或 SD。
 * 串口 tshell 和网页 webterm 线程都可能调用，所以内部做了串行化。
 */
#include <rtthread.h>
#include <finsh.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "cam_switch.h"
#include "stream/include/snap_store.h"

/* 抓拍缓冲放静态区：webterm 线程栈只有 16K，不能放 256K 的局部数组。*/
static unsigned char   g_snap_buf[SNAP_JPEG_MAX];
static pthread_mutex_t g_snap_lock = PTHREAD_MUTEX_INITIALIZER;

static void cam_usage(void)
{
    rt_kprintf("usage: cam [0|1|ai|raw]\n");
    rt_kprintf("  0 / ai  = G1 带 AI 叠加\n");
    rt_kprintf("  1 / raw = G0 干净原图\n");
}

static int cmd_cam(int argc, char **argv)
{
    int mode;
    int cur;

    if (argc < 2)
    {
        cur = cam_switch_get();
        rt_kprintf("cam: current=%d (%s)\n", cur,
                   cur == CAM_MODE_G1 ? "G1 AI overlay" : "G0 raw");
        return 0;
    }

    if (!strcmp(argv[1], "ai") || !strcmp(argv[1], "g1"))
        mode = CAM_MODE_G1;
    else if (!strcmp(argv[1], "raw") || !strcmp(argv[1], "clean") ||
             !strcmp(argv[1], "g0"))
        mode = CAM_MODE_G0;
    else
    {
        char *end = NULL;

        mode = (int)strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0')
        {
            cam_usage();
            return 0;
        }
    }

    if (mode != CAM_MODE_G0 && mode != CAM_MODE_G1)
    {
        cam_usage();
        return 0;
    }

    if (cam_switch_set(mode) != 0)
    {
        rt_kprintf("cam: switch failed, current=%d\n", cam_switch_get());
        return 0;
    }

    cur = cam_switch_get();
    rt_kprintf("cam: switched to %d (%s)\n", cur,
               cur == CAM_MODE_G1 ? "G1 AI overlay" : "G0 raw");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_cam, cam, switch LCD source: cam 0=AI 1=raw);

static void snap_usage(void)
{
    rt_kprintf("usage: snap [0|1|ai|raw]\n");
    rt_kprintf("  1 / ai  = 抓拍带 AI 叠加的画面（默认）\n");
    rt_kprintf("  0 / raw = 抓拍 G0 干净原图\n");
}

static int cmd_snap(int argc, char **argv)
{
    char     path[96];
    unsigned seq = 0;
    int      cam = 1;
    int      n;

    if (argc > 1)
    {
        if (!strcmp(argv[1], "ai") || !strcmp(argv[1], "1"))
            cam = 1;
        else if (!strcmp(argv[1], "raw") || !strcmp(argv[1], "clean") ||
                 !strcmp(argv[1], "0"))
            cam = 0;
        else
        {
            snap_usage();
            return 0;
        }
    }

    pthread_mutex_lock(&g_snap_lock);
    n = snap_store_manual(cam, g_snap_buf, sizeof(g_snap_buf), 1,
                          path, sizeof(path), &seq);
    pthread_mutex_unlock(&g_snap_lock);

    if (n > 0)
    {
        rt_kprintf("snap: ok cam=%d size=%d seq=%u file=%s\n",
                   cam, n, seq, path);
        return 0;
    }

    switch (n)
    {
    case -1:
        rt_kprintf("snap: no MJPEG frame yet\n");
        break;
    case -2:
        rt_kprintf("snap: buffer too small\n");
        break;
    case -3:
        rt_kprintf("snap: SD not ready\n");
        break;
    case -4:
        rt_kprintf("snap: write failed\n");
        break;
    default:
        rt_kprintf("snap: failed (%d)\n", n);
        break;
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_snap, snap, save snapshot: snap 1=AI 0=raw);
