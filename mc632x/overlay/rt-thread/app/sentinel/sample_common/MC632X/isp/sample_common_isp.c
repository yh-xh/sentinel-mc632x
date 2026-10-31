#include "sample_common_isp.h"

/* 网页那侧排队的 ISP 改动由本线程落地（见 osd_web_isp.c 的 isp_web_pump）。
 * 路径从 app 根算起：app/sentinel 是 -I./ 的根。*/
#ifdef FH_APP_OVERLAY_TOSD_MENU
#include "sample_demo/overlay/osd_menu/osd_web_isp.h"
#endif

static char *model_name = MODEL_TAG_ISP;

static FH_SINT32 isp_model_init(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    ISP_INFO *isp_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++) // 拉低sensor
    {
        isp_info = get_isp_config(grp_id);
        if (isp_info->enable)
        {
            pulldown_sensor(grp_id);
        }
    }

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        isp_info = get_isp_config(grp_id);
        if (isp_info->enable)
        {
            ret = isp_init(grp_id);
            SDK_FUNC_ERROR(model_name, ret);

#ifdef FH_SMART_IR_ENABLE
            ret = sample_fh_smartIR_init(grp_id);
            SDK_FUNC_ERROR(model_name, ret);
#endif
            if (!strcmp(isp_info->sensor_name, "sc3336_mipi")) // sc3336需要翻转
            {
                ret = set_isp_mirror_flip(grp_id, 0, 1);
                SDK_FUNC_ERROR(model_name, ret);
            }
        }
    }

    return ret;
}

static FH_SINT32 isp_model_exit(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    ISP_INFO *isp_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        isp_info = get_isp_config(grp_id);
        if (isp_info->enable)
        {
            if (isp_info->bStop == 0)
            {
                isp_info->bStop = 1;
                while (isp_info->running)
                {
                    usleep(20 * 1000);
                }
#ifdef FH_SMART_IR_ENABLE
                ret = sample_fh_smartIR_exit(grp_id);
                SDK_FUNC_ERROR(model_name, ret);
#endif

                ret = isp_exit(grp_id);
                SDK_FUNC_ERROR(model_name, ret);
            }
        }
    }

    return ret;
}

static FH_VOID *isp_run(FH_VOID *arg)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    ISP_INFO *isp_info = (ISP_INFO *)arg;
    FH_CHAR thread_name[20];

    memset(thread_name, 0, sizeof(thread_name));
    sprintf(thread_name, "demo_isp_%d", isp_info->channel);
    prctl(PR_SET_NAME, thread_name);

    isp_info->running = 1;
    isp_info->pause = 1;
    isp_info->resume = 1;
    while (!isp_info->bStop)
    {
#ifdef FH_APP_OVERLAY_TOSD_MENU
        /* 网页排队的 ISP 改动在这里落地：必须在本线程做，因为全工程
         * ISP 参数路径上没有任何锁，而下面马上要进 API_ISP_Run()。
         * 放在策略之前，改完就能立刻被这一轮策略看到。*/
        isp_web_pump(isp_info->channel);
#endif

        ret = isp_strategy_run(isp_info->channel);
        SDK_FUNC_ERROR_CONTINUE(model_name, ret);

#ifdef FH_SMART_IR_ENABLE
        sample_fh_smartIR_run(isp_info->channel);
#endif

        usleep(10 * 1000);
    }

    isp_info->bStop = 0;
    isp_info->running = 0;

    return NULL;
}

static FH_SINT32 creatISPThread(pthread_t *isp_thread, pthread_attr_t *attr, struct sched_param *param, FH_SINT32 grp_id, ISP_INFO *ispInfo)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    pthread_attr_init(attr);
    pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);
#ifdef __RTTHREAD_OS__
    param->sched_priority = 130;
    pthread_attr_setstacksize(attr, 30 * 1024);
#endif
    pthread_attr_setschedparam(attr, param);
    ret = pthread_create(isp_thread, attr, isp_run, ispInfo);
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 sample_common_isp_get_frame_blk_size(FH_SINT32 grp_id, FH_UINT32 *blk_size)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    *blk_size = 0;

    return ret;
}

FH_SINT32 sample_common_isp_bind_vpu(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    ISP_INFO *isp_info;

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        isp_info = get_isp_config(grp_id);
        if (isp_info->enable)
        {
            ret = isp_bind_vpu(grp_id);
            SDK_FUNC_ERROR(model_name, ret);
        }
    }

    return ret;
}

FH_SINT32 sample_common_isp_init(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    ret = isp_model_init();
    SDK_FUNC_ERROR(model_name, ret);

    ret = sample_common_isp_bind_vpu();
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 sample_common_isp_exit(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;

    ret = isp_model_exit();
    SDK_FUNC_ERROR(model_name, ret);

    return ret;
}

FH_SINT32 sample_common_isp_start(FH_VOID)
{
    FH_SINT32 ret = FH_SDK_SUCCESS;
    ISP_INFO *isp_info;
    pthread_t isp_thread[MAX_GRP_NUM];
    pthread_attr_t attr[MAX_GRP_NUM];
    struct sched_param param[MAX_GRP_NUM];

    for (FH_SINT32 grp_id = 0; grp_id < MAX_GRP_NUM; grp_id++)
    {
        isp_info = get_isp_config(grp_id);
        if (isp_info->enable)
        {
            isp_info->bStop = 0;
            ret = creatISPThread(&isp_thread[grp_id], &attr[grp_id], &param[grp_id], grp_id, isp_info);
            SDK_FUNC_ERROR(model_name, ret);
        }
    }

    return ret;
}
