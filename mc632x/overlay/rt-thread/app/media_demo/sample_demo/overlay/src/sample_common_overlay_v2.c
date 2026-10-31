#include "overlay/include/sample_common_overlay.h"

static char *model_name = MODEL_TAG_DEMO_OVERLAY;

static FH_UINT32 g_overlay_inited[MAX_GRP_NUM] = {0};

static FH_SINT32 sample_osd_init(FH_SINT32 grp_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_UINT32 graph_ctrl = 0;

#ifdef FH_APP_OVERLAY_LOGO
    graph_ctrl |= SAMPLE_LOGO_GRAPH;
#endif

#ifdef FH_APP_OVERLAY_TOSD
    graph_ctrl |= SAMPLE_TOSD_GRAPH;
#endif

#ifdef FH_APP_OVERLAY_GBOX
    graph_ctrl |= SAMPLE_GBOX_GRAPH;

    ret = FHAdv_Osd_Config_GboxPixel(grp_id, FH_APP_OVERLAY_GBOX_BIT, FHTEN_OSD_GBOX_Pixel);
    SDK_FUNC_ERROR(model_name, ret);

#ifndef FH_APP_OVERLAY_GBOX_TWOBUF
    // 设置为单buffer
    ret = FHAdv_Osd_Config_GboxExt(grp_id, 0, FHTEN_OSD_GBOX_Pixel, FHTEN_OSD_BOX_ONEBUF, 0);
    SDK_FUNC_ERROR(model_name, ret);
#endif
#endif

#ifdef FH_APP_OVERLAY_MASK
    graph_ctrl |= SAMPLE_MASK_GRAPH;
#endif

    /* 初始化OSD */
    ret = FHAdv_Osd_Init(grp_id, FHT_OSD_DEBUG_LEVEL_ERROR, graph_ctrl, 0, 0);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 sample_fh_overlay_start(FH_SINT32 grp_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    SDK_CHECK_MAX_VALID(model_name, grp_id, MAX_GRP_NUM, "grp_id[%d] > MAX_GRP_NUM[%d]\n", grp_id, MAX_GRP_NUM);

    if (!g_overlay_inited[grp_id])
    {
        ret = sample_osd_init(grp_id);
        SDK_FUNC_ERROR(model_name, ret);

        g_overlay_inited[grp_id] = 1;

        SDK_PRT(model_name, "OSD[%d] init success!\n", grp_id);
    }

#ifdef FH_APP_OVERLAY_LOGO
    ret = sample_set_logo(grp_id);
    SDK_FUNC_ERROR(model_name, ret);
#endif

#ifdef FH_APP_OVERLAY_TOSD
    ret = sample_set_tosd(grp_id);
    SDK_FUNC_ERROR(model_name, ret);
#ifdef FH_APP_OVERLAY_TOSD_MENU
    osd_menu_init(NULL);
#endif
#endif

#ifdef FH_APP_OVERLAY_MASK
    ret = sample_set_mask(grp_id);
    SDK_FUNC_ERROR(model_name, ret);
#endif

    return ret;
}

FH_SINT32 sample_fh_overlay_stop(FH_SINT32 grp_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    SDK_CHECK_MAX_VALID(model_name, grp_id, MAX_GRP_NUM, "grp_id[%d] > MAX_GRP_NUM[%d]\n", grp_id, MAX_GRP_NUM);

    if (g_overlay_inited[grp_id])
    {
#ifdef FH_APP_OVERLAY_TOSD
#ifdef FH_APP_OVERLAY_TOSD_MENU
        osd_menu_exit();
#endif
        ret = sample_unload_tosd(grp_id);
        SDK_FUNC_ERROR(model_name, ret);
#endif

        ret = FHAdv_Osd_UnInit(grp_id);
        SDK_FUNC_ERROR(model_name, ret);

        g_overlay_inited[grp_id] = 0;
    }

    return ret;
}