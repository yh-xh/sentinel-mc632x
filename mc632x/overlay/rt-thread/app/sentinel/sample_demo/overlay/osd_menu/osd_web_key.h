#ifndef _OSD_WEB_KEY_H_
#define _OSD_WEB_KEY_H_

#include <stddef.h>

/*
 * 极小的 HTTP 服务线程（端口 8080）。它是 osd_menu 唯一的按键输入源，
 * 同时托管 ISP 控制面板（见 osd_web_isp.h）。
 *
 *   GET /                                 ISP 控制面板（手机入口）
 *   GET /osd                               OSD 菜单网页遥控（方向键）
 *   GET /key?k=UP|DOWN|LEFT|RIGHT|ENTER    注入一次按键
 *   GET /menu?g=1&c=0&font=32&space=10&x=128&y=128
 *                                          运行时改菜单参数并立即重绘；不带参数则回显当前值
 *   GET /api/isp                           ISP 状态读写（见 osd_web_isp.h）
 *
 * 必须在 osd_menu_init() 之后再调用（依赖 OSD 已完成 FHAdv_Osd_Init）。
 */
int osd_web_key_start(void);

/*
 * 下面两个是给同目录其它模块复用的。导出而不是各抄一份，是因为
 * 边界规则（p == query || p[-1] == '&'）只能有一份定义。
 */

/* 发一个完整 HTTP 响应。内部带 send 重试，处理部分写。 */
void osd_web_send(int fd, const char *status, const char *ctype, const char *body);

/* 可缓存的静态资源（看板的 CSS/JS）。页面里用 ?v=N 版本号，改资源时
 * 记得把 gen_dash.py 里的 v 加一，浏览器就会重新拉。 */
void osd_web_send_cached(int fd, const char *status, const char *ctype,
                         const char *body, int max_age);

/* 二进制安全版本：用显式长度，JPEG 里的 0x00 不会被截断。
 * extra 是可选的一条额外头（不含结尾 CRLF），例如 "X-Snap-File: /mnt/..."。*/
void osd_web_send_bin(int fd, const char *status, const char *ctype,
                      const void *body, int len);
void osd_web_send_bin_ex(int fd, const char *status, const char *ctype,
                         const void *body, int len, const char *extra);

/* 只发裸数据（已经在发 body 的流式响应里用）。返回 0 成功，<0 对端断开。 */
int  osd_web_send_raw(int fd, const void *p, int n);

/* 在 query 串里取 key=value 的整数值，找不到返回 def。
 * 注意 atoi 语义：atoi("abc") == 0，解析失败与 0 无法区分，
 * 调用方必须靠范围钳位兜底，不要信任返回值。 */
int osd_web_query_get_int(const char *query, const char *key, int def);

/* 同上取字符串；找到就拷进 out（含结尾 0）并返回 out，否则返回 NULL。 */
char *osd_web_query_get_str(const char *query, const char *key, char *out, size_t outsz);
char *osd_web_query_get_str_raw(const char *query, const char *key, char *out, size_t outsz);

#endif /* _OSD_WEB_KEY_H_ */
