#ifndef _OSD_WEB_TERM_H_
#define _OSD_WEB_TERM_H_

/*
 * 网页 msh 终端（方案一：HTTP 命令控制台）
 *
 *   GET /api/term?cmd=<url-encoded>[&force=1]
 *       提交一条命令。危险命令第一次返回 confirm=1，前端确认后带 force=1 重发。
 *   GET /api/term/out?since=<seq>
 *       取回 since 之后的新输出；since 传 -1 表示从当前时刻开始。
 *       响应头：
 *         X-Term-Since: 下一次轮询应传的序号
 *         X-Term-Lost:  1 表示输出太快，中间有内容被覆盖
 *         X-Term-Busy:  1 表示板端命令仍在执行
 *
 * 命令在独立的 webterm 线程里调用 msh_exec() 执行，不占用 8080 服务线程。
 * 执行期间 console 和 stdio 临时切到 "webterm" 内存设备，输出被缓存后由
 * 网页轮询取走；执行完自动切回 uart0。
 */
int osd_web_term_init(void);
int osd_web_term_handle(int fd, const char *path, const char *query);

#endif /* _OSD_WEB_TERM_H_ */
