#include "jpeg_config.h"

static char *model_name = MODEL_TAG_JPEG_CONFIG;

static FH_SINT32 g_mjpeg_chn = MAX_MJPEG_CHN;
static FH_SINT32 g_jpeg_chn = 0;

ENC_INFO g_jpeg_chn_infos[MAX_GRP_NUM][MAX_VPU_CHN_NUM] = {
    {
        {
            .enable = 0,
#if defined(CH0_MJPEG_G0) || defined(UVC_ENABLE)
            .enable = 1,
            .channel = 0,
            .width = CH0_WIDTH_G0,
            .height = CH0_HEIGHT_G0,
#if (CH0_MAX_WIDTH_G0 > CH0_WIDTH_G0)
            .max_width = CH0_MAX_WIDTH_G0,
#else
            .max_width = CH0_WIDTH_G0,
#endif
#if (CH0_MAX_HEIGHT_G0 > CH0_HEIGHT_G0)
            .max_height = CH0_MAX_HEIGHT_G0,
#else
            .max_height = CH0_HEIGHT_G0,
#endif
            .frame_count = CH0_MJPEG_FRAME_COUNT_G0,
            .frame_time = CH0_MJPEG_FRAME_TIME_G0,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH0_MJPEG_RC_TYPE_G0,
            .breath_on = 0,
#endif /*CH0_MJPEG_G0*/
        },
        {
            .enable = 0,
#if defined(CH1_MJPEG_G0)
            .enable = 1,
            .channel = 0,
            .width = CH1_WIDTH_G0,
            .height = CH1_HEIGHT_G0,
#if (CH1_MAX_WIDTH_G0 > CH1_WIDTH_G0)
            .max_width = CH1_MAX_WIDTH_G0,
#else
            .max_width = CH1_WIDTH_G0,
#endif
#if (CH1_MAX_HEIGHT_G0 > CH1_HEIGHT_G0)
            .max_height = CH1_MAX_HEIGHT_G0,
#else
            .max_height = CH1_HEIGHT_G0,
#endif
            .frame_count = CH1_MJPEG_FRAME_COUNT_G0,
            .frame_time = CH1_MJPEG_FRAME_TIME_G0,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH1_MJPEG_RC_TYPE_G0,
            .breath_on = 0,
#endif /*CH1_MJPEG_G0*/
        },
        {
            .enable = 0,
#if defined(CH2_MJPEG_G0)
            .enable = 1,
            .channel = 0,
            .width = CH2_WIDTH_G0,
            .height = CH2_HEIGHT_G0,
#if (CH2_MAX_WIDTH_G0 > CH2_WIDTH_G0)
            .max_width = CH2_MAX_WIDTH_G0,
#else
            .max_width = CH2_WIDTH_G0,
#endif
#if (CH2_MAX_HEIGHT_G0 > CH2_HEIGHT_G0)
            .max_height = CH2_MAX_HEIGHT_G0,
#else
            .max_height = CH2_HEIGHT_G0,
#endif
            .frame_count = CH2_MJPEG_FRAME_COUNT_G0,
            .frame_time = CH2_MJPEG_FRAME_TIME_G0,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH2_MJPEG_RC_TYPE_G0,
            .breath_on = 0,
#endif /*CH2_MJPEG_G0*/
        },
    },
    /*****************************G0*****************************/
    {
        {
            .enable = 0,
#if defined(CH0_MJPEG_G1)
            .enable = 1,
            .channel = 0,
            .width = CH0_WIDTH_G1,
            .height = CH0_HEIGHT_G1,
#if (CH0_MAX_WIDTH_G1 > CH0_WIDTH_G1)
            .max_width = CH0_MAX_WIDTH_G1,
#else
            .max_width = CH0_WIDTH_G1,
#endif
#if (CH0_MAX_HEIGHT_G1 > CH0_HEIGHT_G1)
            .max_height = CH0_MAX_HEIGHT_G1,
#else
            .max_height = CH0_HEIGHT_G1,
#endif
            .frame_count = CH0_MJPEG_FRAME_COUNT_G1,
            .frame_time = CH0_MJPEG_FRAME_TIME_G1,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH0_MJPEG_RC_TYPE_G1,
            .breath_on = 0,
#endif /*CH0_MJPEG_G1*/
        },
        {
            .enable = 0,
#if defined(CH1_MJPEG_G1)
            .enable = 1,
            .channel = 0,
            .width = CH1_WIDTH_G1,
            .height = CH1_HEIGHT_G1,
#if (CH1_MAX_WIDTH_G1 > CH1_WIDTH_G1)
            .max_width = CH1_MAX_WIDTH_G1,
#else
            .max_width = CH1_WIDTH_G1,
#endif
#if (CH1_MAX_HEIGHT_G1 > CH1_HEIGHT_G1)
            .max_height = CH1_MAX_HEIGHT_G1,
#else
            .max_height = CH1_HEIGHT_G1,
#endif
            .frame_count = CH1_MJPEG_FRAME_COUNT_G1,
            .frame_time = CH1_MJPEG_FRAME_TIME_G1,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH1_MJPEG_RC_TYPE_G1,
            .breath_on = 0,
#endif /*CH1_MJPEG_G1*/
        },
        {
            .enable = 0,
#if defined(CH2_MJPEG_G1)
            .enable = 1,
            .channel = 0,
            .width = CH2_WIDTH_G1,
            .height = CH2_HEIGHT_G1,
#if (CH2_MAX_WIDTH_G1 > CH2_WIDTH_G1)
            .max_width = CH2_MAX_WIDTH_G1,
#else
            .max_width = CH2_WIDTH_G1,
#endif
#if (CH2_MAX_HEIGHT_G1 > CH2_HEIGHT_G1)
            .max_height = CH2_MAX_HEIGHT_G1,
#else
            .max_height = CH2_HEIGHT_G1,
#endif
            .frame_count = CH2_MJPEG_FRAME_COUNT_G1,
            .frame_time = CH2_MJPEG_FRAME_TIME_G1,
            .bps = 0 * 1024, // not support
            .enc_type = FH_MJPEG,
            .rc_type = CH2_MJPEG_RC_TYPE_G1,
            .breath_on = 0,
#endif /*CH2_MJPEG_G1*/
        },
    },
    /*****************************G1*****************************/
};

static FH_SINT32 jpeg_set_chn_buffer(FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_CHN_PARAM pstchnparam;

    memset(&pstchnparam, 0, sizeof(pstchnparam));
    ret = FH_VENC_GetChnParam(chn_id, &pstchnparam);
    SDK_FUNC_ERROR(model_name, ret);

#ifdef UVC_ENABLE // 独立缓冲
    pstchnparam.stm_mode = FH_STM_CHN_BUF;
    if (chn_id == JPEG_CHN)
    {
        pstchnparam.stm_size = JPEG_SINGLE_BUFFER;
    }
    else
    {
        pstchnparam.stm_size = MJPEG_SINGLE_BUFFER;
    }
    pstchnparam.max_stm_num = 32;
#else // 公共缓冲
    pstchnparam.stm_mode = FH_STM_PUB_BUF;
#endif

    ret = FH_VENC_SetChnParam(chn_id, &pstchnparam);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

static FH_SINT32 set_mjpeg_rc(FH_SINT32 grp_id, FH_SINT32 chn_id, FH_VENC_CHN_CONFIG *cfg_param)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    switch (g_jpeg_chn_infos[grp_id][chn_id].rc_type)
    {
    case FH_RC_MJPEG_FIXQP:
        cfg_param->rc_attr.rc_type = FH_RC_MJPEG_FIXQP;
        cfg_param->rc_attr.mjpeg_fixqp.qp = 50;
        cfg_param->rc_attr.mjpeg_fixqp.FrameRate.frame_count = g_jpeg_chn_infos[grp_id][chn_id].frame_count;
        cfg_param->rc_attr.mjpeg_fixqp.FrameRate.frame_time = g_jpeg_chn_infos[grp_id][chn_id].frame_time;
        break;
    case FH_RC_MJPEG_CBR:
        cfg_param->rc_attr.rc_type = FH_RC_MJPEG_CBR;
        cfg_param->rc_attr.mjpeg_cbr.initqp = 50;
        cfg_param->rc_attr.mjpeg_cbr.bitrate = g_jpeg_chn_infos[grp_id][chn_id].bps;
        cfg_param->rc_attr.mjpeg_cbr.FrameRate.frame_count = g_jpeg_chn_infos[grp_id][chn_id].frame_count;
        cfg_param->rc_attr.mjpeg_cbr.FrameRate.frame_time = g_jpeg_chn_infos[grp_id][chn_id].frame_time;
        break;
    case FH_RC_MJPEG_VBR:
        cfg_param->rc_attr.rc_type = FH_RC_MJPEG_VBR;
        cfg_param->rc_attr.mjpeg_vbr.initqp = 50;
        cfg_param->rc_attr.mjpeg_vbr.bitrate = g_jpeg_chn_infos[grp_id][chn_id].bps;
        cfg_param->rc_attr.mjpeg_vbr.minqp = 30;
        cfg_param->rc_attr.mjpeg_vbr.maxqp = 80;
        cfg_param->rc_attr.mjpeg_vbr.FrameRate.frame_count = g_jpeg_chn_infos[grp_id][chn_id].frame_count;
        cfg_param->rc_attr.mjpeg_vbr.FrameRate.frame_time = g_jpeg_chn_infos[grp_id][chn_id].frame_time;
        break;
    default:
        ret = -1;
        break;
    }
    SDK_FUNC_ERROR(model_name, ret);

    ret = FH_VENC_SetChnAttr(g_jpeg_chn_infos[grp_id][chn_id].channel, cfg_param);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

static FH_SINT32 set_mjpeg(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_CHN_CONFIG cfg_param;

    cfg_param.chn_attr.enc_type = FH_MJPEG;
    cfg_param.chn_attr.mjpeg_attr.pic_size.u32Width = g_jpeg_chn_infos[grp_id][chn_id].width;
    cfg_param.chn_attr.mjpeg_attr.pic_size.u32Height = g_jpeg_chn_infos[grp_id][chn_id].height;
    cfg_param.chn_attr.mjpeg_attr.rotate = 0;
    cfg_param.chn_attr.mjpeg_attr.encode_speed = 0; /* 0-9 */

    ret = set_mjpeg_rc(grp_id, chn_id, &cfg_param);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

static FH_SINT32 set_jpeg(FH_UINT32 jpeg_chn)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_CHN_CONFIG cfg_param;

    cfg_param.chn_attr.enc_type = FH_JPEG;
    cfg_param.chn_attr.jpeg_attr.qp = 50;
    cfg_param.chn_attr.jpeg_attr.rotate = 0;
    cfg_param.chn_attr.jpeg_attr.encode_speed = 0; /* 大幅面下编码器性能不够时，设置为0 */

    ret = FH_VENC_SetChnAttr(jpeg_chn, &cfg_param);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 get_jpeg_chn(FH_VOID)
{
    return g_jpeg_chn;
}

ENC_INFO *get_mjpeg_config(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    return &g_jpeg_chn_infos[grp_id][chn_id];
}

FH_SINT32 jpeg_create_chn(FH_UINT32 chn_w, FH_UINT32 chn_h)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_CHN_CAP cfg_vencmem;

    cfg_vencmem.support_type = FH_JPEG;
    cfg_vencmem.max_size.u32Width = chn_w;
    cfg_vencmem.max_size.u32Height = chn_h;

#if defined CH0_LPBUF_ENABLE_G0 || defined CH0_LPBUF_ENABLE_G1
    cfg_vencmem.work_mode = FH_HARD_MODE;
    SDK_PRT(model_name, "JPEG workmode = 0x%x\n", cfg_vencmem.work_mode); // TODO
#else
    cfg_vencmem.work_mode = FH_NORMAL_MODE;
#endif

    if (g_jpeg_chn == 0) // 未被初始化过
    {
        ret = jpeg_set_chn_buffer(JPEG_CHN);
        SDK_FUNC_ERROR(model_name, ret);

        ret = FH_VENC_CreateChn(JPEG_CHN, &cfg_vencmem);
        SDK_FUNC_ERROR(model_name, ret);
        g_jpeg_chn = JPEG_CHN;

        ret = set_jpeg(g_jpeg_chn);
        SDK_FUNC_ERROR(model_name, ret);
    }

    return ret;
}

FH_SINT32 jpeg_stop(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    if (g_jpeg_chn)
    {
        ret = FH_VENC_StopRecvPic(g_jpeg_chn);
        SDK_FUNC_ERROR(model_name, ret);
    }

    return ret;
}

FH_SINT32 jpeg_destroy(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    ret = jpeg_stop();
    SDK_FUNC_ERROR(model_name, ret);

    if (g_jpeg_chn)
    {
        ret = FH_VENC_DestroyChn(g_jpeg_chn);
        SDK_FUNC_ERROR(model_name, ret);

        g_jpeg_chn = 0;
    }

    return ret;
}

FH_SINT32 mjpeg_create_chn(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_CHN_CAP cfg_vencmem;
    FH_SINT32 mjpeg_chn = 0;

    cfg_vencmem.support_type = FH_MJPEG;
    cfg_vencmem.max_size.u32Width = g_jpeg_chn_infos[grp_id][chn_id].max_width;
    cfg_vencmem.max_size.u32Height = g_jpeg_chn_infos[grp_id][chn_id].max_height;
    mjpeg_chn = g_jpeg_chn_infos[grp_id][chn_id].channel;

#if defined CH0_LPBUF_ENABLE_G0 || defined CH0_LPBUF_ENABLE_G1
    /* lpbuf 只存在于 chn0；chn1/chn2 必须走普通模式，
     * 否则 VPU 通道（非 lpbuf）不会向 MJPEG 编码器送帧。
     * 和 enc_config.c 里 H.264 的判断保持一致。*/
    if (chn_id == 0)
        cfg_vencmem.work_mode = FH_HARD_MODE;
    else
        cfg_vencmem.work_mode = FH_NORMAL_MODE;
    SDK_PRT(model_name, "MJPEG workmode = 0x%x (grp%d chn%d)\n",
            cfg_vencmem.work_mode, grp_id, chn_id);
#else
    cfg_vencmem.work_mode = FH_NORMAL_MODE;
#endif

    if (mjpeg_chn == 0) // 未被初始化过
    {
        ret = jpeg_set_chn_buffer(g_mjpeg_chn);
        SDK_FUNC_ERROR(model_name, ret);

        ret = FH_VENC_CreateChn(g_mjpeg_chn, &cfg_vencmem);
        SDK_FUNC_ERROR(model_name, ret);
        g_jpeg_chn_infos[grp_id][chn_id].channel = g_mjpeg_chn;
        g_mjpeg_chn--;
    }
    else
    {
        ret = FH_VENC_CreateChn(mjpeg_chn, &cfg_vencmem);
        SDK_FUNC_ERROR(model_name, ret);
    }

    return ret;
}

FH_SINT32 mjpeg_set_chn_attr(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    ret = set_mjpeg(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 mjpeg_start(FH_SINT32 grp_id, FH_SINT32 chn_id)
{

    FH_SINT32 ret = FH_SDK_SUCCESS;

    ret = FH_VENC_StartRecvPic(g_jpeg_chn_infos[grp_id][chn_id].channel);
    SDK_FUNC_ERROR(model_name, ret);

    g_jpeg_chn_infos[grp_id][chn_id].run_status = 1;

    return ret;
}

FH_SINT32 mjpeg_stop(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_SINT32 mjpeg_chn = 0;

    mjpeg_chn = g_jpeg_chn_infos[grp_id][chn_id].channel;

    if (mjpeg_chn)
    {
        if (g_jpeg_chn_infos[grp_id][chn_id].run_status)
        {
            ret = FH_VENC_StopRecvPic(mjpeg_chn);
            SDK_FUNC_ERROR(model_name, ret);
        }
    }

    return ret;
}

FH_SINT32 mjpeg_destroy(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_SINT32 mjpeg_chn = 0;

    mjpeg_chn = g_jpeg_chn_infos[grp_id][chn_id].channel;

    if (mjpeg_chn)
    {
        if (g_jpeg_chn_infos[grp_id][chn_id].run_status)
        {
            ret = FH_VENC_DestroyChn(mjpeg_chn);
            SDK_FUNC_ERROR(model_name, ret);

            g_jpeg_chn_infos[grp_id][chn_id].run_status = 0;
        }
    }

    return ret;
}

FH_SINT32 mjpeg_jpeg_exit(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        for (FH_SINT32 chn_id = 0; chn_id < MAX_VPU_CHN_NUM; chn_id++)
        {
            ret = mjpeg_stop(grp_id, chn_id);
            SDK_FUNC_ERROR(model_name, ret);

            ret = mjpeg_destroy(grp_id, chn_id);
            SDK_FUNC_ERROR(model_name, ret);
        }
    }

    ret = jpeg_stop();
    SDK_FUNC_ERROR(model_name, ret);

    ret = jpeg_destroy();
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 getJpegStream(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_VENC_STREAM stream;
    FH_SINT32 jpegDataLength = 0;

#ifdef UVC_ENABLE
    FH_SINT32 jpeg_chn = JPEG_CHN;
    ret = enc_get_chn_stream(jpeg_chn, &stream);
#else
    ret = enc_get_common_stream(FH_STREAM_JPEG, &stream);
#endif
    SDK_FUNC_ERROR(model_name, ret);

    jpegDataLength = stream.jpeg_stream.length;
    SDK_PRT(model_name, "Capture JPEG OK, size=%d\n\n", jpegDataLength);

    ret = enc_release_stream(&stream);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 mjpeg_chn_relase(FH_SINT32 grp_id, FH_SINT32 chn_id)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    if (!g_jpeg_chn_infos[grp_id][chn_id].enable)
    {
        SDK_ERR_PRT(model_name, "mjpeg_chn_relase Failed, VPU[%d] CHN[%d] MJPEG not enable!\n", grp_id, chn_id);
        return FH_SDK_FAILED;
    }

    ret = mjpeg_stop(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    ret = mjpeg_destroy(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 changeMjpegAttr(FH_SINT32 grp_id, FH_SINT32 chn_id, FH_SINT32 width, FH_SINT32 height)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    if (!g_jpeg_chn_infos[grp_id][chn_id].enable)
    {
        SDK_ERR_PRT(model_name, "changeMjpegAttr Failed, VPU[%d] CHN[%d] MJPEG not enable!\n", grp_id, chn_id);
        return FH_SDK_FAILED;
    }

    g_jpeg_chn_infos[grp_id][chn_id].max_width = width;
    g_jpeg_chn_infos[grp_id][chn_id].max_height = height;

    g_jpeg_chn_infos[grp_id][chn_id].width = width;
    g_jpeg_chn_infos[grp_id][chn_id].height = height;

    ret = mjpeg_create_chn(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    ret = mjpeg_set_chn_attr(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    ret = mjpeg_start(grp_id, chn_id);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}
