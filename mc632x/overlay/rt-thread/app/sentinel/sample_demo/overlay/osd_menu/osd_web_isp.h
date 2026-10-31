#ifndef _OSD_WEB_ISP_H_
#define _OSD_WEB_ISP_H_

/*
 * ISP 控制面板。挂在 osd_web_key.c 的 8080 服务器上（不能另起一个，
 * 端口已被占用，而且 osd_menu 核心无锁、必须单线程）。
 *
 *   GET /                                  面板页面（手机入口）
 *   GET /api/isp                           读全量状态
 *   GET /api/isp?k=<key>&v=<int>           写一项，回全量状态
 *   GET /api/isp?<key>=<int>&<key>=<int>…  批量写（带 k 时忽略），回全量状态
 *
 * 线上格式沿用板子自己的 query 语法： "br=0&ct=0&sat=0&…"（约 90 字节）。
 * 不手搓 JSON —— 反正 == 板子已经在用的解析器，一个编码器一个解码器，
 * 无转义规则、无嵌套，客户端也是我们自己写的。
 *
 * 控件清单与取值范围见 osd_web_isp.c 的 g_ctl[]。
 *
 * 返回 1 表示这个 path 归本模块处理，调用方不要再往下走；0 表示不归。
 */
int osd_web_isp_handle(int fd, const char *path, const char *query);

/*
 * 把网页线程排队的 ISP 改动落到硬件上。
 *
 * ★ 必须由 ISP 线程调用（sample_common_isp.c 的 isp_run 循环里），
 *   不能从网页线程调 —— 全工程 ISP 参数路径上没有任何锁，而 ISP 线程
 *   每 10ms 就在跑 API_ISP_Run()，并发会丢更新。
 *
 * devid 传调用者自己的 ISP 设备号；本函数内部只处理 == FH_APP_GRP_ID 的那个，
 * 所以每个 group 的 ISP 线程都调一次也没关系。没有待处理请求时立即返回。
 */
void isp_web_pump(int devid);

#endif /* _OSD_WEB_ISP_H_ */
