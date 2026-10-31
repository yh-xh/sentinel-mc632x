#ifndef __CLIP_STORE_H__
#define __CLIP_STORE_H__

/*
 * 事件短视频：MJPEG 前后 5 秒，写成 AVI（MJPG）。
 *
 * 观察者在 get_stream 线程里只做 memcpy 到环形缓冲；
 * 事件线程 begin -> 等 5 秒 -> finish 写 SD。
 */

int  clip_store_init(void);
void clip_store_deinit(void);

/* get_stream 的 MJPEG 回调里调用；内部只认 AI 那一路。*/
void clip_store_on_frame(int server_index, const unsigned char *data, int len);

/* 事件线程调用：冻结 pre 环，开始收 post 帧。返回 1=有 pre 帧可录。*/
int  clip_store_begin(unsigned event_ms);

/* 等 post 5 秒（期间观察者继续收帧）。*/
int  clip_store_wait_end(void);

/* 写 AVI 到 path；成功返回 0，size/w/h 可空。*/
int  clip_store_finish(const char *path, int cap, unsigned *size, int *w, int *h);

/* 放弃本次 clip，恢复 pre 环。*/
void clip_store_abort(void);

#endif /* __CLIP_STORE_H__ */
