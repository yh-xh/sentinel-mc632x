/*
 * osd_web_sys.c —— /api/sysinfo 设备状态
 *
 * CPU 占用用内核 scheduler.c 里的 rt_cpuload_get()：它返回“上次调用以来”
 * 的 idle / total 时间片，所以网页轮询间隔就是统计窗口。第一次调用拿到的是
 * 开机以来的累计值，直接丢弃。
 *
 * 温度来自 RTC 内部温度传感器（rtc_get_temp），不需要额外 I2C 器件。
 * 全部是只读操作，不碰 SD / ISP / 视频通道。
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtthread.h>

#include "config.h"
#include "osd_web_key.h"
#include "osd_web_sys.h"

/* scheduler.c / drivers/rtc 提供，但没有在 app 可见的头文件里声明 */
extern void rt_cpuload_get(rt_uint32_t *idle, rt_uint32_t *total);
extern int  rtc_get_temp(void);
/* lwIP 的 ioctl，compat 头文件里没有声明；SIOCGIFADDR 由 eth_ioctl 转发 */
extern int  lwip_ioctl(int s, long cmd, void *argp);

#define SYS_FW_NAME "SENTINEL 1.0"

static int      s_cpu_ready = 0;
static unsigned s_cpu_last  = 0;

static unsigned cpu_usage(void)
{
    rt_uint32_t idle  = 0;
    rt_uint32_t total = 0;
    rt_uint64_t used;

    rt_cpuload_get(&idle, &total);

    /* 第一次是开机以来的累计值，没有参考意义 */
    if (!s_cpu_ready)
    {
        s_cpu_ready = 1;
        return 0;
    }

    /* 两次采样太近（小于约 20ms）时沿用上次结果，避免除出噪声 */
    if (total < 20000)
        return s_cpu_last;

    if (idle > total)
        idle = total;

    used = (rt_uint64_t)(total - idle) * 100ULL;
    s_cpu_last = (unsigned)(used / total);
    if (s_cpu_last > 100)
        s_cpu_last = 100;

    return s_cpu_last;
}

static int net_ip(char *ip, size_t ip_cap, char *name, size_t name_cap)
{
    struct ifreq ifr;
    int          fd;
    int          ok = 0;

    ip[0]   = '\0';
    name[0] = '\0';

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "e0", IFNAMSIZ - 1);

    if (lwip_ioctl(fd, SIOCGIFADDR, &ifr) == 0)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;

        strncpy(ip, inet_ntoa(sin->sin_addr), ip_cap - 1);
        ip[ip_cap - 1] = '\0';

        if (name_cap > 1)
        {
            strncpy(name, "e0", name_cap - 1);
            name[name_cap - 1] = '\0';
        }
        ok = 1;
    }

    close(fd);
    return ok;
}

static int sysinfo_api(int fd)
{
    rt_uint32_t mem_total = 0;
    rt_uint32_t mem_used  = 0;
    rt_uint32_t mem_max   = 0;
    char        ip[24]    = "";
    char        ifname[8] = "";
    char        body[384];
    int         temp;
    unsigned    up;

    rt_memory_info(&mem_total, &mem_used, &mem_max);

    temp = rtc_get_temp();
    if (temp < -40 || temp > 150)
        temp = -1;

    up = (unsigned)(rt_tick_get() / RT_TICK_PER_SECOND);
    net_ip(ip, sizeof(ip), ifname, sizeof(ifname));

    snprintf(body, sizeof(body),
             "cpu=%u&mem_total=%u&mem_used=%u&mem_max=%u&temp=%d"
             "&uptime=%u&ifname=%s&ip=%s"
             "&sysver=RT-Thread %d.%d.%d&fwver=%s&build=%s %s",
             cpu_usage(), (unsigned)mem_total, (unsigned)mem_used,
             (unsigned)mem_max, temp,
             up, ifname, ip,
             (int)RT_VERSION, (int)RT_SUBVERSION, (int)RT_REVISION,
             SYS_FW_NAME, __DATE__, __TIME__);

    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
    return 1;
}

int osd_web_sys_handle(int fd, const char *path, const char *query)
{
    (void)query;

    if (!strcmp(path, "/api/sysinfo"))
        return sysinfo_api(fd);

    return 0;
}
