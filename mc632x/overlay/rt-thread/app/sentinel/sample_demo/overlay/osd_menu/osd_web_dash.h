#ifndef _OSD_WEB_DASH_H_
#define _OSD_WEB_DASH_H_

/*
 * SENTINEL 看板：
 *   GET /            看板 HTML
 *   GET /dash.css    看板样式
 *   GET /dash.js     看板脚本
 *
 * 返回 1 = 已处理，0 = 不是本模块的路由。
 */
int osd_web_dash_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_DASH_H_ */
