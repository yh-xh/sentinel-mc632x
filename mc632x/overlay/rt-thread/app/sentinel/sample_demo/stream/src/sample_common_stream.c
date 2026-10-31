#include "stream/include/sample_common_stream.h"
#include "stream/include/mjpeg_cache.h"
#include "stream/include/snap_store.h"
#include "stream/include/rec_store.h"
#include "stream/include/clip_store.h"

static char *model_name = MODEL_TAG_STREAM;

static STREAM_INFO g_stream_info[MAX_GRP_NUM];
static FH_VOID _get_stream_info_type()
{
    memset(g_stream_info, 0, sizeof(g_stream_info));

    for (FH_SINT32 i = 0; i < MAX_GRP_NUM; i++)
    {
        g_stream_info[i].grpid = i;
        switch (i)
        {
        case 0:
#ifdef FH_APP_USING_PES_G0
            g_stream_info[0].type |= FH_PES;
#endif
#ifdef FH_APP_USING_RTSP_G0
            g_stream_info[0].type |= FH_RTSP;
#endif
#ifdef FH_APP_RECORD_RAW_STREAM_G0
            g_stream_info[0].type |= FH_RAW;
#endif
            break;
        case 1:
#ifdef FH_APP_USING_PES_G1
            g_stream_info[1].type |= FH_PES;
#endif
#ifdef FH_APP_USING_RTSP_G1
            g_stream_info[1].type |= FH_RTSP;
#endif
#ifdef FH_APP_RECORD_RAW_STREAM_G1
            g_stream_info[1].type |= FH_RAW;
#endif
            break;
        case 2:
#ifdef FH_APP_USING_PES_G2
            g_stream_info[2].type |= FH_PES;
#endif
#ifdef FH_APP_USING_RTSP_G2
            g_stream_info[2].type |= FH_RTSP;
#endif
#ifdef FH_APP_RECORD_RAW_STREAM_G2
            g_stream_info[2].type |= FH_RAW;
#endif
            break;
        case 3:
#ifdef FH_APP_USING_PES_G3
            g_stream_info[3].type |= FH_PES;
#endif
#ifdef FH_APP_USING_RTSP_G3
            g_stream_info[3].type |= FH_RTSP;
#endif
#ifdef FH_APP_RECORD_RAW_STREAM_G3
            g_stream_info[3].type |= FH_RAW;
#endif
            break;
        default:
            SDK_ERR_PRT(model_name, "Not support!\n");
            break;
        }
    }
}

static void stream_frame_observer(int server_index,
                                  const unsigned char *data, int len)
{
    mjpeg_cache_on_frame(server_index, data, len);
    clip_store_on_frame(server_index, data, len);
    rec_store_on_frame(server_index, data, len);
}

FH_SINT32 sample_common_dmc_init(FH_CHAR *dst_ip, FH_UINT32 port)
{
    FH_SINT32 index;
    FH_SINT32 pesgrp;
    FH_SINT32 rtspgrp;
    FH_SINT32 rawgrp;

    _get_stream_info_type();

    /* 抓拍/事件基础设施：必须在 MJPEG 订阅之前就绪 */
    mjpeg_cache_init();
    clip_store_init();
    snap_store_init();
    rec_store_init();

    dmc_init();

    index = 0;
    pesgrp = 0;
    rtspgrp = 0;
    rawgrp = 0;
    while (index < MAX_GRP_NUM)
    {
        if (g_stream_info[index].type & FH_PES)
            pesgrp |= 1 << g_stream_info[index].grpid;
        if (g_stream_info[index].type & FH_RTSP)
            rtspgrp |= 1 << g_stream_info[index].grpid;
        if (g_stream_info[index].type & FH_RAW)
            rawgrp |= 1 << g_stream_info[index].grpid;

        index++;
    }

#ifdef FH_PES_ENABLE
    if (dst_ip != NULL && port != 0 && pesgrp)
        dmc_pes_subscribe(pesgrp, dst_ip, port);
#endif

#ifdef FH_RTSP_ENABLE
    if (port != 0 && rtspgrp)
        dmc_rtsp_subscribe(rtspgrp, port);
#endif

#ifdef FH_RECORD_ENABLE
    if (rawgrp)
        dmc_record_subscribe(rawgrp);
#endif

#ifdef FH_MJPEG_ENABLE
    dmc_http_mjpeg_set_frame_observer(stream_frame_observer);
    dmc_http_mjpeg_subscribe(HTTP_MJPEG_PORT);
#endif

    return 0;
}

FH_SINT32 sample_common_dmc_deinit(FH_VOID)
{
#ifdef FH_PES_ENABLE
    dmc_pes_unsubscribe();
#endif

#ifdef FH_RTSP_ENABLE
    dmc_rtsp_unsubscribe();
#endif

#ifdef FH_RECORD_ENABLE
    dmc_record_unsubscribe();
#endif

#ifdef FH_MJPEG_ENABLE
    dmc_http_mjpeg_set_frame_observer(NULL);
    dmc_http_mjpeg_unsubscribe();
#endif

    /* 先停事件线程和录像线程，再释放帧缓存 */
    rec_store_deinit();
    snap_store_deinit();
    clip_store_deinit();
    mjpeg_cache_deinit();

    dmc_deinit();

    return 0;
}

static FH_SINT32 g_get_stream_stop = 0;
static FH_SINT32 g_get_stream_init = 0;
FH_VOID *get_stream(FH_VOID *arg)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_BIND_INFO *vpu_bind_info = NULL;
    ENC_INFO *mjpeg_info = NULL;
    FH_SINT32 isBind = 0;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        for (FH_SINT32 chn_id = 0; chn_id < MAX_VPU_CHN_NUM; chn_id++)
        {
            vpu_bind_info = get_vpu_bind_info(grp_id, chn_id);
            mjpeg_info = get_mjpeg_config(grp_id, chn_id);
            if (vpu_bind_info->toEnc || mjpeg_info->enable)
            {
                isBind = 1;
            }
        }
    }

    g_get_stream_stop = 0;
    if (isBind)
    {
        g_get_stream_init = 1;
    }
    else
    {
        g_get_stream_init = 0;
        SDK_PRT(model_name, "No Channel Bind, get_stream thread exit!\n");
        return NULL;
    }

    prctl(PR_SET_NAME, "demo_get_stream");

    while (!g_get_stream_stop)
    {
        ret = getAllEncStreamToDmc();
        SDK_FUNC_ERROR_CONTINUE(model_name, ret);
        usleep(10);
    }

    g_get_stream_stop = 0;
    return NULL;
}

FH_SINT32 sample_common_start_get_stream(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
#ifdef UVC_ENABLE
    return ret;
#endif
    pthread_attr_t attr;
    pthread_t thread_stream;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED); /* 子线程默认继承父线程调度策略，此处配为分离式，即不继承父线程 */
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
#ifdef __RTTHREAD_OS__
    param.sched_priority = 130;
    pthread_attr_setstacksize(&attr, 8 * 1024);
#else
    param.sched_priority = 55;
#endif
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&thread_stream, &attr, get_stream, NULL);

    pthread_attr_destroy(&attr);

    return ret;
}

FH_SINT32 sample_common_stop_get_stream(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
#ifdef UVC_ENABLE
    return ret;
#endif
    /*让获取码流的线程退出,置退出标记*/
    g_get_stream_stop = 1;

    /*等待获取码流的线程退出*/
    while (g_get_stream_stop && g_get_stream_init)
    {
        usleep(10 * 1000);
    }

    g_get_stream_init = 0;

    return ret;
}

FH_VOID *send_stream(FH_VOID *arg)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    FH_SINT32 loop = 0;
    FILE *y_fd = NULL;
    FILE *uv_fd = NULL;
    FILE *yuv_fd = NULL;
    MEM_DESC y_data[YUV_BUF_NUM];
    MEM_DESC uv_data[YUV_BUF_NUM];
    MEM_DESC yuv_data[YUV_BUF_NUM];
    FH_PHYADDR ydata;
    FH_PHYADDR uvdata;
    FH_CHAR buffer_name[20];
    FH_UINT32 buf_id = 0;
    VPU_MEM_IN *vpu_mem_in = NULL;
    FH_UINT64 y_size = 0;
    FH_UINT64 uv_size = 0;

    vpu_mem_in = (VPU_MEM_IN *)arg;
    SDK_CHECK_NULL_PTR(model_name, vpu_mem_in);

    ret = getYuvSizeFromeYuvType(vpu_mem_in, &y_size, &uv_size);
    SDK_FUNC_ERROR_GOTO(model_name, ret);

    if (vpu_mem_in->SendFrameInfo.yfile && vpu_mem_in->SendFrameInfo.uvfile)
    {
        ret = openFile(&y_fd, vpu_mem_in->SendFrameInfo.yfile);
        SDK_FUNC_ERROR_GOTO(model_name, ret);

        ret = openFile(&uv_fd, vpu_mem_in->SendFrameInfo.uvfile);
        SDK_FUNC_ERROR_GOTO(model_name, ret);
    }
    else if (vpu_mem_in->SendFrameInfo.yuvfile)
    {
        ret = openFile(&yuv_fd, vpu_mem_in->SendFrameInfo.yuvfile);
        SDK_FUNC_ERROR_GOTO(model_name, ret);
    }
    else
    {
        SDK_PRT(model_name, "Didn't set yuv filename!\n");
        goto Exit;
    }

    for (buf_id = 0; buf_id < YUV_BUF_NUM; buf_id++)
    {
        memset(buffer_name, 0, sizeof(buffer_name));
        if (yuv_fd)
        {
            sprintf(buffer_name, "UsrPic_YUVData_%d", buf_id);
            ret = buffer_malloc_withname(&yuv_data[buf_id], y_size + uv_size, 16, buffer_name);
            SDK_FUNC_ERROR_GOTO(model_name, ret);
        }
        else
        {
            sprintf(buffer_name, "UsrPic_YData_%d", buf_id);
            ret = buffer_malloc_withname(&y_data[buf_id], y_size, 16, buffer_name);
            SDK_FUNC_ERROR_GOTO(model_name, ret);

            memset(buffer_name, 0, sizeof(buffer_name));
            sprintf(buffer_name, "UsrPic_UVData_%d", buf_id);
            ret = buffer_malloc_withname(&uv_data[buf_id], uv_size, 16, buffer_name);
            SDK_FUNC_ERROR_GOTO(model_name, ret);
        }
    }

    prctl(PR_SET_NAME, "demo_vpu_send");
    buf_id = 0;

    while (!vpu_mem_in->bStop)
    {
        if (yuv_fd)
        {
            ret = readFileRepeatByLength(yuv_fd, y_size + uv_size, yuv_data[buf_id].vbase);
            SDK_FUNC_ERROR_CONTINUE(model_name, ret);
            ydata = yuv_data[buf_id].base;
            uvdata = yuv_data[buf_id].base + y_size;
        }
        else
        {
            ret = readFileRepeatByLength(y_fd, y_size, y_data[buf_id].vbase);
            SDK_FUNC_ERROR_CONTINUE(model_name, ret);
            ret = readFileRepeatByLength(uv_fd, uv_size, uv_data[buf_id].vbase);
            SDK_FUNC_ERROR_CONTINUE(model_name, ret);
            ydata = y_data[buf_id].base;
            uvdata = uv_data[buf_id].base;
        }

        vpu_mem_in->time_stamp = vpu_mem_in->time_stamp * loop;
        vpu_mem_in->frame_id = loop;
        ret = sendStreamToVpu(vpu_mem_in, ydata, uvdata);
        SDK_FUNC_ERROR_CONTINUE(model_name, ret);
        buf_id++;
        buf_id = (buf_id == YUV_BUF_NUM) ? 0 : (buf_id);
        loop++;
    }

Exit:
    if (y_fd)
    {
        fclose(y_fd);
    }

    if (uv_fd)
    {
        fclose(uv_fd);
    }

    if (yuv_fd)
    {
        fclose(yuv_fd);
    }

    vpu_mem_in->bStart = 0;
    vpu_mem_in->bStop = 0;

    return NULL;
}

FH_SINT32 sample_common_start_send_stream(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_MEM_IN *vpuMemInInfo;
    pthread_attr_t attr;
    pthread_t thread_stream;
    struct sched_param param;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpuMemInInfo = get_vpu_mem_in_config(grp_id);
        if (vpuMemInInfo->enable)
        {
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#ifdef __RTTHREAD_OS__
            param.sched_priority = 130;
#endif
            pthread_attr_setschedparam(&attr, &param);
            pthread_create(&thread_stream, &attr, send_stream, vpuMemInInfo);
            vpuMemInInfo->bStart = 1;
            pthread_attr_destroy(&attr);
        }
    }

    return ret;
}

FH_SINT32 sample_common_stop_send_stream(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    VPU_MEM_IN *vpuMemInInfo;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpuMemInInfo = get_vpu_mem_in_config(grp_id);
        if (vpuMemInInfo->enable && vpuMemInInfo->bStart)
        {
            vpuMemInInfo->bStop = 1;
        }
    }

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        vpuMemInInfo = get_vpu_mem_in_config(grp_id);
        if (vpuMemInInfo->enable && vpuMemInInfo->bStop)
        {
            usleep(10 * 1000);
        }
    }

    return ret;
}
