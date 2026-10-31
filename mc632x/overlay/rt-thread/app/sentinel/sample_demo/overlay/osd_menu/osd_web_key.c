/*
 * osd_web_key.c —— 用手机/浏览器遥控 OSD 菜单
 *
 * 背景：osd_menu 原本靠 osd_main.c 里的 osd_config() 线程每秒轮询 tosd_event.cfg
 * 文件来接收按键。那条路在这块板子上用不了（msh 里没有 echo/vi/ymodem，写不进文件；
 * 而且那份代码的 fread(..., length-1, ...) 少读一个字节，必须写 "UP\n" 才能匹配）。
 *
 * 这里改成起一个 HTTP 服务线程当唯一输入源：
 *   - OSD 全程只有一个线程在调 manager_*，因此不存在竞态（osd_menu 核心代码无锁）
 *   - 按键即时生效，没有 1 秒轮询延迟
 *   - 不再每秒读写一次 JFFS2 flash
 *
 * socket 写法照抄工程自带的 components/librtsp/src/rtsp.c:1332-1368。
 *
 * 路由（全部挂在 8080 这一个服务器上，避免第二个线程和第二个端口）：
 *   /            ISP 控制面板，由 osd_web_isp.c 认领（手机入口）
 *   /api/isp     ISP 状态读写，同上
 *   /osd         本文件的方向键遥控页
 *   /key /menu   本文件
 *   /api/term   网页 msh 终端（osd_web_term.c）
 * 面板路由的判断放在最前面（见 web_handle_request），所以 / 落到面板上。
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/prctl.h>

#include "config.h"
#include "osd_main.h"
#include "mock/key_event.h"
#include "osd_web_key.h"
#include "osd_web_dash.h"
#include "osd_web_isp.h"
#include "osd_web_cam.h"
#include "osd_web_stats.h"
#include "osd_web_clip.h"
#include "osd_web_app.h"
#include "osd_web_rec.h"
#include "osd_web_term.h"
#include "osd_web_sys.h"

#define OSD_WEB_KEY_PORT        (8080)
#define OSD_WEB_THREAD_PRIO     (130)
#define OSD_WEB_THREAD_STACK    (8 * 1024)
#define OSD_WEB_REQ_MAX         (512)

static volatile int g_web_exit      = 0;
static int          g_web_started   = 0;
static int          g_web_server_fd = -1;

/* 菜单当前参数。初值取 osd_main.h 的默认宏；/menu?... 可在运行时改写并立即重绘。
 * 注意 osd_main.c 的 menu_set_para() 对 font/x/y/color/layer 是"非 0 才覆盖"，
 * 所以这里必须持有完整的一份值，不能只存被改过的字段。 */
static OSD_MENU_PARA_t g_web_para = {
    .osd_group_id   = OSD_DEF_GROUP_ID,
    .osd_channel_id = OSD_DEF_CHANNEL,
    .osd_font_size  = OSD_DEF_FONT_SIZE,
    .row_space      = OSD_DEF_RAW_SPACE,
    .pos_offset_x   = OSD_DEF_POS_OFFSET_X,
    .pos_offset_y   = OSD_DEF_POS_OFFSET_Y,
    .font_color     = OSD_DEF_FONT_COLOR,
    .edge_color     = OSD_DEF_EDGE_COLOR,
    .layerMaxWidth  = OSD_DEF_MAX_LAYER_W,
    .layerMaxHeight = OSD_DEF_MAX_LAYER_H,
};

/* ------------------------------------------------------------------ */
/* 迷你 HTTP                                                           */
/* ------------------------------------------------------------------ */

/* send 不保证一次写完。本工程 LWIP_SO_SNDTIMEO 是开的，一旦有人设了发送超时，
 * lwIP 就不再把阻塞发送内部循环起来（api_lib.c 的 dontblock 分支），
 * 几 KB 的页面会在 TCP_SND_BUF 处被静默截断。所以这里自己循环。*/
static int osd_web_send_all(int fd, const char *p, int n)
{
    int sent = 0;
    int stalls = 0;

    while (sent < n)
    {
        int r = send(fd, p + sent, n - sent, 0);

        if (r > 0)
        {
            sent += r;
            stalls = 0;
            continue;
        }

        /* 对端只是暂时不收（发送超时）时继续等，最多连续 3 次，约 6 秒。
         * 用户中途关掉页面/点返回时，浏览器可能既不读也不关 socket；以前这里
         * 会永远阻塞，把 8080 唯一的服务线程卡死，整个看板都不响应。*/
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && stalls < 3)
        {
            stalls++;
            continue;
        }

        /* 图片/视频比页面大，发到一半失败时这里会留下现场；浏览器端就表现为
         * “图片显示不全”。*/
        printf("[osd_web] send failed: sent=%d/%d errno=%d\n", sent, n, errno);
        return -1;
    }
    return sent;
}

void osd_web_send_bin_ex(int fd, const char *status, const char *ctype,
                         const void *body, int len, const char *extra)
{
    char hdr[320];
    char whole[768];
    int  n;
    int  has_extra = (extra != NULL && extra[0] != '\0');

    n = snprintf(hdr, sizeof(hdr),
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "Cache-Control: no-store\r\n"
                 "Connection: close\r\n"
                 "%s%s\r\n",
                 status, ctype, len,
                 has_extra ? extra : "",
                 has_extra ? "\r\n" : "");
    /* 头 + 小 body 合成一次 send：lwIP 默认开 Nagle，分两次发时 body 会
     * 等头的 ACK 才出去，每个 API 请求白白多一个 RTT，看板轮询时很明显。*/
    if (n > 0 && body && len > 0 && n + len <= (int)sizeof(whole))
    {
        memcpy(whole, hdr, (size_t)n);
        memcpy(whole + n, body, (size_t)len);
        osd_web_send_all(fd, whole, n + len);
        return;
    }
    if (n > 0)
        osd_web_send_all(fd, hdr, n);
    if (body && len > 0)
        osd_web_send_all(fd, (const char *)body, len);
}

void osd_web_send_bin(int fd, const char *status, const char *ctype,
                      const void *body, int len)
{
    osd_web_send_bin_ex(fd, status, ctype, body, len, NULL);
}

void osd_web_send(int fd, const char *status, const char *ctype, const char *body)
{
    osd_web_send_bin(fd, status, ctype, body, body ? (int)strlen(body) : 0);
}

int osd_web_send_raw(int fd, const void *p, int n)
{
    if (p == NULL || n <= 0)
    {
        return 0;
    }

    return (osd_web_send_all(fd, (const char *)p, n) == n) ? 0 : -1;
}

/* 给静态资源用：允许浏览器缓存，减少每次切页时重新下载几十 KB 的 CSS/JS，
 * 这是页面来回切换卡顿的主要原因之一。*/
void osd_web_send_cached(int fd, const char *status, const char *ctype,
                         const char *body, int max_age)
{
    char hdr[320];
    int  len = body ? (int)strlen(body) : 0;
    int  n;

    n = snprintf(hdr, sizeof(hdr),
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "Cache-Control: public, max-age=%d\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 status, ctype, len, max_age);
    if (n > 0)
        osd_web_send_all(fd, hdr, n);
    if (body && len > 0)
        osd_web_send_all(fd, body, len);
}

/*
 * 在 query 串里找 key=value 的整数值，找不到返回 def。
 * 要求 key 出现在 query 开头或紧跟 '&'，否则 "x=" 会命中 "font=x=" 这类子串。
 */
int osd_web_query_get_int(const char *query, const char *key, int def)
{
    char        pat[16];
    size_t      klen = strlen(key);
    const char *p;

    if (!query || klen + 2 > sizeof(pat))
        return def;

    snprintf(pat, sizeof(pat), "%s=", key);

    p = query;
    while ((p = strstr(p, pat)) != NULL)
    {
        if (p == query || p[-1] == '&')
            return atoi(p + klen + 1);
        p += klen + 1;
    }
    return def;
}

static int osd_web_hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* 同上，取字符串值；找到就拷贝到 out 并返回 out，否则返回 NULL。
 * 会做一次 %XX 解码：文件名在浏览器里用 encodeURIComponent 编码后，
 * "left/xxx.jpg" 会变成 "left%2Fxxx.jpg"。*/
static char *query_get_str_impl(const char *query, const char *key, char *out,
                                size_t outsz, int decode)
{
    char        pat[16];
    size_t      klen = strlen(key);
    const char *p;
    size_t      i = 0;

    if (!query || klen + 2 > sizeof(pat) || outsz == 0)
        return NULL;

    snprintf(pat, sizeof(pat), "%s=", key);

    p = query;
    while ((p = strstr(p, pat)) != NULL)
    {
        if (p == query || p[-1] == '&')
        {
            p += klen + 1;
            /* 边界判断放最前：原来的写法把 i < outsz-1 放在最后，
             * 靠巧合才没越界，改一个条件就会溢出。*/
            while (i < outsz - 1 && p[i] && p[i] != '&' && p[i] != ' ')
            {
                out[i] = p[i];
                i++;
            }
            out[i] = '\0';

            /* URL 解码（就地） */
            if (decode)
            {
                char *src = out;
                char *dst = out;

                while (*src != '\0')
                {
                    int hi = osd_web_hexval(src[1]);
                    int lo = (hi >= 0) ? osd_web_hexval(src[2]) : -1;

                    if (*src == '%' && hi >= 0 && lo >= 0)
                    {
                        *dst++ = (char)((hi << 4) | lo);
                        src += 3;
                    }
                    else
                    {
                        *dst++ = *src++;
                    }
                }
                *dst = '\0';
            }

            return out;
        }
        p += klen + 1;
    }
    return NULL;
}

char *osd_web_query_get_str(const char *query, const char *key, char *out, size_t outsz)
{
    return query_get_str_impl(query, key, out, outsz, 1);
}

char *osd_web_query_get_str_raw(const char *query, const char *key, char *out, size_t outsz)
{
    return query_get_str_impl(query, key, out, outsz, 0);
}

/* ------------------------------------------------------------------ */
/* 路由                                                                */
/* ------------------------------------------------------------------ */

static void web_handle_key(int fd, const char *query)
{
    char key[12];
    int  event = 0;

    if (osd_web_query_get_str(query, "k", key, sizeof(key)))
    {
        if (!strcmp(key, "UP"))         event = EVENT_UP_CLICK;
        else if (!strcmp(key, "DOWN"))  event = EVENT_DOWN_CLICK;
        else if (!strcmp(key, "LEFT"))  event = EVENT_LEFT_CLICK;
        else if (!strcmp(key, "RIGHT")) event = EVENT_RIGHT_CLICK;
        else if (!strcmp(key, "ENTER")) event = EVENT_ENTER_CLICK;
    }

    if (!event)
    {
        osd_web_send(fd, "400 Bad Request", "text/plain; charset=utf-8",
                     "bad key, use UP/DOWN/LEFT/RIGHT/ENTER\n");
        return;
    }

    osd_menu_handle(event);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok\n");
}

static void web_handle_menu(int fd, const char *query)
{
    char buf[256];

    g_web_para.osd_group_id   = osd_web_query_get_int(query, "g",     g_web_para.osd_group_id);
    g_web_para.osd_channel_id = osd_web_query_get_int(query, "c",     g_web_para.osd_channel_id);
    g_web_para.osd_font_size  = osd_web_query_get_int(query, "font",  g_web_para.osd_font_size);
    g_web_para.row_space      = osd_web_query_get_int(query, "space", g_web_para.row_space);
    g_web_para.pos_offset_x   = osd_web_query_get_int(query, "x",     g_web_para.pos_offset_x);
    g_web_para.pos_offset_y   = osd_web_query_get_int(query, "y",     g_web_para.pos_offset_y);
    g_web_para.layerMaxWidth  = osd_web_query_get_int(query, "lw",    g_web_para.layerMaxWidth);
    g_web_para.layerMaxHeight = osd_web_query_get_int(query, "lh",    g_web_para.layerMaxHeight);

    /* 覆盖 g_osd_para 并强制重绘 */
    osd_menu_paracfg(&g_web_para);

    snprintf(buf, sizeof(buf),
             "group=%u channel=%u font=%u space=%u x=%u y=%u layer=%ux%u\n",
             g_web_para.osd_group_id, g_web_para.osd_channel_id,
             g_web_para.osd_font_size, g_web_para.row_space,
             g_web_para.pos_offset_x, g_web_para.pos_offset_y,
             g_web_para.layerMaxWidth, g_web_para.layerMaxHeight);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", buf);
}

/*
 * 响应发完后不要立刻 close()。
 *
 * lwIP 的 send() 返回时只保证数据已经排进 TCP 发送队列，不保证对端已 ACK。
 * 小页面（< TCP_SND_BUF）没问题，但 34KB 的存证图会超过 24576 的发送缓冲；
 * 如果这时马上 close，连接进入关闭流程，浏览器只收到已经发出去的那部分，
 * 表现就是图片底部整块灰色。这里先半关闭发送方向，让 FIN 排在数据后面，
 * 再等浏览器把数据读完并关闭；超时 500ms 兜底。
 */
static void web_finish(int fd)
{
    struct timeval tv;
    char           tmp[128];

    /* 请求读取在第一个空行处停止，可能还有残余 RX；先非阻塞排空，
     * 避免关闭时因为未读数据触发 RST。*/
    while (recv(fd, tmp, sizeof(tmp), MSG_DONTWAIT) > 0)
        ;

    shutdown(fd, SHUT_WR);

    tv.tv_sec  = 0;
    tv.tv_usec = 500 * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (recv(fd, tmp, sizeof(tmp), 0) > 0)
        ;
}

static void web_handle_request(int fd, char *req)
{
    char *path, *query, *sp;

    /* 只处理请求行： "GET /key?k=UP HTTP/1.1" */
    if (strncmp(req, "GET ", 4) != 0)
    {
        osd_web_send(fd, "405 Method Not Allowed", "text/plain; charset=utf-8", "use GET\n");
        return;
    }

    path = req + 4;
    sp   = strchr(path, ' ');
    if (sp)
        *sp = '\0';

    query = strchr(path, '?');
    if (query)
        *query++ = '\0';

    /* 新看板认领 /、/dash.css、/dash.js；/isp 仍由 ISP 面板认领 */
    if (osd_web_dash_handle(fd, path, query))
        return;

    /* ISP 面板认领 /isp 和 /api/isp */
    if (osd_web_isp_handle(fd, path, query))
        return;

    /* LCD 画面源切换: /api/cam */
    if (osd_web_cam_handle(fd, path, query))
        return;

    /* 抓拍 / AI 叠加 / 事件: /api/snap, /api/overlay, /api/events */
    if (osd_web_app_handle(fd, path, query))
        return;

    /* 录像与报告: /api/rec*, /api/report*, /report */
    if (osd_web_rec_handle(fd, path, query))
        return;

    /* 安防统计: /stats, /api/stats */
    if (osd_web_stats_handle(fd, path, query))
        return;

    /* 事件短视频: /clip, /api/clip */
    if (osd_web_clip_handle(fd, path, query))
        return;

    /* 网页 msh 终端: /api/term, /api/term/out */
    if (osd_web_term_handle(fd, path, query))
        return;

    /* 设备状态: /api/sysinfo */
    if (osd_web_sys_handle(fd, path, query))
        return;

    if (!strcmp(path, "/key"))
        web_handle_key(fd, query);
    else if (!strcmp(path, "/menu"))
        web_handle_menu(fd, query);
    else
        osd_web_send(fd, "404 Not Found", "text/plain; charset=utf-8",
                     "try / , /stats , /api/isp , /key?k=UP , /menu?g=1&c=0&font=32\n");
}

/* ------------------------------------------------------------------ */
/* 服务线程                                                            */
/* ------------------------------------------------------------------ */

static void *osd_web_key_thread(void *arg)
{
    struct sockaddr_in addr;
    struct timeval     tv;
    struct timeval     rcvto;
    fd_set             fds;
    char               req[OSD_WEB_REQ_MAX + 1];
    int                fd, ret;

    (void)arg;
    prctl(PR_SET_NAME, "osd_web_key");

    rcvto.tv_sec  = 3;
    rcvto.tv_usec = 0;

    while (!g_web_exit)
    {
        socklen_t addr_len = sizeof(addr);   /* accept 会改写它，每轮重置 */

        FD_ZERO(&fds);
        FD_SET(g_web_server_fd, &fds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        ret = select(g_web_server_fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0)
            continue;
        if (!FD_ISSET(g_web_server_fd, &fds))
            continue;

        fd = accept(g_web_server_fd, (struct sockaddr *)&addr, &addr_len);
        if (fd < 0)
            continue;

        /* lwIP 不把监听套接字上的 SO_RCVTIMEO 继承给 accept 出来的连接
         * （net/lwip-2.1.2/src/api/api_msg.c 里 conn->recv_timeout = 0，
         * alloc_socket() 也不拷贝），所以这里必须补设一次。
         * 否则浏览器的 TCP 预连接（Chrome/Safari 会主动开空连接不发数据）
         * 会把唯一的服务线程永久卡死在下面的 recv 上，表现为"网页突然不响应"。*/
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rcvto, sizeof(rcvto));

        /* 发送也要设超时：大文件（1.45MB 的 AVI）发到一半用户点了返回，
         * 浏览器不读也不关，send 会一直阻塞，把唯一的服务线程卡死。
         * osd_web_send_all() 里对 EAGAIN 最多再重试 3 次。*/
        {
            struct timeval sndto;

            sndto.tv_sec  = 2;
            sndto.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sndto, sizeof(sndto));
        }

        /* 关掉 Nagle：图片/视频响应是"头 + body"两次 send，Nagle 会把 body
         * 压到头的 ACK 回来才发，每个请求多一个 RTT。局域网里 RTT 小，
         * 但看板一次刷新十来个缩略图时累积起来就能感觉到卡。*/
        {
            int on = 1;

            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
        }

        /* 浏览器悬停链接、预加载时会留空连接不发请求。以前这种 socket
         * 直接进 recv，要按 RCVTIMEO 干等 3 秒，后面的真实请求全被堵住，
         * 表现为“点返回看板卡住”。先 select 等 800ms，没数据就关掉。*/
        {
            fd_set         rfds;
            struct timeval wto;

            wto.tv_sec  = 0;
            wto.tv_usec = 800 * 1000;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            if (select(fd + 1, &rfds, NULL, NULL, &wto) <= 0)
            {
                close(fd);
                continue;
            }
        }

        /* 一次 recv 不等于一个完整请求：请求行可能被 TCP 分段切开。
         * 收到空行或缓冲满为止。缓冲多留 1 字节且 memset 过，
         * 所以第 OSD_WEB_REQ_MAX 号字节恒为 0，解析不会读飞。*/
        memset(req, 0, sizeof(req));
        ret = 0;
        while (ret < OSD_WEB_REQ_MAX)
        {
            int n = recv(fd, req + ret, OSD_WEB_REQ_MAX - ret, 0);
            if (n <= 0)
                break;
            ret += n;
            if (strstr(req, "\r\n\r\n"))
                break;
        }
        if (ret > 0)
        {
            web_handle_request(fd, req);
            web_finish(fd);
        }

        close(fd);
    }
    return NULL;
}

int osd_web_key_start(void)
{
    struct sockaddr_in addr;
    struct timeval     timeout;
    pthread_attr_t     attr;
    pthread_t          thread;
    int                on = 1;

    if (g_web_started)
        return 0;

    if (osd_web_term_init() != 0)
        printf("[osd_web] webterm unavailable\n");

    g_web_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_web_server_fd < 0)
    {
        printf("[osd_web] socket failed\n");
        return -1;
    }

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    setsockopt(g_web_server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    setsockopt(g_web_server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(OSD_WEB_KEY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_web_server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("[osd_web] bind %d failed\n", OSD_WEB_KEY_PORT);
        goto fail;
    }
    if (listen(g_web_server_fd, 5) != 0)
    {
        printf("[osd_web] listen failed\n");
        goto fail;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#ifdef __RTTHREAD_OS__
    {
        struct sched_param param;
        param.sched_priority = OSD_WEB_THREAD_PRIO;
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setstacksize(&attr, OSD_WEB_THREAD_STACK);
    }
#endif

    if (pthread_create(&thread, &attr, osd_web_key_thread, NULL) != 0)
    {
        printf("[osd_web] create thread failed\n");
        goto fail;
    }

    g_web_started = 1;
    printf("[osd_web] key server ready on port %d (menu group=%u channel=%u font=%u)\n",
           OSD_WEB_KEY_PORT, g_web_para.osd_group_id, g_web_para.osd_channel_id,
           g_web_para.osd_font_size);
    return 0;

fail:
    close(g_web_server_fd);
    g_web_server_fd = -1;
    return -1;
}
