#ifndef _OSD_WEB_SYS_H_
#define _OSD_WEB_SYS_H_

/*
 * 设备状态接口：
 *
 *   GET /api/sysinfo
 *
 * 返回一行 & 分隔的键值，供看板“设备状态”卡片轮询：
 *   cpu / mem_total / mem_used / mem_max / temp / uptime
 *   ifname / ip / sysver / fwver / build
 *
 * 只读接口，不访问 SD、ISP 或视频通道，放在 HTTP 单线程里调用是安全的。
 */
int osd_web_sys_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_SYS_H_ */
