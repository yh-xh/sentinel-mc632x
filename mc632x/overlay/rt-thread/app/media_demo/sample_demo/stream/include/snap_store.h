#ifndef __SNAP_STORE_H__
#define __SNAP_STORE_H__

#include "types/type_def.h"

#define SNAP_DIR                "/mnt/snap"
/* 左摄(G0/摄像头1) 与 右摄(G1/摄像头2) 的图片、视频分目录存放；
 * 事件日志仍在 SNAP_DIR 根下。*/
#define SNAP_DIR_LEFT           SNAP_DIR "/left"
#define SNAP_DIR_RIGHT          SNAP_DIR "/right"
#define SNAP_JPEG_MAX           (256 * 1024)
#define SNAP_CLIP_MAX           (4 * 1024 * 1024)
#define SNAP_EVENT_RING_NUM     16
#define SNAP_EVENT_COOLDOWN_MS  3000
#define SNAP_TEST_COOLDOWN_MS    300
#define SNAP_FILE_NAME_MAX      96      /* 网页按文件名读取时的缓冲上限 */

/* 事件类型：和 /api/events 输出的 kind 一致 */
#define SNAP_EV_TRIP            0   /* 绊线 */
#define SNAP_EV_PERI            1   /* 周界 */
#define SNAP_EV_MANUAL          2   /* 手动抓拍 */

/* 事件人工标注：-1 未标，0 误报，1 正确 */
#define SNAP_LABEL_NONE        (-1)
#define SNAP_LABEL_FP           0
#define SNAP_LABEL_TP           1

typedef struct
{
    unsigned seq;
    unsigned ms;
    unsigned epoch;     /* 本地 epoch 秒；0=当时还没同步时间 */
    unsigned kind;
    int      rule;
    unsigned mode;      /* TAD_TRIP_* / TAD_AREA_*，手动为 0 */
    unsigned conf;      /* 最大置信度 0-100 */
    unsigned count;     /* 触发时画面里的人数 */
    unsigned ok;        /* AI 图保存成功 */
    unsigned clean_ok;  /* G0 干净图保存成功 */
    int      label;     /* SNAP_LABEL_* */
    unsigned size;
    char     time[20];  /* "2026-10-24 16:30:12" 或 "--" */
    char     file[64];  /* AI 叠加图 */
    char     clean[64]; /* G0 干净图；没有则为空 */
    char     clip[64];  /* 事件短视频 AVI；没有则为空 */
    unsigned clip_size;
    unsigned clip_ok;
} snap_event_t;

/* 返回 0 成功；-1 初始化失败 */
int  snap_store_init(void);
void snap_store_deinit(void);

/* SD 是否真的挂在 /mnt（不会误判成 JFFS2 根分区）*/
int  snap_store_sd_ready(void);

/*
 * 自动事件抓拍开关（绊线/周界）。默认 0=关闭，避免一上电就往 SD 卡写图。
 * 关闭时检测和 OSD 叠加照常运行，只是不保存事件照片；手动抓拍不受影响。
 */
void snap_store_auto_set(int on);
int  snap_store_auto_get(void);

/* NNA 线程调用：只复制最新 AI 帧 + 标记 pending，不写 SD。
 * mode 传 TAD_TRIP_* / TAD_AREA_*；conf 为最大置信度 0-100；count 为人数。*/
void snap_store_notify_event(int kind, int rule_id, int mode, int conf, int count);

/*
 * 测试模式：事件冷却从 3s 降到 300ms，并每秒写一行 /mnt/snap/detect.csv。
 * 只影响事件记录，不改变自动存图开关（测试前仍要先布防/打开自动抓拍）。
 */
void snap_store_test_set(int on);
int  snap_store_test_get(void);

/* NNA 线程每帧调用：只更新内存里的最新检测值，不写 SD。*/
void snap_store_test_update(int count, int conf);

/* 人工标注：seq 对应 /api/events 里的序号。返回 0 成功。*/
int  snap_store_label_event(unsigned seq, int label);
/* 记录一次漏报（没有 seq），写进 labels.csv。*/
int  snap_store_label_miss(void);

/* SD 读写自检：写 64KB 模式数据、fsync、读回校验、删除。返回 0 成功。*/
int  snap_store_selftest(char *msg, int cap);

/* 读取 SNAP_DIR 下的文本日志（detect.csv / labels.csv 等）。
 * tail=1 时只读最后 cap-1 字节；返回实际读取字节数。*/
int  snap_store_read_text(const char *name, char *out, int cap, int tail);

/*
 * 手动抓拍。cam: 0=G0 干净, 1=G1 AI。
 * save=1 时写 SD，并在 path 里返回文件路径。
 * 返回 >0 = JPEG 长度；-1 无帧；-2 缓冲太小；-3 SD 未就绪；-4 写失败。
 */
int  snap_store_manual(int cam, unsigned char *out, int cap, int save,
                       char *path, int path_sz, unsigned *seq);

/* 复制事件环形缓冲，newest-first。返回条数。*/
int  snap_store_events(snap_event_t *out, int max);
int  snap_store_event_count(void);
unsigned snap_store_dropped(void);

/* 事件线程写完短视频后回填到事件环。seq 对应 /api/events 里的序号。*/
void snap_store_set_event_clip(unsigned seq, const char *clip,
                               unsigned size, int ok);

/* 按文件名读取 SNAP_DIR 下的 AVI（仅 .avi）。成功返回长度并 malloc 到 *out，
 * 调用方 free；-1 不存在，-2 太大，-3 SD 未就绪，-4 文件名非法。*/
int  snap_store_load_clip(const char *name, unsigned char **out, int *size);

/*
 * 分段读取 AVI（网页边读边发）。一次 read 最多 32KB，读完就放掉 SD 锁，
 * 避免几 MB 的整文件 read 把 SD 卷锁占住几秒、连带拖垮录像/抓拍。
 * 返回 fd（>=0）+ 文件大小；错误码同 snap_store_load_clip。
 */
#define SNAP_CLIP_STREAM_MAX (128u * 1024u * 1024u)
int  snap_store_clip_open(const char *name, unsigned *size);
int  snap_store_clip_read(int fd, void *buf, int len);
void snap_store_clip_close(int fd);

/* 按 seq 读取事件图片。返回长度；-1 不存在/未保存。*/
int  snap_store_read_event(unsigned seq, unsigned char *buf, int cap);

/*
 * 列出 SNAP_DIR 下的 JPEG，最新的在前（内存环形缓冲之外的完整相册）。
 * 输出文本：
 *   sd=1 total=N
 *   <文件名> <字节数>
 *   ...
 * 或 SD 未就绪时：
 *   sd=0
 * 返回写入 out 的字节数；参数非法返回 -1。
 */
int  snap_store_list_files(char *out, int cap, int max_files);

/*
 * 按纯文件名读取 SNAP_DIR 下的 JPEG（不允许路径分隔符）。
 * 返回 >0 长度；-1 不存在；-2 文件超过 cap；-3 SD 未就绪；-4 文件名非法。
 */
int  snap_store_read_file(const char *name, unsigned char *buf, int cap);

/*
 * 删除 SNAP_DIR 下的一个 JPEG（只接受纯文件名）。
 * 返回 0 成功；-1 文件不存在或删除失败；-3 SD 未就绪；-4 文件名非法。
 */
int  snap_store_delete_file(const char *name);

/* SD 剩余/总容量，单位 KB。返回 0 成功；-1 SD 未就绪。*/
int  snap_store_space(unsigned *free_kb, unsigned *total_kb);

/*
 * 只保留最新 keep 个 JPEG（按文件名时间排序），返回删除数量。
 * 返回 -3 = SD 未就绪，-1 = 目录读取失败。
 */
int  snap_store_clean_files(int keep);

/*
 * 重新挂载 /mnt。卡重新插好、或开机时 1 秒内没识别到卡导致没挂上时用。
 * force=1 会先卸载旧挂载再挂；返回 0 成功，-1 失败。
 */
int  snap_store_remount(int force);

#endif /* __SNAP_STORE_H__ */
