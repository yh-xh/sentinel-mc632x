#ifndef _OSD_WEB_APP_H_
#define _OSD_WEB_APP_H_

/*
 * 抓拍 / 叠加 / 事件接口，挂在 8080 上：
 *   GET /api/snap?cam=0|1[&save=1][&ret=1]
 *   GET /api/overlay[?on=0|1]
 *   GET /api/events[?n=5]
 *   GET /api/event.jpg[?id=N]
 *   GET /api/files[?n=20]       列 /mnt/snap 下全部 JPEG（重启后仍可浏览）
 *   GET /api/file?name=xxx.jpg  按文件名读取原图
 *   GET /api/delete?name=xxx.jpg 删除一张照片（仅限 /mnt/snap 下的 jpg）
 *   GET /api/auto[?on=0|1]      绊线/周界自动抓拍开关（默认关闭）
 *   GET /api/arm[?on=0|1]       布防/撤防（布防=自动存图+允许小智播报）
 *   GET /api/status             armed/auto/overlay/sd/people/model/count
 *   GET /api/time[?t=<utc>&tz=<min>]  网页同步板子时间
 *   GET /api/clean[?keep=100]   只保留最新 keep 个 JPEG
 *   GET /api/space              查询 SD 剩余/总容量
 *   GET /api/sd/remount[?force=1] 重新挂载 /mnt
 *
 * 返回 1 = 已处理，0 = 不是本模块的路由。
 */
int osd_web_app_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_APP_H_ */
