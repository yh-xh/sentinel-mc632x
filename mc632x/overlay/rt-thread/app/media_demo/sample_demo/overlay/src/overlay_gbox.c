#include "overlay/include/overlay_gbox.h"
#include "isp_config.h"

static char *model_name = MODEL_TAG_DEMO_OVERLAY_GBOX;

static FH_VOID checkParams(FH_SINT32 grp_id, FH_SINT32 chn_id, OSD_GBOX *gbox)
{
    FH_UINT32 chn_w = 0;
    FH_UINT32 chn_h = 0;
    FH_UINT32 vi_w = 0;
    FH_UINT32 vi_h = 0;

    chn_w = get_vpu_chn_w(grp_id, chn_id);
    chn_h = get_vpu_chn_h(grp_id, chn_id);

    /* OSD 画布是 VI(ISP) 幅面,比 VPU 通道幅面矮:
     * 本板 VI=1920x1080,而 CH0 报 1920x1088(缓冲按 16 对齐)。
     * FHAdv_Osd_Ex_SetGbox 内部是按 VI 幅面校验的
     * (libadvapi_osd 的 process_gbox: x+w>frameW 或 y+h>frameH 就整条返回 0xa0c84003),
     * 于是取满高的绊线(gbox.h=chn_h=1088)、以及贴底的检测框都会被拒掉。
     * 这里多按 VI 幅面夹一道,只夹不撑,正常大小的框不受影响。*/
    vi_w = get_isp_w(grp_id);
    vi_h = get_isp_h(grp_id);

    if (vi_w > 0 && vi_w < chn_w)
    {
        chn_w = vi_w;
    }

    if (vi_h > 0 && vi_h < chn_h)
    {
        chn_h = vi_h;
    }

    gbox->x = CLIP(gbox->x, 0, chn_w);
    gbox->y = CLIP(gbox->y, 0, chn_h);
    gbox->w = CLIP(gbox->w, 0, chn_w);
    gbox->h = CLIP(gbox->h, 0, chn_h);

    if (gbox->x + gbox->w > chn_w)
    {
        gbox->w = chn_w - gbox->x;
    }

    if (gbox->y + gbox->h > chn_h)
    {
        gbox->h = chn_h - gbox->y;
    }
}

static FH_VOID getChnSize(FH_SINT32 grp_id, FH_SINT32 chn_id, FH_SINT32 *w, FH_SINT32 *h)
{
#ifdef UVC_ENABLE
    *w = CHN0_MAX_WIDTH;
    *h = CHN0_MAX_HEIGHT;
#else
    *w = get_vpu_chn_w(grp_id, chn_id);
    *h = get_vpu_chn_h(grp_id, chn_id);
#endif
}

FH_SINT32 sample_set_gbox(FH_SINT32 grp_id, FH_SINT32 chn_id, OSD_GBOX *gbox)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FHT_OSD_Gbox_t gbox_cfg;
    FH_SINT32 w = 0, h = 0;

    memset(&gbox_cfg, 0, sizeof(gbox_cfg));

    SDK_CHECK_NULL_PTR(model_name, gbox);

    checkParams(grp_id, chn_id, gbox);
    getChnSize(grp_id, chn_id, &w, &h);

    /* 配置gbox 0 */
    gbox_cfg.enable = gbox->enable;  /* 使能gbox显示 */
    gbox_cfg.gboxId = gbox->gbox_id; /* gbox ID 为0 */
    gbox_cfg.max_frame_w = w;
    gbox_cfg.max_frame_h = h;

    if (gbox->enable)
    {
        FH_SINT32 lw = 5;                              /* 见下 */
        FH_SINT32 side = 0;

        gbox_cfg.rotate = 0;               /* 不旋转 */
        /* 原为 2。GBOX 走 BEFORE_VP,框画在 1080p 幅面上再缩到 800x480(约 0.42 倍),
         * 2px 到屏上只剩 0.83px 几乎看不见。5px -> 屏上约 2px。*/
        side = (gbox->w < gbox->h ? gbox->w : gbox->h) / 2; /* 框短边的一半 */

        /* 线宽不能超过短边一半,否则四条边糊成一个实心块(绊线只有几像素宽,必须压细)。*/
        if (side < 1)
        {
            side = 1;
        }

        if (lw > side)
        {
            lw = side;
        }

        gbox_cfg.gboxLineWidth = lw;
        gbox_cfg.area.fTopLeftX = gbox->x; /* gbox左上角起始点像素宽度位置 */
        gbox_cfg.area.fTopLeftY = gbox->y; /* gbox左上角起始点像素高度位置 */
        gbox_cfg.area.fWidth = gbox->w;    /* gbox宽度 */
        gbox_cfg.area.fHeigh = gbox->h;    /* gbox高度 */
        gbox_cfg.osdColor.fAlpha = gbox->color.fAlpha;
        gbox_cfg.osdColor.fRed = gbox->color.fRed;
        gbox_cfg.osdColor.fGreen = gbox->color.fGreen;
        gbox_cfg.osdColor.fBlue = gbox->color.fBlue;
    }
    else /*just work-around FHAdv_Osd_Ex_SetGbox,it will check fWidth and fHeight*/
    {
        gbox_cfg.gboxLineWidth = 2;
        gbox_cfg.area.fTopLeftX = 0;
        gbox_cfg.area.fTopLeftY = 0;
        gbox_cfg.area.fWidth = 16;
        gbox_cfg.area.fHeigh = 16;
        gbox_cfg.osdColor.fAlpha = gbox->color.fAlpha;
        gbox_cfg.osdColor.fRed = gbox->color.fRed;
        gbox_cfg.osdColor.fGreen = gbox->color.fGreen;
        gbox_cfg.osdColor.fBlue = gbox->color.fBlue;
    }

#ifdef FH_APP_OVERLAY_GBOX
    ret = FHAdv_Osd_Ex_SetGbox(grp_id, chn_id, &gbox_cfg);
    SDK_FUNC_ERROR(model_name, ret);
#endif

    return ret;

Exit:
    return FH_SDK_FAILED;
}
