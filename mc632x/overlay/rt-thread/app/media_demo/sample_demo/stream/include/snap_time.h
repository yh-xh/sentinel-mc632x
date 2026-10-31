#ifndef __SNAP_TIME_H__
#define __SNAP_TIME_H__

/*
 * 板子没有 RTC，开机时间从 1970 开始。网页打开时把浏览器时间
 * 同步过来，这里只维护一个基于 get_ms() 的偏移，不去改系统时间，
 * 避免影响 SDK 自己的定时器。
 *
 * 时间按“本地时间”保存：utc_epoch + tz_min*60。
 */

/* utc_epoch: 浏览器 Date.now()/1000；tz_min: 东八区为 480 */
void snap_time_set(unsigned utc_epoch, int tz_min);

/* 返回 1=已同步，0=未同步；local_epoch 可为 NULL */
int  snap_time_get(unsigned *local_epoch);

/* 当前时间拆成日期 20261024 和时间 163012；未同步返回 0。*/
int  snap_time_name(char *date, int dcap, char *clk, int ccap);

/* 指定本地 epoch 拆成日期/时间；epoch=0 返回 0。*/
int  snap_time_name_at(unsigned epoch, char *date, int dcap, char *clk, int ccap);

/* local_epoch -> "2026-10-24 16:30:12"；0/未同步输出 "--" */
void snap_time_text(unsigned local_epoch, char *buf, int cap);

#endif /* __SNAP_TIME_H__ */
