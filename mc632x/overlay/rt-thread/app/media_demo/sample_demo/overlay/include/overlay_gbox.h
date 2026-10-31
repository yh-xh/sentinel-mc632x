#ifndef __OVERLAY_GBOX_H__
#define __OVERLAY_GBOX_H__
#include "sample_common_media.h"
#include "dsp_ext/FHAdv_OSD_mpi.h"
#include "vpu_chn_config.h"

#define MAX_GBOX_LAYER_W (CHN0_MAX_WIDTH)
#define MAX_GBOX_LAYER_H (CHN0_MAX_HEIGHT)

/* 必须是 BEFORE_VP(分通道前)。原因:
 * AFTER_VP 下每个通道各要一层 GBOX,而 OSD 库只给 chn0 分配了层,
 * 对 chn1(本板送屏那路)调 FHAdv_Osd_Ex_SetGbox 会返回 0xa0c8 直接失败。
 * BEFORE_VP 只需配 chn0,即对整个 group 生效,所有通道(含送屏的 chn1)都带框。
 * 代价:框画在 1080p 幅面上再缩到 800x480,所以线宽要在 overlay_gbox.c 里放大。*/
#define SAMPLE_GBOX_GRAPH FHT_OSD_GRAPH_CTRL_GBOX_BEFORE_VP  /* 设置GBOX在VPU分通道前 */

typedef struct
{
    FH_UINT32 enable;     /* gbox使能 0: 关闭  1: 开启 */
    FH_UINT32 gbox_id;    /* gbox id */
    FH_SINT32 x;          /*矩形框x坐标  */
    FH_SINT32 y;          /*矩形框y坐标  */
    FH_SINT32 w;          /* 矩形框宽度　 */
    FH_SINT32 h;          /* 矩形框高度　 */
    FHT_RgbColor_t color; /* 矩形框颜色　 */
} OSD_GBOX;

FH_SINT32 sample_set_gbox(FH_SINT32 grp_id, FH_SINT32 chn_id, OSD_GBOX *gbox);

#endif /*__OVERLAY_GBOX_H__*/
