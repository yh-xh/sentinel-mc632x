#ifndef __STATS_H__
#define __STATS_H__

/*
 * 安防统计：进出人数、区域进入/离开、当前画面人数、最长停留。
 * 计数由 NNA 线程更新，网页线程只读；数据落盘到 /mnt/snap/stats.bin。
 */

void stats_init(void);

/* 清零今日计数（累计值保留）。*/
void stats_reset(void);

/* 绊线计数：in=1 进入，in=0 离开。rule/conf/count 用于记录最后事件。*/
void stats_on_trip(int in, int rule, int conf, int count);

/* 周界区域进入/离开计数。enter=1 进入，0 离开。*/
void stats_on_area(int enter, int rule, int count);

/* 区域停留：objId 进入时 enter=1，离开时 enter=0；内部算停留时长。*/
void stats_on_area_obj(int objId, int enter);

/* 每帧画面人数（NNA 线程）。*/
void stats_on_visible(int count);

/* 录像会话：录制开始时清零，结束时取出用于人流量报告。*/
void stats_session_begin(void);
void stats_session_end(unsigned *in, unsigned *out, unsigned *peak);

/* 输出 text/plain：第一行 ok=1 synced=... date=...，后面 24 行 h=HH in=N out=M。*/
int stats_format(char *out, int cap);

#endif /* __STATS_H__ */
