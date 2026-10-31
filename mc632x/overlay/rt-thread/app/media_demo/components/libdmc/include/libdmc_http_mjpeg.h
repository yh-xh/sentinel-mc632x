#ifndef __libdmc_http_mjpeg_h__
#define __libdmc_http_mjpeg_h__
#include "types/type_def.h"

extern FH_SINT32 dmc_http_mjpeg_subscribe(FH_UINT16 port);
extern FH_SINT32 dmc_http_mjpeg_unsubscribe(FH_VOID);

/* 每收到一帧 MJPEG 回调一次（在 get_stream 线程）。
 * 回调里 data 只在函数返回前有效，观察者必须同步复制。*/
typedef void (*dmc_mjpeg_frame_obs_fn)(int server_index,
                                       const unsigned char *data, int len);
extern void dmc_http_mjpeg_set_frame_observer(dmc_mjpeg_frame_obs_fn fn);

#endif /*__libdmc_http_mjpeg_h__*/
