/*
 * rule_layer.c —— LCD 专用规则线图层（VOU graphic layer）
 *
 * 背景：
 *   绊线/周界框以前画在 VPU 的 OSD 上，OSD 在编码前合成，画上去就会进
 *   MJPEG/H.264，事件视频里必然带着。现在拆成两条路：
 *     - 编码流（网页预览 / 事件视频 / RTSP）：只有动态人形框；
 *     - LCD：用 VOU 图形层叠一张 ARGB1555 位图，只影响显示。
 *
 * 图形层是一张 800x480 全屏位图，几何跟原来 OSD 规则完全一致：
 *   绊线 x = W/2、周界框 x=1/6..2/6 与 4/6..5/6、y=1/6..5/6。
 *
 * 初始化在 _vlcview() 之后做；任何一步失败都只打印并放弃，
 * 视频流水线和网页不受影响（LCD 最多是看不到规则线）。
 */
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "sample_common_media.h"
#include "dsp/fh_vou_mpi.h"
#include "rule_layer.h"

#define RL_W        800
#define RL_H        480
#define RL_LAYER    0
#define RL_VODEV    0

/* ARGB1555：bit15 = alpha，1 不透明；红色 = R31 G0 B0 */
#define RL_RED      0xFC00u

static MEM_DESC       g_mem;
static unsigned char *g_vbuf;
static int            g_ready;
static int            g_overlay_on = 1;
static int            g_cam_g1     = 1;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned short *rl_row(int y)
{
    return (unsigned short *)(g_vbuf + (size_t)y * RL_W * 2);
}

/* 画一个 2px 边框的矩形 */
static void rl_rect(int x0, int y0, int x1, int y1, int t)
{
    int x, y, i;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= RL_W) x1 = RL_W - 1;
    if (y1 >= RL_H) y1 = RL_H - 1;

    for (y = y0; y <= y1; y++)
    {
        unsigned short *p = rl_row(y);

        if (y < y0 + t || y > y1 - t)
        {
            for (x = x0; x <= x1; x++)
                p[x] = (unsigned short)RL_RED;
        }
        else
        {
            for (i = 0; i < t && x0 + i <= x1; i++)
                p[x0 + i] = (unsigned short)RL_RED;
            for (i = 0; i < t && x1 - i >= x0; i++)
                p[x1 - i] = (unsigned short)RL_RED;
        }
    }
}

static void rl_vline(int x, int y0, int y1, int t)
{
    int y, i;

    for (y = y0; y <= y1; y++)
    {
        unsigned short *p = rl_row(y);

        for (i = 0; i < t; i++)
        {
            int xx = x + i;

            if (xx >= 0 && xx < RL_W)
                p[xx] = (unsigned short)RL_RED;
        }
    }
}

static void rl_draw(void)
{
    memset(g_vbuf, 0, (size_t)RL_W * RL_H * 2);

    /* 绊线：画面中间竖线，3px 宽，通高 */
    rl_vline(RL_W / 2 - 1, 0, RL_H - 1, 3);

    /* 周界框 1 / 2：和原 OSD 规则同一几何 */
    rl_rect(RL_W / 6,     RL_H / 6, RL_W * 2 / 6, RL_H * 5 / 6, 2);
    rl_rect(RL_W * 4 / 6, RL_H / 6, RL_W * 5 / 6, RL_H * 5 / 6, 2);
}

static int rl_upload(void)
{
    VIDEO_FRAME_INFO_S frm;

    memset(&frm, 0, sizeof(frm));
    frm.u32PoolId = -1;
    frm.stVFrame.enCompressMode = COMPRESS_MODE_NONE;
    frm.stVFrame.enVideoFormat  = VIDEO_FORMAT_LINEAR;
    frm.stVFrame.enPixelFormat  = PIXEL_FORMAT_RGB_1555;
    frm.stVFrame.u32Width  = RL_W;
    frm.stVFrame.u32Height = RL_H;
    frm.stVFrame.u32Field  = VIDEO_FIELD_FRAME;
    frm.stVFrame.u32PhyAddr[0] = g_mem.base;
    frm.stVFrame.u32Stride[0]  = RL_W;
    frm.stVFrame.u64pts = 0;

    return FH_MPI_VO_UpdateGraphicLayer(RL_LAYER, &frm, 0);
}

static void rl_apply_locked(void)
{
    if (!g_ready)
        return;

    /* 用位图内容控制显隐，不依赖 Show/Hide：
     *  - 有些版本 Hide 没实现，直接返回错误；
     *  - ARGB1555 打开 alpha channel 后，全局 alpha 也不参与混合。
     * 隐藏时把整张位图清空再上传，效果等价于完全透明。*/
    if (g_overlay_on && g_cam_g1)
    {
        rl_draw();
    }
    else
    {
        memset(g_vbuf, 0, (size_t)RL_W * RL_H * 2);
    }

    rl_upload();
}

int rule_layer_init(void)
{
    FH_POINT pos  = {0, 0};
    FH_SIZE  size;
    ALPHA_S  alpha;
    int      ret;

    pthread_mutex_lock(&g_lock);
    if (g_ready)
    {
        pthread_mutex_unlock(&g_lock);
        return 0;
    }

    memset(&alpha, 0, sizeof(alpha));
    memset(&size, 0, sizeof(size));
    size.u32Width  = RL_W;
    size.u32Height = RL_H;

    ret = buffer_malloc_withname(&g_mem, RL_W * RL_H * 2, 1024, "rule_layer");
    if (ret != 0)
    {
        printf("[rule_layer] buffer_malloc %dKB failed: %d\n",
               RL_W * RL_H * 2 / 1024, ret);
        pthread_mutex_unlock(&g_lock);
        return -1;
    }

    g_vbuf = (unsigned char *)g_mem.vbase;
    rl_draw();

    ret = FH_MPI_VO_EnableGraphicLayer(RL_LAYER);
    if (ret == 0)
        ret = FH_MPI_VO_BindGraphicLayer(RL_LAYER, RL_VODEV);
    if (ret == 0)
        ret = FH_MPI_VO_SetGraphicLayerPos(RL_LAYER, &pos);
    if (ret == 0)
        ret = FH_MPI_VO_SetGraphicLayerSize(RL_LAYER, &size);
    if (ret == 0)
    {
        alpha.bAlphaEnable  = 1;
        alpha.bAlphaChannel = 1;
        alpha.u8Alpha0      = 0;
        alpha.u8Alpha1      = 255;
        alpha.u8GlobalAlpha = 255;
        ret = FH_MPI_VO_SetGraphicLayerAlpha(RL_LAYER, &alpha);
    }
    if (ret == 0)
        ret = rl_upload();

    if (ret != 0)
    {
        printf("[rule_layer] VOU graphic layer init failed: 0x%x\n", ret);
        pthread_mutex_unlock(&g_lock);
        return -1;
    }

    g_ready = 1;
    rl_apply_locked();
    printf("[rule_layer] ready, %dx%d ARGB1555\n", RL_W, RL_H);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

void rule_layer_set_overlay(int on)
{
    pthread_mutex_lock(&g_lock);
    g_overlay_on = on ? 1 : 0;
    rl_apply_locked();
    pthread_mutex_unlock(&g_lock);
}

void rule_layer_set_cam(int g1)
{
    pthread_mutex_lock(&g_lock);
    g_cam_g1 = g1 ? 1 : 0;
    rl_apply_locked();
    pthread_mutex_unlock(&g_lock);
}
