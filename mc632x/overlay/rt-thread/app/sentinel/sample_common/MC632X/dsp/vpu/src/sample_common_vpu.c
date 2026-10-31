#include "sample_common_vpu.h"

static char *model_name = MODEL_TAG_VPU;

FH_SINT32 sample_common_vpu_get_frame_blk_size(FH_SINT32 grp_id, FH_SINT32 chn_id, FH_UINT32 *blk_size)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_CHN_INFO *vpu_chn_info;

    vpu_chn_info = get_vpu_chn_config(grp_id, chn_id);

    *blk_size = 0;
    if (vpu_chn_info->enable && !vpu_chn_info->bufnum)
    {
        ret = get_vpu_chn_blk_size(grp_id, chn_id, blk_size);
        SDK_FUNC_ERROR(model_name, ret);

#if defined CH0_LPBUF_ENABLE_G0 || defined CH0_LPBUF_ENABLE_G1
        if (chn_id == 0) // 开启lpbuf的情况下，chn0不需要额外的vb和vmm
        {
            *blk_size = 0;
        }
        if (grp_id == 1 && chn_id == 1) // 最小buffer跑电性能test，子码流3个buffer公用vb
        {
            *blk_size = 0;
        }
#endif
    }
    else
    {
        *blk_size = 0;
    }

    return ret;
}

FH_SINT32 sample_common_vpu_init(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_INFO *vpu_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpu_info = get_vpu_config(grp_id);
        if (vpu_info->enable)
        {
            ret = vpu_init(grp_id);
            SDK_FUNC_ERROR(model_name, ret);
        }
    }

    return ret;
}

FH_SINT32 sample_common_vpu_chn_init(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_INFO *vpu_info;
    VPU_CHN_INFO *vpu_chn_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpu_info = get_vpu_config(grp_id);
        if (vpu_info->enable)
        {
            for (FH_SINT32 chn_id = 0; chn_id < MAX_VPU_CHN_NUM; chn_id++)
            {
                vpu_chn_info = get_vpu_chn_config(grp_id, chn_id);
                if (vpu_chn_info->enable)
                {
                    SDK_PRT(model_name, "Vpu[%d] CHN[%d] start init!\n", grp_id, chn_id);
                    ret = vpu_chn_init(grp_id, chn_id);
                    SDK_FUNC_ERROR(model_name, ret);
                }
            }
        }
    }

    return ret;
}

FH_SINT32 sample_common_vpu_bind(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_INFO *vpu_info;
    VPU_CHN_INFO *vpu_chn_info;
    VPU_BIND_INFO *vpu_bind_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpu_info = get_vpu_config(grp_id);
        if (vpu_info->enable)
        {
            for (FH_SINT32 chn_id = 0; chn_id < MAX_VPU_CHN_NUM; chn_id++)
            {
                vpu_chn_info = get_vpu_chn_config(grp_id, chn_id);
                if (vpu_chn_info->enable)
                {
                    vpu_bind_info = get_vpu_bind_info(grp_id, chn_id);
                    if (vpu_bind_info->toEnc)
                    {
                        ret = vpu_bind_enc(grp_id, chn_id);
                        SDK_FUNC_ERROR(model_name, ret);
                    }

                    if (vpu_bind_info->toVou)
                    {
                        ret = vpu_bind_vou(grp_id, chn_id);
                        SDK_FUNC_ERROR(model_name, ret);
                    }
                }
            }
        }
    }

    return ret;
}
