#ifndef _OSD_WEB_CAM_H_
#define _OSD_WEB_CAM_H_

/*
 * LCD 画面源切换的网页接口，挂在 osd_web_key.c 的 8080 服务器上。
 *
 *   GET /api/cam                读当前模式，回 "mode=0\n" 或 "mode=1\n"
 *   GET /api/cam?m=0|1          切换后回实际模式（切换失败也回 200 + 实际模式，
 *                               让前端自己对齐，不会卡在错误的按钮状态）
 *
 * 模式含义见 cam_switch.h。
 *
 * 返回 1 表示这个 path 归本模块处理，调用方不要再往下走；0 表示不归。
 */
int osd_web_cam_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_CAM_H_ */
