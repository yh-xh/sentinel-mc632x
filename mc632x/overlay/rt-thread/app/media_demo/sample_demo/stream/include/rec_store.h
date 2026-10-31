#ifndef __REC_STORE_H__
#define __REC_STORE_H__

/*
 * rec_store —— 手动 / 定时录像（MJPEG AVI，5fps）
 *
 *   - MJPEG 观察者里只把选中的帧 memcpy 到队列，SD 写在独立线程；
 *   - 一路录一个 AVI，文件按摄像头进 /mnt/snap/left 或 right；
 *   - 定时：单次（日期+起止分钟）或每天重复；
 *   - 录制期间暂停布防的事件短视频（clip_store），截图/播报不受影响；
 *   - 结束后生成一条人流量报告，网页可查看，亦可让小智语音播报。
 */

#define REC_REPORT_MAX      8

typedef struct
{
    int      valid;
    unsigned id;
    int      cam;               /* 0=左摄 1=右摄 */
    unsigned start_epoch;       /* 本地 epoch 秒；0=未同步时间 */
    unsigned end_epoch;
    unsigned in;                /* 录制期间进入人数 */
    unsigned out;               /* 录制期间离开人数 */
    unsigned peak;              /* 录制期间画面最高人数 */
    unsigned frames;
    unsigned bytes;
    char     file[80];          /* 相对 SNAP_DIR，如 right/rec_...avi */
    char     t_start[20];
    char     t_end[20];
} rec_report_t;

int  rec_store_init(void);
void rec_store_deinit(void);

/* get_stream 的 MJPEG 回调里调用（两路都会进来）。*/
void rec_store_on_frame(int server_index, const unsigned char *data, int len);

/* 手动录制：cam 0=左摄 1=右摄；duration_sec=0 手动停止，>0 到点自动停（上限 3 分钟）。
 * 返回 0=已受理，<0=忙/未就绪。*/
int  rec_store_manual_start(int cam, int duration_sec);
int  rec_store_manual_stop(void);
int  rec_store_is_recording(void);
int  rec_store_active_cam(void);

/* 定时安排：daily=1 时忽略 date；start_min/end_min 是当天 0 点起的分钟数；
 * cam 0=左摄（原画） 1=右摄（AI）。返回 0 成功。*/
int  rec_sched_set(int daily, int date, int start_min, int end_min, int cam);
int  rec_sched_clear(void);
void rec_sched_get(int *enabled, int *daily, int *date,
                   int *start_min, int *end_min, int *cam);

/* 最近一次完成的报告 id；没有返回 -1。*/
int  rec_report_latest(void);
int  rec_report_get(unsigned id, rec_report_t *out);

/* 报告列表，最新在前；返回条数。*/
int  rec_report_list(rec_report_t *out, int max);

/* 网页“让小智播报”按钮：置一个待播报标记。*/
int  rec_report_play(unsigned id);

/* 小智轮询：有请求时返回 1 并带出人数；没有返回 0。*/
int  rec_report_pending(unsigned *id, unsigned *in, unsigned *out,
                        unsigned *peak, unsigned *req);
void rec_report_ack(void);

/* 一行状态文本，给 /api/rec/status。*/
int  rec_status_format(char *out, int cap);

#endif /* __REC_STORE_H__ */
