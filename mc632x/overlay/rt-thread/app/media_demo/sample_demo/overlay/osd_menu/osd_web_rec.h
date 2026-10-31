#ifndef __OSD_WEB_REC_H__
#define __OSD_WEB_REC_H__

/*
 * 手动/定时录像 + 人流量报告页面
 *   GET /api/rec            状态，?on=1&cam=0|1 开始，?on=0 停止
 *   GET /api/rec/sched      定时设置，?enabled=1&daily=0|1&date=YYYYMMDD&start=分钟&end=分钟
 *   GET /api/reports        报告列表
 *   GET /api/report?id=N    报告详情；?play=1 让小智播报；?ack=1 清待播报
 *   GET /api/report/pending 小智轮询用
 *   GET /report             报告页面
 */
int osd_web_rec_handle(int fd, const char *path, const char *query);

#endif
