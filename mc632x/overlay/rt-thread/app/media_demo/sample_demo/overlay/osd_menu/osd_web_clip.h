#ifndef _OSD_WEB_CLIP_H_
#define _OSD_WEB_CLIP_H_

/*
 * 事件短视频：
 *   GET /api/clip?name=evt_..._clip.avi   二进制 AVI
 *   GET /clip?name=...                    网页播放器
 * 返回 1 = 已处理，0 = 不是本模块的路由。
 */
int osd_web_clip_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_CLIP_H_ */
