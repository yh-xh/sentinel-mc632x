/*
 * cam_switch.c —— LCD 画面源在 G0 / G1 两个摄像头之间切换
 *
 * 背景：
 *   VOU 框架写死了 1 层 1 通道（MAX_VIDEO_LAYER_NUM=1 / MAX_VIDEO_LAYER_CHN_NUM=1），
 *   vpu_bind_vou() 里 dst.chn_id 也是硬编码 0，所以屏幕上做不了画中画。
 *   那就一次显示一路，用户在网页上点一下切换。
 *
 * 两路源都是 800x480：
 *   G1 chn1 = 原送屏那路，带 OSD 叠加
 *   G0 chn1 = 专为送屏新加的通道（G0 chn0 是 1920x1088，
 *             VOU 不缩放，直接绑过去会是纯色）
 *
 * 换绑**只做解绑 -> 绑定**，中途一个 VOU 通道操作都不能有。
 * 实测：加上 DisableChn/EnableChn/重下发属性之后，能切成功两次，第三次
 * unbind 返回 0x81014102，紧接着整个 VOU 上下文报废（后续调用全返 0xA00F8006），
 * 屏幕停在纯色。去掉这些调用后可以任意次数来回切。
 * 另外解绑返回错误码不能直接退出 —— 它可能在"状态其实已经变了"的情况下返回
 * 非 0，以 FH_SYS_GetBindbyDest() 查到的实际绑定表为准。
 */
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "sample_common_media.h"
#include "dsp/fh_system_mpi.h"
#include "dsp/fh_vou_mpi.h"
#include "vpu_chn_config.h"
#include "cam_switch.h"
#include "rule_layer.h"

#define CAM_G1_GRP  1
#define CAM_G1_CHN  1
#define CAM_G0_GRP  0
#define CAM_G0_CHN  1

#define CAM_VO_LAYER  0
#define CAM_VO_CHN    0

/* 开机时 VOU 已经绑在 G1 chn1 上（CH1_BIND_VOU_DHD0_CHN0_G1） */
static int g_cam_mode = CAM_MODE_G1;

/* 网页、串口 msh、网页终端都可能同时切换，VOU 换绑必须串行。*/
static pthread_mutex_t g_cam_lock = PTHREAD_MUTEX_INITIALIZER;

static FH_BIND_INFO cam_vou_dst(void)
{
    FH_BIND_INFO dst;

    memset(&dst, 0, sizeof(dst));
    dst.obj_id = FH_OBJ_VOU;
    dst.dev_id = CAM_VO_LAYER;
    dst.chn_id = CAM_VO_CHN;
    return dst;
}

static FH_BIND_INFO cam_vpu_src(int grp, int chn)
{
    FH_BIND_INFO src;

    memset(&src, 0, sizeof(src));
    src.obj_id = FH_OBJ_VPU_VO;
    src.dev_id = grp;
    src.chn_id = chn;
    return src;
}

/* 查当前 VOU 接的是谁。返回 0 且 src 有值表示查到了 */
static int cam_vou_get_src(FH_BIND_INFO *src)
{
    FH_BIND_INFO dst = cam_vou_dst();

    memset(src, 0, sizeof(*src));
    return FH_SYS_GetBindbyDest(src, dst);
}

static void cam_dump_bind(const char *tag)
{
    FH_BIND_INFO src;
    int          r;

    r = cam_vou_get_src(&src);
    printf("[cam] %s: VOU <- obj=%d dev=%d chn=%d (ret=%d)\n",
           tag, (int)src.obj_id, (int)src.dev_id, (int)src.chn_id, r);
}

/* VOU 当前是否接在 grp/chn 上 */
static int cam_vou_is_src(int grp, int chn)
{
    FH_BIND_INFO cur;
    int          r;

    r = cam_vou_get_src(&cur);
    if (r != 0)
        return 0;

    return (cur.obj_id == FH_OBJ_VPU_VO &&
            cur.dev_id == (FH_UINT32)grp &&
            cur.chn_id == (FH_UINT32)chn);
}

/*
 * 把 VOU 接到指定源。返回 0 = 查表确认已接上。
 *
 * 实测记录（MC632X）：
 *   - 直接 FH_SYS_Bind(新源, VOU) 不支持替换，返回 0x81014100。
 *   - "DisableChn -> unbind -> bind -> 属性 -> EnableChn" 能成功两次，
 *     第三次 unbind 返回 0x81014102，之后 VOU 所有调用都变成 0xA00F8006
 *     （连 GetVideoLayerAttr 都失败），整个 VOU 上下文被打坏。
 *   - 只做 unbind -> bind（没有任何 VOU 通道操作）时可以无限次反复切换。
 *
 * 所以这里**完全不动 VOU 通道**，只做解绑+绑定。
 */
static int cam_vou_route(int grp, int chn)
{
    FH_BIND_INFO src = cam_vpu_src(grp, chn);
    FH_BIND_INFO dst = cam_vou_dst();
    FH_SINT32    ret;

    /* 1. 解绑当前源。返回值不可信，成败以第 3 步的绑定表为准。*/
    ret = FH_SYS_UnBindbyDst(dst);
    if (ret != 0)
        printf("[cam] unbind ret=0x%x (ignored)\n", ret);

    /* 2. 绑新源 */
    ret = FH_SYS_Bind(src, dst);
    if (ret != 0)
        printf("[cam] bind G%d CHN%d ret=0x%x\n", grp, chn, ret);

    /* 3. 以绑定表为准判断成败 */
    if (cam_vou_is_src(grp, chn))
        return 0;

    printf("[cam] bind NOT effective (want G%d CHN%d)\n", grp, chn);
    cam_dump_bind("actual");
    return -1;
}

int cam_switch_get(void)
{
    int mode;

    pthread_mutex_lock(&g_cam_lock);
    mode = g_cam_mode;
    pthread_mutex_unlock(&g_cam_lock);
    return mode;
}

static int cam_switch_set_locked(int mode)
{
    VPU_CHN_INFO *info;
    int           grp, chn;

    if (mode != CAM_MODE_G0 && mode != CAM_MODE_G1)
        return -1;

    if (mode == g_cam_mode)
        return 0;

    if (mode == CAM_MODE_G0)
    {
        grp = CAM_G0_GRP;
        chn = CAM_G0_CHN;
    }
    else
    {
        grp = CAM_G1_GRP;
        chn = CAM_G1_CHN;
    }

    info = get_vpu_chn_config(grp, chn);
    if (!info || !info->enable)
    {
        printf("[cam] VPU[%d] CHN[%d] not enable\n", grp, chn);
        return -1;
    }

    if (cam_vou_route(grp, chn) == 0)
    {
        g_cam_mode = mode;
        /* 规则线图层只属于 G1 的 AI 画面；切到 G0 原始画面要藏起来 */
        rule_layer_set_cam(mode == CAM_MODE_G1);
        printf("[cam] switched to G%d CHN%d (%ux%u)\n",
               grp, chn, info->width, info->height);
        return 0;
    }

    /* 失败：无条件回到 G1，别把屏幕留在纯色 */
    if (grp != CAM_G1_GRP || chn != CAM_G1_CHN)
    {
        printf("[cam] rollback to G%d CHN%d\n", CAM_G1_GRP, CAM_G1_CHN);
        cam_vou_route(CAM_G1_GRP, CAM_G1_CHN);
    }
    g_cam_mode = CAM_MODE_G1;
    return -1;
}

int cam_switch_set(int mode)
{
    int ret;

    pthread_mutex_lock(&g_cam_lock);
    ret = cam_switch_set_locked(mode);
    pthread_mutex_unlock(&g_cam_lock);
    return ret;
}
