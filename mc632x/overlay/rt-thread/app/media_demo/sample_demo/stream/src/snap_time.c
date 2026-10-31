/*
 * snap_time.c —— 网页同步时间 + 本地时间格式化
 *
 * 不调用 settimeofday/stime：这块板子没有 RTC，改系统时间会影响
 * SDK 和 lwIP 对时间的假设。只保存一个偏移，拍照命名和事件列表
 * 用这个偏移算时间。
 */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <rtthread.h>

#include "stream/include/snap_time.h"
#include "libsample_common/include/libsample_common.h"

static pthread_mutex_t g_time_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long g_base_ms;      /* 同步那一刻的 get_ms() */
static unsigned long long g_base_epoch;   /* 同步那一刻的本地 epoch 秒 */
static int g_time_valid;

void snap_time_set(unsigned utc_epoch, int tz_min)
{
    struct timeval tv;

    if (tz_min < -1440)
    {
        tz_min = -1440;
    }
    if (tz_min > 1440)
    {
        tz_min = 1440;
    }

    pthread_mutex_lock(&g_time_lock);
    g_base_epoch = (unsigned long long)utc_epoch + (long long)tz_min * 60;
    g_base_ms    = get_ms();
    g_time_valid = (utc_epoch != 0);
    pthread_mutex_unlock(&g_time_lock);

    /* 同时校准系统时间，让 LCD 左上角的 OSD 时钟也走对。
     * OSD 用 localtime()/RTC，和上面的拍照时间偏移是两套东西。*/
    if (utc_epoch != 0)
    {
        rt_device_t  rtc;
        unsigned int sec = utc_epoch;

        tv.tv_sec  = (time_t)utc_epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        /* 有些 OSD/驱动直接读 RTC 设备，不读 timekeeping，这里一并写。*/
        rtc = rt_device_find("rtc");
        if (rtc != RT_NULL)
        {
            rt_device_control(rtc, RT_DEVICE_CTRL_RTC_SET_TIME, &sec);
        }
    }
}

int snap_time_get(unsigned *local_epoch)
{
    int valid;
    unsigned long long now;

    pthread_mutex_lock(&g_time_lock);
    valid = g_time_valid;
    if (valid)
    {
        now = g_base_epoch + (get_ms() - g_base_ms) / 1000;
        if (local_epoch != NULL)
        {
            *local_epoch = (unsigned)now;
        }
    }
    pthread_mutex_unlock(&g_time_lock);

    return valid;
}

static int time_parts(unsigned epoch, struct tm *tm)
{
    time_t t = (time_t)epoch;

    return (epoch != 0 && gmtime_r(&t, tm) != NULL);
}

int snap_time_name_at(unsigned epoch, char *date, int dcap, char *clk, int ccap)
{
    struct tm tm;

    if (date != NULL && dcap > 0)
    {
        date[0] = '\0';
    }
    if (clk != NULL && ccap > 0)
    {
        clk[0] = '\0';
    }

    if (!time_parts(epoch, &tm))
    {
        return 0;
    }

    if (date != NULL && dcap > 0)
    {
        snprintf(date, dcap, "%04d%02d%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
    if (clk != NULL && ccap > 0)
    {
        snprintf(clk, ccap, "%02d%02d%02d",
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    return 1;
}

int snap_time_name(char *date, int dcap, char *clk, int ccap)
{
    unsigned epoch;

    if (!snap_time_get(&epoch))
    {
        if (date != NULL && dcap > 0)
        {
            date[0] = '\0';
        }
        if (clk != NULL && ccap > 0)
        {
            clk[0] = '\0';
        }
        return 0;
    }

    return snap_time_name_at(epoch, date, dcap, clk, ccap);
}

void snap_time_text(unsigned epoch, char *buf, int cap)
{
    struct tm tm;

    if (buf == NULL || cap <= 0)
    {
        return;
    }

    if (!time_parts(epoch, &tm))
    {
        snprintf(buf, cap, "--");
        return;
    }

    snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}
