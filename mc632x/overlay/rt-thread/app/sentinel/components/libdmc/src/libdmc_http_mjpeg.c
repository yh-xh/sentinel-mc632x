#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libdmc/include/libdmc.h"
#include "libdmc/include/libdmc_http_mjpeg.h"
#include <libmjpeg_over_http/include/libhttp_mjpeg.h>
#include "jpeg_config.h"

static MJPEG_HANDLE g_mjpeg_server[MAX_GRP_NUM * MAX_VPU_CHN_NUM];
static unsigned int g_mjpeg_port[MAX_GRP_NUM * MAX_VPU_CHN_NUM];
static unsigned int g_printed[MAX_GRP_NUM * MAX_VPU_CHN_NUM];
static dmc_mjpeg_frame_obs_fn g_frame_obs = NULL;

void dmc_http_mjpeg_set_frame_observer(dmc_mjpeg_frame_obs_fn fn)
{
    g_frame_obs = fn;
}

static int _http_mjpeg_input_fn(int media_chn,
                                int media_type,
                                int media_subtype,
                                unsigned long long frame_pts,
                                unsigned char *frame_data,
                                int frame_len,
                                int frame_end_flag)
{
    int idx = MAX_MJPEG_CHN - media_chn;

    if (idx < 0 || idx >= MAX_GRP_NUM * MAX_VPU_CHN_NUM)
    {
        return 0;
    }

    /* 先交给观察者复制一份（抓拍/事件用），再走网络发送。
     * 这样即使客户端很慢甚至没连，缓存依然是最新的。*/
    if (g_frame_obs)
    {
        g_frame_obs(idx, frame_data, frame_len);
    }

    libmjpeg_send_stream(&g_mjpeg_server[idx], frame_data, frame_len);

    if (!g_printed[idx])
    {
        printf("[MJPEG MAP] media_chn=%d idx=%d port=%d\n",
               media_chn, idx, g_mjpeg_port[idx]);
        g_printed[idx] = 1;
    }

    return 0;
}

FH_SINT32 dmc_http_mjpeg_subscribe(FH_UINT16 port)
{
    int ret = 0;
    ENC_INFO *mjpeg_info = NULL;
    int mjpeg_max_chn = MAX_MJPEG_CHN;
    int mjpeg_grp_code = 0;
    int index = 0;

    for (int grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        for (int chn_id = 0; chn_id < MAX_VPU_CHN_NUM; chn_id++)
        {
            mjpeg_info = get_mjpeg_config(grp_id, chn_id);
            if (mjpeg_info->enable)
            {
                for (; index < MAX_GRP_NUM * MAX_VPU_CHN_NUM; index++)
                {
                    if (g_mjpeg_server[index] == NULL)
                    {
                        g_mjpeg_port[index] = port + index;
                        ret = libmjpeg_start_server(&g_mjpeg_server[index], index, g_mjpeg_port[index]);
                        if (ret)
                        {
                            printf("libmjpeg_start_server failed, ret = %d!\n", ret);
                            return ret;
                        }
                        g_printed[index] = 0;
                        printf("[MJPEG SERVER]Start MJPEG Server[%d] GRP[%d] CHN[%d] MJPEG CHN[%d]!\n", index, grp_id, chn_id, mjpeg_max_chn);
                        mjpeg_grp_code |= 1 << (mjpeg_max_chn / MAX_VPU_CHN_NUM);
                        mjpeg_max_chn--;
                        break;
                    }
                }
            }
        }
    }
    ret = dmc_subscribe("HTTP_MJPEG", mjpeg_grp_code, DMC_MEDIA_TYPE_MJPEG, _http_mjpeg_input_fn);
    if (ret)
    {
        printf("dmc_subscribe failed, ret = %d!\n", ret);
    }

    return ret;
}

FH_SINT32 dmc_http_mjpeg_unsubscribe(FH_VOID)
{
    dmc_unsubscribe("HTTP_MJPEG", DMC_MEDIA_TYPE_MJPEG);

    for (int index = 0; index < MAX_GRP_NUM * MAX_VPU_CHN_NUM; index++)
    {
        if (g_mjpeg_server[index] != NULL)
        {
            libmjpeg_stop_server(&g_mjpeg_server[index]);
            g_mjpeg_server[index] = NULL;
            g_printed[index] = 0;
            g_mjpeg_port[index] = 0;
        }
    }

    return 0;
}
