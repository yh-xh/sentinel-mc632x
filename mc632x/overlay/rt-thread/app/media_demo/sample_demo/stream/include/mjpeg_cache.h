#ifndef __MJPEG_CACHE_H__
#define __MJPEG_CACHE_H__

#include "types/type_def.h"

/* 单帧上限。800x480 JPEG 通常 50~150KB，留足余量。*/
#define MJPEG_CACHE_BUF_SIZE (256 * 1024)

/* server_index 与 dmc_http_mjpeg 的端口序号一致：
 * 1111 -> 0, 1112 -> 1, ... */

int  mjpeg_cache_init(void);
void mjpeg_cache_deinit(void);

/* 在 get_stream 线程的 MJPEG 回调里调用；data 只在回调期间有效，
 * 这里会同步 memcpy，绝不保存指针。*/
void mjpeg_cache_on_frame(int server_index, const unsigned char *data, int len);

/* 把最新一帧复制到 dst。返回帧长度；-1 无帧，-2 目标缓冲太小。*/
int  mjpeg_cache_copy(int server_index, unsigned char *dst, int cap);

/* 根据 (grp, chn) 查它的 server_index；未启用返回 -1。*/
int  mjpeg_cache_index_of(int grp_id, int chn_id);

/* 最新帧的序号，用于给 HTTP header 或调试打点。*/
unsigned mjpeg_cache_seq(int server_index);

int  mjpeg_cache_is_ready(int server_index);

#endif /* __MJPEG_CACHE_H__ */
