/*
 * osd_web_term.c —— 网页 msh 命令控制台
 *
 * 思路：
 *   1. 注册一个名为 "webterm" 的字符设备，write 回调把内容写进环形缓冲；
 *   2. 命令来时，工作线程打开 capture 标志；内核 rt_kprintf 的输出钩子把
 *      该线程的 console 输出额外复制一份到 webterm，同时只把 stdout/stderr
 *      临时指向 webterm 以抓 printf。console 始终是 uart0；
 *   3. 执行完清 capture、恢复 stdout，网页通过 /api/term/out?since=N 取输出。
 *
 * 关键：不切 console、不动 stdin、不关闭 uart0 的任何句柄。
 * 调试时试过 libc_stdio_set_console() 和 rt_console_set_device("webterm")：
 * 前者会 fclose 掉 uart0 的 stdio FILE（finsh 正阻塞在它的 getchar() 上，
 * 引用计数归零后串口驱动被 close）；后者会把 console 抢走，导致 minicom
 * 在网页命令执行期间看不到回显。现在的 tee 方案让两边真正可以同时使用。
 *
 * 不依赖串口：USB-TTL 只是 uart0 的物理出口，网页终端走的是内存设备。
 * 命令在 webterm 线程里跑，8080 服务线程只负责入队/取输出，不会阻塞。
 */
#include <rtthread.h>
#include <rthw.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/reent.h>

#include "msh.h"

#include "osd_web_key.h"
#include "osd_web_term.h"

#define TERM_CMD_MAX        (192)
#define TERM_OUT_SIZE       (32 * 1024)   /* 必须是 2 的幂 */
#define TERM_HTTP_CHUNK      (4096)
#define TERM_WORKER_STACK   (16 * 1024)

static char                 g_out[TERM_OUT_SIZE];
static volatile rt_uint32_t g_out_total;
static rt_uint32_t          g_out_head;

static struct rt_device     g_dev;
static rt_mq_t              g_mq;
static rt_thread_t          g_thread;
static volatile int         g_busy;
static int                  g_inited;

/* 网页命令执行期间，把该线程的 rt_kprintf 输出额外复制到 webterm。
 * console 始终是 uart0，minicom 不会被抢走。*/
static volatile int         g_term_capture;
static rt_thread_t          g_term_thread;

/* ------------------------------------------------------------------ */
/* 输出环形缓冲                                                       */
/* ------------------------------------------------------------------ */

static void term_out_write(const char *buf, int len)
{
    rt_base_t level;
    int       i;

    if (buf == RT_NULL || len <= 0)
        return;

    level = rt_hw_interrupt_disable();
    for (i = 0; i < len; i++)
    {
        g_out[g_out_head] = buf[i];
        g_out_head = (g_out_head + 1) & (TERM_OUT_SIZE - 1);
        g_out_total++;
    }
    rt_hw_interrupt_enable(level);
}

static int term_out_read(unsigned int since, char *dst, int cap,
                         unsigned int *next, int *lost)
{
    unsigned int total, start;
    rt_base_t    level;
    int          n, i;

    if (dst == RT_NULL || cap <= 0)
    {
        *next = since;
        *lost = 0;
        return 0;
    }

    level = rt_hw_interrupt_disable();
    total = g_out_total;

    if (since > total)
        since = total;

    start = since;
    if (total - start > TERM_OUT_SIZE)
    {
        start = total - TERM_OUT_SIZE;
        *lost = 1;
    }
    else
    {
        *lost = 0;
    }

    n = (int)(total - start);
    if (n > cap)
        n = cap;

    for (i = 0; i < n; i++)
        dst[i] = (char)g_out[(start + i) & (TERM_OUT_SIZE - 1)];

    *next = start + (unsigned int)n;
    rt_hw_interrupt_enable(level);
    return n;
}

/* 内核控制台分流钩子：只复制 webterm 线程自己执行的命令输出。
 * UART 仍是系统 console，finsh 的回显/提示不会混进来。*/
static void term_console_hook(const char *str)
{
    if (g_term_capture && str != NULL && rt_thread_self() == g_term_thread)
        term_out_write(str, (int)rt_strlen(str));
}

/* ------------------------------------------------------------------ */
/* webterm 设备                                                       */
/* ------------------------------------------------------------------ */

static rt_err_t term_dev_init(rt_device_t dev)
{
    (void)dev;
    return RT_EOK;
}

static rt_err_t term_dev_open(rt_device_t dev, rt_uint16_t oflag)
{
    (void)dev;
    (void)oflag;
    return RT_EOK;
}

static rt_err_t term_dev_close(rt_device_t dev)
{
    (void)dev;
    return RT_EOK;
}

static rt_size_t term_dev_read(rt_device_t dev, rt_off_t pos,
                               void *buffer, rt_size_t size)
{
    (void)dev;
    (void)pos;
    (void)buffer;
    (void)size;
    return 0;
}

static rt_size_t term_dev_write(rt_device_t dev, rt_off_t pos,
                                const void *buffer, rt_size_t size)
{
    (void)dev;
    (void)pos;
    term_out_write((const char *)buffer, (int)size);
    return size;
}

static rt_err_t term_dev_control(rt_device_t dev, int cmd, void *args)
{
    (void)dev;
    (void)cmd;
    (void)args;
    return RT_EOK;
}

/* ------------------------------------------------------------------ */
/* 命令执行                                                           */
/* ------------------------------------------------------------------ */

/*
 * 只替换 stdout/stderr，stdin 保持 uart0 不动。
 *
 * 不能用 libc_stdio_set_console("webterm")：它会 fclose 掉 uart0 的
 * stdio FILE，而 finsh 线程这时正阻塞在这个 FILE 的 getchar() 上；
 * uart0 的引用计数被减到 0 后串口驱动被 close，minicom 就永久无响应。
 * 这里直接换 _GLOBAL_REENT 里的输出指针，不关任何已有句柄。
 */
static FILE *g_term_fp;
static FILE *g_term_old_out;
static FILE *g_term_old_err;

static int term_stdio_begin(void)
{
    FILE *fp = fopen("/dev/webterm", "w");

    if (fp == NULL)
        return -1;

    setvbuf(fp, NULL, _IONBF, 0);

    g_term_old_out = _GLOBAL_REENT->_stdout;
    g_term_old_err = _GLOBAL_REENT->_stderr;
    _GLOBAL_REENT->_stdout = fp;
    _GLOBAL_REENT->_stderr = fp;
    g_term_fp = fp;
    return 0;
}

static void term_stdio_end(void)
{
    if (g_term_fp == NULL)
        return;

    _GLOBAL_REENT->_stdout = g_term_old_out;
    _GLOBAL_REENT->_stderr = g_term_old_err;
    fclose(g_term_fp);
    g_term_fp = NULL;
}

static void term_run(char *cmd)
{
    char mark[40];
    int  ret;
    int  n;

    /* 不切 console：UART 保持原样，rt_kprintf 由内核钩子复制到 webterm；
     * stdout 只重定向 printf，stdin 不动。*/
    g_term_capture = 1;
    (void)term_stdio_begin();

    ret = msh_exec(cmd, rt_strlen(cmd));

    n = snprintf(mark, sizeof(mark), "\r\n[exit %d]\r\n", ret);
    if (n > 0)
        term_out_write(mark, n);

    term_stdio_end();
    g_term_capture = 0;
}

static void term_worker(void *arg)
{
    char cmd[TERM_CMD_MAX];

    (void)arg;

    g_term_thread = rt_thread_self();

    while (1)
    {
        if (rt_mq_recv(g_mq, cmd, sizeof(cmd), RT_WAITING_FOREVER) == RT_EOK)
        {
            cmd[TERM_CMD_MAX - 1] = '\0';
            term_run(cmd);
            g_busy = 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* HTTP 接口                                                          */
/* ------------------------------------------------------------------ */

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int url_decode(const char *in, char *out, int cap)
{
    int n = 0;

    while (*in)
    {
        int c;

        if (*in == '%')
        {
            int hi, lo;

            if (in[1] == '\0' || in[2] == '\0')
                return -1;
            hi = hexval((unsigned char)in[1]);
            lo = hexval((unsigned char)in[2]);
            if (hi < 0 || lo < 0)
                return -1;
            c = (hi << 4) | lo;
            in += 3;
        }
        else if (*in == '+')
        {
            c = ' ';
            in++;
        }
        else
        {
            c = (unsigned char)*in++;
        }

        if (n >= cap - 1)
            return -1;
        out[n++] = (char)c;
    }

    out[n] = '\0';
    return n;
}

static int term_is_dangerous(const char *cmd)
{
    static const char *bad[] = {
        "reboot", "reset", "poweroff", "shutdown",
        "fal", "sf", "mkfs", "rm", "rmdir", "mv", "cp",
        "mtest", "memtrace",
    };
    char word[24];
    int  n = 0;
    unsigned int i;

    while (*cmd == ' ' || *cmd == '\t')
        cmd++;

    while (*cmd && *cmd != ' ' && *cmd != '\t' && n < (int)sizeof(word) - 1)
        word[n++] = (char)tolower((unsigned char)*cmd++);
    word[n] = '\0';

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        if (!strcmp(word, bad[i]))
            return 1;
    }
    return 0;
}

static void term_api(int fd, const char *query)
{
    char raw[TERM_CMD_MAX * 3 + 1];
    char cmd[TERM_CMD_MAX];
    int  force;
    int  n;

    if (!osd_web_query_get_str_raw(query, "cmd", raw, sizeof(raw)))
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "err=no_cmd\n");
        return;
    }
    if (strlen(raw) >= sizeof(raw) - 1)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "err=too_long\n");
        return;
    }

    n = url_decode(raw, cmd, sizeof(cmd));
    if (n <= 0)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8",
                     n < 0 ? "err=bad_cmd\n" : "err=empty\n");
        return;
    }

    if (!g_inited || g_mq == RT_NULL)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "err=no_term\n");
        return;
    }

    force = osd_web_query_get_int(query, "force", 0);
    if (term_is_dangerous(cmd) && !force)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "confirm=1\n");
        return;
    }

    if (g_busy)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "busy=1\n");
        return;
    }

    /* 先置忙再入队，避免工作线程执行太快、抢在置忙之前清掉 */
    g_busy = 1;
    if (rt_mq_send(g_mq, cmd, (rt_size_t)n + 1) != RT_EOK)
    {
        g_busy = 0;
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "busy=1\n");
        return;
    }

    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=1\n");
}

static void term_out_api(int fd, const char *query)
{
    static char        chunk[TERM_HTTP_CHUNK];
    char               hdr[128];
    unsigned int       since, next;
    int                since_i, lost, n;

    since_i = osd_web_query_get_int(query, "since", -1);
    since   = (since_i < 0) ? g_out_total : (unsigned int)since_i;

    n = term_out_read(since, chunk, sizeof(chunk), &next, &lost);

    snprintf(hdr, sizeof(hdr),
             "X-Term-Since: %u\r\nX-Term-Lost: %d\r\nX-Term-Busy: %d",
             next, lost, g_busy);
    osd_web_send_bin_ex(fd, "200 OK", "text/plain; charset=utf-8",
                        chunk, n, hdr);
}

int osd_web_term_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/term"))
    {
        term_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/term/out"))
    {
        term_out_api(fd, query);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 初始化                                                             */
/* ------------------------------------------------------------------ */

int osd_web_term_init(void)
{
    if (g_inited)
        return 0;

    rt_memset(&g_dev, 0, sizeof(g_dev));
    g_dev.type    = RT_Device_Class_Char;
    g_dev.init    = term_dev_init;
    g_dev.open    = term_dev_open;
    g_dev.close   = term_dev_close;
    g_dev.read    = term_dev_read;
    g_dev.write   = term_dev_write;
    g_dev.control = term_dev_control;

    if (rt_device_register(&g_dev, "webterm",
                           RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STREAM) != RT_EOK)
    {
        rt_kprintf("[webterm] register failed\n");
        return -1;
    }
    g_mq = rt_mq_create("wterm", TERM_CMD_MAX, 4, RT_IPC_FLAG_FIFO);
    if (g_mq == RT_NULL)
    {
        rt_kprintf("[webterm] mq create failed\n");
        return -1;
    }

    g_thread = rt_thread_create("webterm", term_worker, RT_NULL,
                                TERM_WORKER_STACK, 25, 10);
    if (g_thread == RT_NULL)
    {
        rt_kprintf("[webterm] thread create failed\n");
        return -1;
    }
    rt_thread_startup(g_thread);

    rt_console_set_output_hook(term_console_hook);

    g_inited = 1;
    rt_kprintf("[webterm] ready (web msh console)\n");
    return 0;
}
