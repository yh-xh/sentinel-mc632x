#ifndef _OSD_WEB_STATS_H_
#define _OSD_WEB_STATS_H_

/*
 * 安防统计：
 *   GET /stats        今日进出/周界/停留 + 24 小时柱状图
 *   GET /api/stats    文本键值；?reset=1 清零今日
 * 返回 1 = 已处理，0 = 不是本模块的路由。
 */
int osd_web_stats_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_STATS_H_ */
