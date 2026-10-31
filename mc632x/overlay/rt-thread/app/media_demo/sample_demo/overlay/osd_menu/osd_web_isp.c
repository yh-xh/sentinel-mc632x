/*
 * osd_web_isp.c —— ISP 控制面板网页
 *
 * 为什么要另起一套，而不是驱动屏上那个 OSD 菜单：
 *   osd_menu/mod/callback/ 下 7 个菜单回调文件全是自动生成的空壳，
 *   menu_X_register() 的注册代码整段被注释、handler 函数体也是空的。
 *   那个菜单改任何一项都只会改自己的控件状态然后重画，不碰 ISP。
 *   所以"控制 ISP"这条路绕开菜单更直接。
 *
 * 页面由固件端从 g_ctl[] 生成（不是把控件表在 JS 里再抄一份），
 * 这样增删控件只需改一个地方。生成物放在静态缓冲 g_page 里 ——
 * 服务线程栈只有 8K，不能用栈上的大缓冲。
 *
 * 本文件挂在 osd_web_key.c 的 8080 服务器上，不另起线程：
 * 端口已被它 bind，且 osd_menu 核心无锁、必须单线程。
 *
 * 标签直接用 UTF-8 字面量。OSD 菜单那套标签是 GB2312，但那是给屏上
 * 字库用的；本模块绕开菜单，浏览器按 UTF-8 收，零转码。
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <pthread.h>

#include "config.h"
#include "osd_web_key.h"
#include "osd_web_isp.h"

/* 这两个头文件现在就 include 进来（虽然本轮还没调用），
 * 趁改动还小时先证明它们在这个编译单元里能解析。
 * isp_sensor_if.h 必须在前面：FHAdv_Isp_mpi_v3.h 里
 * FHAdv_Isp_SensorInit() 用到了 struct isp_sensor_if，而 -Werror 下
 * "declared inside parameter list" 是硬错误。*/
#include "isp/isp_sensor_if.h"
#include "isp/isp_common.h"
#include "isp/isp_api.h"
#include "dsp_ext/FHAdv_Isp_mpi_v3.h"

#define ISP_WEB_PAGE_MAX    (14500)   /* TCP_WND 是 14600，整页要能一次塞进窗口 */
#define ISP_WEB_SNAP_MAX    (256)

/* ------------------------------------------------------------------ */
/* 控件表 —— 页面和控制逻辑的唯一真相来源                              */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *key;    /* 线上键名 */
    const char *name;   /* 中文标签 */
    int         min;    /* 闭区间下限 */
    int         max;    /* 闭区间上限 */
    int         def;    /* 出厂值 */
    int         value;  /* 当前值 */
    const char *opts;   /* 枚举选项，'|' 分隔；连续量为 NULL */
} isp_web_ctl_t;

/*
 * br/ct/sat/sp/ev 以 0 为中心：对齐 SDK "128 表示采用默认值" 的语义，
 * 软件层面 0 就是"不动它"，滑块起点居中。
 * rg/bg 0..100 对齐 script/in/awb.ini。
 * ir 单列一键，因为厂商菜单里 [infraRed] 是独立于 [dayNightMode] 的另一个控件。
 */
static isp_web_ctl_t g_ctl[] = {
/*   key    名称        min   max  def  value  枚举选项 */
    { "br",  "亮度",    -100, 100,   0,   0,  NULL },
    { "ct",  "对比度",  -100, 100,   0,   0,  NULL },
    { "sat", "饱和度",  -100, 100,   0,   0,  NULL },
    { "sp",  "锐度",    -100, 100,   0,   0,  NULL },
    { "ev",  "曝光补偿",  -8,   8,   0,   0,  NULL },
    { "dn",  "日夜模式",   0,   1,   0,   0,  "彩色|黑白" },
    { "ir",  "红外",       0,   2,   0,   0,  "智能红外|开|关" },
    { "wb",  "白平衡",     0,   1,   0,   0,  "自动|锁定" },
    { "rg",  "红增益",     0, 100,  50,  50,  NULL },
    { "bg",  "蓝增益",     0, 100,  50,  50,  NULL },
};

#define ISP_WEB_CTL_NUM (sizeof(g_ctl) / sizeof(g_ctl[0]))

static isp_web_ctl_t *ctl_find(const char *key)
{
    unsigned int i;

    for (i = 0; i < ISP_WEB_CTL_NUM; i++)
    {
        if (!strcmp(g_ctl[i].key, key))
            return &g_ctl[i];
    }
    return NULL;
}

/*
 * 待应用请求的标志位，和 g_ctl[] 一一对应。网页线程写，ISP 线程读并清 ——
 * 这是本模块唯一的跨线程数据。两边都只碰对齐的 int（ARM 上天然原子），
 * 每个槽位只有一个写者，所以不需要锁。开机时是 0，即无待处理请求。
 *
 * 只传标志不传值：目标值就在 g_ctl[i].value 里（isp_web_set_one 写），
 * 同样是单写单读。这样连值都不用搬。
 */
static volatile int g_pend_req[MAX_GRP_NUM][ISP_WEB_CTL_NUM];

/* 每路一份当前值（默认值同 g_ctl 的 def 列：br/ct/sat/sp/ev/dn/ir/wb=0，
 * rg/bg=50）。网页快照读这里，pump 按 dev 写这里。*/
static int g_dev_value[MAX_GRP_NUM][ISP_WEB_CTL_NUM] = {
    /* br ct sat sp ev dn ir wb rg bg —— 和 g_ctl 的 def 列一致 */
    { 0, 0, 0, 0, 0, 0, 0, 0, 50, 50 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 50, 50 },
};

/* pump 当前处理的 dev 的值数组；cv()/sv() 是 pump 内的取值/存值助手 */
static int *g_apply_vals;
static pthread_mutex_t g_pump_lock = PTHREAD_MUTEX_INITIALIZER;

/* ==================================================================== */
/*                        ★ ISP SEAM ★                                  */
/* ==================================================================== */
/*
 * 本模块唯一会碰 ISP SDK 的地方。它上面的一切（HTTP 解析、钳位、快照编码、
 * 页面生成）都刻意与 ISP 无关，所以以后再也不用改。
 *
 * 调用者：HTTP 服务线程（osd_web_key 的线程，prio 130，8K 栈），
 *         不是 ISP/app 线程。
 *
 *   key   : g_ctl[].key，调用方已确认合法
 *   value : 调用方已钳到 [min, max]
 *   返回  : 0  = 接受，状态已更新
 *           <0 = 拒绝，状态不变，值被丢弃，但 HTTP 层仍回 200 + 快照
 *
 * 返回值是"策略结果"，永远不是 SDK 错误码 —— 这正是让下面这个函数体
 * 能从"只存值"换成"直接调 SDK"再换成"入队交给 app 线程"而 HTTP 层
 * 完全无感的原因。见下面的 THREADING。
 *
 * --------------------------- 已接入 ---------------------------
 * 十个键里九个可用；ir 保留在表里但页面不展示，直接调用会被拒掉：
 *
 *   br   API_ISP_SetBrightnessCfg      FHAdv_Isp_SetBrightness 头文件明写
 *                                      "V3 不支持"；FHAdv_Isp_SetLtmCfg 声明了
 *                                      但库里没导出（调了会链接失败）。
 *   ct    FHAdv_Isp_SetContrast(dev, 0, 128+v)      mode 0 = 手动
 *   sat   FHAdv_Isp_SetSaturation(dev, 0, 128+v)
 *   sp    FHAdv_Isp_SetSharpeness(dev, 0, 128+v)
 *   ev    API_ISP_Get/SetAeDefaultCfg 改 stAeLumaCfg.u08LumaRef
 *   dn    0=彩色 1=黑白（FHAdv_Isp_SetColorMode）
 *         外部/自动已从页面移除 —— smartIR 没编进来
 *   ir   拒绝，页面也不再展示 —— SDK 里根本没有 IR-cut 接口
 *   wb    FHAdv_Isp_SetAwbGain(dev, mode)  mode 就是 SDK 模式号
 *   rg/bg API_ISP_Get/SetAwbGain，G 通道读回别覆盖
 *
 * 必须先 Get 再改动再 Set 的（br/ev/rg/bg）：那几个结构里既有别人配好的
 * 字段，也有带"更新时需置位 refresh 位"的字段，整体覆盖会出事。
 *
 * ---------------------------- THREADING ----------------------------
 * 本函数只入队，真正的 SDK 调用在 isp_web_pump() 里，由 ISP 线程每轮执行。
 *
 * 为什么不能在这里直接调：全工程 ISP 参数路径上没有任何锁（isp_config.c /
 * sample_common_isp.c 里连一个互斥量都没有），而 ISP 线程每 10ms 就在
 * API_ISP_Run() 里跑策略。网页线程直接调会和它并发，上面那几个 Get-改-Set
 * 尤其会丢更新。
 *
 * 因为返回的是策略结果而不是 SDK 错误码，从"直接调"改成"入队"只动了这一个
 * 函数体加一个 pump，下面的钳位、快照编码、页面生成一行没改。
 */
static int isp_web_apply(int dev, const char *key, int value)
{
    int i;

    if (dev < 0 || dev >= MAX_GRP_NUM)
        dev = FH_APP_GRP_ID;

    for (i = 0; i < (int)ISP_WEB_CTL_NUM; i++)
    {
        if (strcmp(g_ctl[i].key, key))
            continue;

        /* 本层级做不到的，在这里就拒掉 —— 值不会被存，客户端拿到的快照
         * 还是旧值，UI 会把控件弹回去。这比默默收下要好。*/
        /* 值没变就别再打扰 ISP：AE/AWB/亮度每写一次都可能触发一轮收敛，
         * 重复下发同样的参数会让画面像呼吸灯一样忽明忽暗。*/
        if (value == g_dev_value[dev][i])
            return 0;

        if (!strcmp(key, "ir"))
        {
            /* 整个 SDK 里没有 IR-cut / 红外灯的可调用接口。
             * platform/mc632x/app_board/board_def.h 只有两个 GPIO 号
             * (CONFIG_IRCUT_ON_GPIO=0 / CONFIG_IRCUT_OFF_GPIO=1)，
             * 开机时 _ircut_init() 把它们配成输出低就再没人碰过。
             * 双线制大概是自锁驱动，要给脉冲，时序没依据不能乱试。*/
            printf("[isp_web] 'ir' 暂不支持: 本层级没有 IR-cut 接口\n");
            return -1;
        }

        g_dev_value[dev][i] = value;
        g_pend_req[dev][i] = 1;     /* 最后写：这一句才是"请求有效"的标志 */
        return 0;
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/* 真正的 ISP 调用 —— 只跑在 ISP 线程上                                */
/* ------------------------------------------------------------------ */

/*
 * AE 参考亮度的基准值。曝光补偿是相对量，基准必须在第一次用到时从
 * 硬件读回来，不能硬编码（厂商默认值会随 sensor/效果参数变）。
 * 0 表示还没读。只在 ISP 线程上访问，不需要保护。
 */
static int g_ev_base  = 0;   /* G1 */
static int g_ev_base0 = 0;   /* G0（曝光联动，EV 要同步）*/

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static int cv(int idx)
{
    return g_apply_vals[idx];
}

static void sv(int idx, int v)
{
    g_apply_vals[idx] = v;
}

/* 对比度 / 饱和度 / 锐度用的是 FHAdv 那层，约定 128 = 默认，所以
 * 页面的 -100..100 直接加 128 得到 28..228 的手动值。*/
static int isp_do_scalar(int dev, int idx)
{
    int v   = 128 + cv(idx);
    int ret;

    if (!strcmp(g_ctl[idx].key, "ct"))
        ret = FHAdv_Isp_SetContrast(dev, 0 /* 手动 */, (FH_UINT32)v);
    else if (!strcmp(g_ctl[idx].key, "sat"))
        ret = FHAdv_Isp_SetSaturation(dev, 0, (FH_UINT32)v);
    else
        ret = FHAdv_Isp_SetSharpeness(dev, 0, (FH_UINT32)v);

    return ret;
}

/*
 * 亮度走 API_ISP_SetBrightnessCfg —— FHAdv_Isp_SetBrightness 头文件明写
 * "V3 不支持"，而 FHAdv_Isp_SetLtmCfg 虽然声明了但 libadvapi_isp.a 里
 * 根本没导出（nm 查过），调它会链接失败。
 * 必须 Get-改-Set，别整体覆盖：bYcEn / s08BrtMap[] 是别人配好的。
 * s08Brt 范围 [-128,127]，和页面的 -100..100 正好对得上。
 */
static int isp_do_brightness(int dev)
{
    ISP_BRIGHTNESS_CFG cfg;
    int ret;

    memset(&cfg, 0, sizeof(cfg));
    ret = API_ISP_GetBrightnessCfg(dev, &cfg);
    if (ret)
        return ret;

    cfg.bYcEn         = 1;      /* 不使能的话 s08Brt 写了也不生效 */
    cfg.bGainMappingEn = 0;     /* 手动 */
    cfg.s08Brt        = (FH_SINT8)clampi(cv(0), -128, 127);

    return API_ISP_SetBrightnessCfg(dev, &cfg);
}

/*
 * 曝光补偿 = 抬/降 AE 的参考亮度 u08LumaRef（[0,255]），4 个亮度单位 ≈ 1 档。
 * 同样 Get-改-Set：这个结构里好几个字段带"更新时需置位 refresh 位"，
 * 整体覆盖会把别人的配置冲掉。
 */
static int isp_do_ev(int dev)
{
    AE_DEFAULT_CFG cfg;
    int ret;

    memset(&cfg, 0, sizeof(cfg));
    ret = API_ISP_GetAeDefaultCfg(dev, &cfg);
    if (ret)
        return ret;

    if (g_ev_base == 0)
        g_ev_base = cfg.stAeLumaCfg.u08LumaRef;   /* 第一次读回基准 */

    cfg.stAeLumaCfg.u08LumaRef =
        (FH_UINT8)clampi(g_ev_base + cv(4) * 4, 0, 255);

    ret = API_ISP_SetAeDefaultCfg(dev, &cfg);

    /* 这块板子两路 ISP 的曝光是联动的（改一路，两路亮度一起变）。
     * 如果只改 G1 的 AE 目标，G0 的 AE 会把曝光拉回去，两个 AE 互相
     * 拉锯就表现为两边一起呼吸式渐亮渐暗。EV 同步写进 G0，目标一致就
     * 不会打起来。*/
    memset(&cfg, 0, sizeof(cfg));
    if (API_ISP_GetAeDefaultCfg(0, &cfg) == 0)
    {
        if (g_ev_base0 == 0)
            g_ev_base0 = cfg.stAeLumaCfg.u08LumaRef;
        cfg.stAeLumaCfg.u08LumaRef =
            (FH_UINT8)clampi(g_ev_base0 + cv(4) * 4, 0, 255);
        (void)API_ISP_SetAeDefaultCfg(0, &cfg);
    }

    /* EV 是两路共用的（AE 联动，分开设会互拉锯）；UI 两路都显示同一个值 */
    g_dev_value[0][4] = cv(4);
    g_dev_value[1][4] = cv(4);

    return ret;
}

/* 手动增益的基准（第一次进入手动时的硬件增益），两路各一份 */
static int       g_awb_base_valid[MAX_GRP_NUM];
static FH_UINT16 g_awb_base_r[MAX_GRP_NUM];
static FH_UINT16 g_awb_base_b[MAX_GRP_NUM];

/* 白平衡只保留两种模式：
 *   0 自动：AWB 算法持续跟踪；
 *   1 锁定：停住当前增益，交给手动红/蓝增益微调。
 * 白炽灯/暖光灯/自然光/日光灯这类预设需要 sensor 库实现 get_user_awb_gain，
 * cv2005 没有，选了也不生效，所以已经从页面移除。*/
static int isp_do_wb(int dev)
{
    g_awb_base_valid[dev] = 0;
    if (cv(7) == 0)
    {
        sv(8, 50);
        sv(9, 50);
    }
    return FHAdv_Isp_SetAwbGain(dev, (FH_UINT32)cv(7));
}

/* 手动增益是"相对当前自动白平衡"的：50 = 基准不变，0/100 = 基准的 0.5x/1.5x。
 * 基准在第一次进入手动时从硬件读回，所以从 50 挪到 51 只是 1% 的微调，
 * 不会像绝对 1.0x 那样把整幅画面的色温一下推翻。*/
static FH_UINT16 awb_ui_scale(FH_UINT16 base, int v)
{
    unsigned long g;

    v = clampi(v, 0, 100);
    g = (unsigned long)base * (unsigned)(50 + v) / 100;
    if (g < 0x40)
        g = 0x40;
    if (g > 0x1fff)
        g = 0x1fff;
    return (FH_UINT16)g;
}

/*
 * 手动白平衡增益。红/蓝是页面上两个独立滑块，底层一次写三个通道，
 * 所以合并成一次调用，G 通道读回原值别覆盖。
 *
 * 两路都回到 50 时交还自动白平衡；只要有任意一路离开 50，就锁定 AWB
 * 并按"基准 x 滑杆比例"下发，基准取第一次进入手动时的当前增益。
 */
static int isp_do_rgb_gain(int dev)
{
    ISP_AWB_GAIN g;
    int ret;

    memset(&g, 0, sizeof(g));
    ret = API_ISP_GetAwbGain(dev, &g);
    if (ret)
        return ret;

    if (cv(8) == 50 && cv(9) == 50)
    {
        sv(7, 0);
        g_awb_base_valid[dev] = 0;
        FHAdv_Isp_SetAwbGain(dev, 0);
        return 0;
    }

    if (cv(7) == 0 || !g_awb_base_valid[dev])
    {
        if (g.u16Rgain != 0)
            g_awb_base_r[dev] = g.u16Rgain;
        if (g.u16Bgain != 0)
            g_awb_base_b[dev] = g.u16Bgain;
        g_awb_base_valid[dev] = 1;
        sv(7, 1);                       /* UI 同步显示"锁定" */
        FHAdv_Isp_SetAwbGain(dev, 1);
    }

    g.u16Rgain = awb_ui_scale(g_awb_base_r[dev], cv(8));
    g.u16Bgain = awb_ui_scale(g_awb_base_b[dev], cv(9));
    /* u16Ggain 保持读回来的值 */

    return API_ISP_SetAwbGain(dev, &g);
}

/* UI 值：0=彩色 1=黑白。
 * SDK 的 FHAdv_Isp_SetColorMode 参数反过来：0=黑白(饱和度为0) 1=彩色。
 * 只有这两个是"状态"，能直接设；外部/自动要靠 smartIR，未编译。*/
static int isp_do_dn(int dev)
{
    return FHAdv_Isp_SetColorMode(dev, (FH_UINT32)(cv(5) == 1 ? 0 : 1));
}

/*
 * 由 ISP 线程每轮调用一次（见 sample_common_isp.c 的 isp_run）。
 *
 * 为什么必须在这个线程：全工程 ISP 参数路径上没有任何锁，
 * 而 ISP 线程每 10ms 就在 API_ISP_Run() 里跑策略。网页线程直接调
 * FHAdv_Isp_* / API_ISP_* 会和它并发。上面好几个还是 Get-改-Set，
 * 并发下会丢更新。
 *
 * 每个槽位"取走-清标志-再执行"：标志先清，这样这轮执行期间新来的请求
 * 会重新置位，下轮再处理，不会丢。
 */
void isp_web_pump(int devid)
{
    int i;

    if (devid < 0 || devid >= MAX_GRP_NUM)
        return;

    /* 两路 ISP 线程各跑各的，但共享 g_dev_value / AWB 基准；
     * 参数落地这一段必须串行。*/
    pthread_mutex_lock(&g_pump_lock);
    g_apply_vals = g_dev_value[devid];

    for (i = 0; i < (int)ISP_WEB_CTL_NUM; i++)
    {
        int ret;
        int idx = i;

        if (!g_pend_req[devid][i])
            continue;

        g_pend_req[devid][i] = 0;

        if (!strcmp(g_ctl[idx].key, "br"))
            ret = isp_do_brightness(devid);
        else if (!strcmp(g_ctl[idx].key, "ev"))
            ret = isp_do_ev(devid);
        else if (!strcmp(g_ctl[idx].key, "dn"))
            ret = isp_do_dn(devid);
        else if (!strcmp(g_ctl[idx].key, "wb"))
            ret = isp_do_wb(devid);
        else if (!strcmp(g_ctl[idx].key, "rg") || !strcmp(g_ctl[idx].key, "bg"))
            ret = isp_do_rgb_gain(devid);
        else
            ret = isp_do_scalar(devid, idx);

        if (ret)
            printf("[isp_web] dev=%d %s=%d failed 0x%x\n",
                   devid, g_ctl[idx].key, cv(idx), ret);
    }

    pthread_mutex_unlock(&g_pump_lock);
}

/* ------------------------------------------------------------------ */
/* 状态快照                                                            */
/* ------------------------------------------------------------------ */

/*
 * 编成 "br=0&ct=0&sat=0&..." —— 就是板子自己已经在解析的格式
 * （osd_web_query_get_int），一个编码器一个解码器，无转义、无嵌套。
 */
static int isp_web_snapshot(int dev, char *buf, int sz)
{
    unsigned int i;
    int          n = 0;

    if (dev < 0 || dev >= MAX_GRP_NUM)
        dev = FH_APP_GRP_ID;

    for (i = 0; i < ISP_WEB_CTL_NUM; i++)
    {
        int r = snprintf(buf + n, sz - n, "%s%s=%d",
                         i ? "&" : "", g_ctl[i].key, g_dev_value[dev][i]);

        /* snprintf 返回的是"本该写多长"。n 一旦越过 sz，再算 sz-n 就是
         * size_t 下溢，所以必须在这里就截断退出。*/
        if (r < 0 || r >= sz - n)
        {
            buf[sz - 1] = '\0';
            return sz - 1;
        }
        n += r;
    }

    if (n < sz - 1)
    {
        buf[n++] = '\n';    /* curl 友好 */
        buf[n]   = '\0';
    }
    return n;
}

/* 先钳位、再交给缝 —— 这样回给客户端的快照就是真相，UI 靠它把滑块弹回去 */
static void isp_web_set_one(int dev, const char *key, int value)
{
    isp_web_ctl_t *c = ctl_find(key);

    if (!c)
        return;

    if (value < c->min)
        value = c->min;
    if (value > c->max)
        value = c->max;

    (void)isp_web_apply(dev, c->key, value);
}

static void isp_web_api(int fd, const char *query)
{
    char snap[ISP_WEB_SNAP_MAX];
    int  dev = osd_web_query_get_int(query, "dev", FH_APP_GRP_ID);

    if (dev < 0 || dev >= MAX_GRP_NUM)
        dev = FH_APP_GRP_ID;

    if (query && query[0])
    {
        char k[12];

        if (osd_web_query_get_str(query, "k", k, sizeof(k)))
        {
            /* 单写： /api/isp?k=ct&v=40 */
            isp_web_set_one(dev, k, osd_web_query_get_int(query, "v", 0));
        }
        else
        {
            /* 批量： /api/isp?br=0&ct=0&...  每个已知键都试一遍，
             * 未知键自然被忽略。给防抖发送和"恢复默认"用。*/
            unsigned int i;

            for (i = 0; i < ISP_WEB_CTL_NUM; i++)
            {
                int v = osd_web_query_get_int(query, g_ctl[i].key, INT_MIN);

                if (v != INT_MIN)
                    isp_web_set_one(dev, g_ctl[i].key, v);
            }
        }
    }

    isp_web_snapshot(dev, snap, sizeof(snap));
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", snap);
}

/* ------------------------------------------------------------------ */
/* 页面生成                                                            */
/* ------------------------------------------------------------------ */

/* 单线程服务器，无重入，所以静态缓冲是安全的（栈只有 8K，放不下整页） */
static char g_page[ISP_WEB_PAGE_MAX];
static int  g_page_len;
static int  g_page_logged;

static void page_puts(const char *s)
{
    int n = (int)strlen(s);

    if (g_page_len + n >= ISP_WEB_PAGE_MAX)
        n = ISP_WEB_PAGE_MAX - 1 - g_page_len;
    if (n <= 0)
        return;

    memcpy(g_page + g_page_len, s, n);
    g_page_len += n;
    g_page[g_page_len] = '\0';
}

static void page_printf(const char *fmt, ...)
{
    char    tmp[256];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n >= 0)
        page_puts(tmp);
}

static const char ISP_PAGE_HEAD[] =
    "<!DOCTYPE html><html lang=\"zh\"><head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
    "<meta name=\"theme-color\" content=\"#ffe\">"
    "<link rel=\"icon\" href=\"data:,\">"
    "<title>ISP 控制面板</title><style>"
    ":root{--bg:#ffe;--card:#fff;--fg:#222;--dim:#777;--ac:#e6a700}"
    "*{box-sizing:border-box}"
    "body{margin:0;padding:12px 12px calc(12px + env(safe-area-inset-bottom));"
    "background:var(--bg);color:var(--fg);font:16px/1.5 -apple-system,system-ui,sans-serif;"
    "-webkit-text-size-adjust:100%}"
    "h1{font-size:19px;margin:4px 0 12px;display:flex;align-items:center;gap:8px}"
    "h2{font-size:13px;font-weight:600;color:var(--dim);margin:0 0 4px;letter-spacing:.05em}"
    ".card{background:var(--card);border:1px solid #f0e6c8;border-radius:14px;padding:10px 14px 14px;margin:0 0 12px}"
    ".row{display:flex;align-items:center;gap:10px;min-height:52px}"
    ".lb{flex:0 0 4.2em;font-size:15px}"
    ".val{flex:0 0 3em;text-align:right;color:var(--dim);font-variant-numeric:tabular-nums;font-size:15px}"
    "input[type=range]{-webkit-appearance:none;appearance:none;flex:1;height:44px;"
    "background:transparent;margin:0;touch-action:manipulation}"
    "input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:4px;background:#ddd}"
    "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;"
    "margin-top:-10px;border-radius:50%;background:var(--ac);border:0}"
    "input[type=range]::-moz-range-track{height:8px;border-radius:4px;background:#ddd}"
    "input[type=range]::-moz-range-thumb{width:28px;height:28px;border:0;border-radius:50%;background:var(--ac)}"
    ".seg{display:flex;flex-wrap:wrap;gap:6px;padding:2px 0 6px}"
    ".seg button{flex:1 1 4.6em;min-height:44px;border:1px solid #ddd;border-radius:10px;"
    "background:#fff;color:var(--fg);font-size:15px;font-family:inherit;"
    "touch-action:manipulation;-webkit-tap-highlight-color:transparent}"
    ".seg button.on{background:var(--ac);border-color:var(--ac);color:#7a4d00;font-weight:600}"
    ".bar{display:flex;gap:8px}"
    ".bar button{flex:1;min-height:46px;border:1px solid #ddd;border-radius:12px;"
    "background:#fff;color:var(--fg);font-size:15px;font-family:inherit;"
    "touch-action:manipulation;-webkit-tap-highlight-color:transparent}"
    ".pv{display:flex;gap:6px;margin:4px 0 6px}"
    ".pv img{width:50%;background:#000;border-radius:8px;display:block;min-height:60px}"
    "#evs{max-height:9em;overflow:auto}"
    "#pcard{display:none}"
    ".gal{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px;"
    "margin:6px auto 10px;max-width:960px}"
    "#galcard{max-width:1000px;margin-left:auto;margin-right:auto}"
    ".gi{position:relative;background:#000;border-radius:10px;overflow:hidden;border:1px solid #ddd}"
    ".gi:hover{border-color:var(--ac)}"
    ".gi a{display:block;aspect-ratio:5/3}"
    ".gi img{width:100%;height:100%;object-fit:cover;display:block}"
    ".gdel{position:absolute;top:5px;right:5px;width:28px;height:28px;border:0;border-radius:50%;"
    "background:rgba(0,0,0,.65);color:#fff;font-size:17px;line-height:28px;padding:0;cursor:pointer}"
    "#gallist{max-width:960px;max-height:14em;overflow:auto;margin:0 auto}"
    "#gallist div{display:flex;align-items:center;gap:8px;padding:4px 2px;"
    "border-bottom:1px solid #eee}"
    "#gallist a{color:var(--ac);text-decoration:none;flex:1;overflow:hidden;"
    "text-overflow:ellipsis;white-space:nowrap}"
    ".ldel{flex:0 0 auto;border:1px solid #e0b4b0;border-radius:8px;background:#fdecea;"
    "color:#c0392b;font-size:12px;padding:3px 8px;cursor:pointer}"
    ".note{color:var(--dim);font-size:13px;margin:2px 0 10px}"
    "#st{text-align:center;color:var(--dim);font-size:14px;min-height:1.5em;margin-top:4px}"
    "</style></head><body>"
    "<h1>ISP 控制面板</h1>"
    "<p class=\"note\"><a href=\"/\" style=\"color:var(--ac)\">&larr; 返回 SENTINEL 看板</a></p>"
    "<section class=\"card\"><h2>屏幕画面</h2>"
    "<div class=\"seg\" id=\"camseg\">"
    "<button type=\"button\" data-v=\"0\" class=\"on\">摄像头 1</button>"
    "<button type=\"button\" data-v=\"1\">摄像头 2</button>"
    "</div>"
    "<p class=\"note\" id=\"camnote\">摄像头 1：带人形框 / 绊线</p>"
    "<div class=\"bar\"><button type=\"button\" id=\"armbtn\">布防</button></div>"
    "</section>"
    "<section class=\"card\" id=\"pcard\"><h2>实时画面</h2>"
    "<div class=\"pv\"><img id=\"pv0\" alt=\"G0 clean\"><img id=\"pv1\" alt=\"G1 AI\"></div>"
    "<p class=\"note\">左：G0 原始（:1111） 右：G1 AI 叠加（:1112）</p>"
    "<div class=\"bar\">"
    "<button type=\"button\" id=\"snapai\">抓拍并保存</button>"
    "<button type=\"button\" id=\"ovlbtn\">隐藏叠加</button>"
    "</div>"
    "<div id=\"snaptxt\" class=\"note\"></div>"
    "<div id=\"evs\" class=\"note\"></div>"
    "</section>"
    "<section class=\"card\" id=\"galcard\"><h2>SD 卡相册</h2>"
    "<div class=\"bar\"><button type=\"button\" id=\"galrf\">刷新相册</button>"
    "<button type=\"button\" id=\"autobtn\">自动抓拍：关</button></div>"
    "<div class=\"bar\">"
    "<button type=\"button\" id=\"galclean\">清理</button>"
    "<button type=\"button\" id=\"galspace\">容量</button>"
    "<button type=\"button\" id=\"galmount\">重挂 SD</button></div>"
    "<div id=\"gal\" class=\"gal\"></div>"
    "<div id=\"gallist\" class=\"note\"></div>"
    "<div id=\"galtxt\" class=\"note\"></div>"
    "</section>";

/* 从 g_ctl[] 生成控件。改控件只改上面那张表。 */
static void page_build_controls(void)
{
    static const char *GROUP[] = { "图像", "曝光", "日夜", "白平衡" };
    /* 每个分组对应 g_ctl[] 里的下标区间 [start, end)。
     * 注意：下标 6 的 "ir" 故意不渲染 —— 这块板子的 IR-cut 是机械滤光片，
     * SDK 也没有可调用的 IR-cut 接口，页面上给了按钮也只会被 isp_web_apply()
     * 拒绝并弹回。保留在 g_ctl[] 里只是为了让后面的 wb/rg/bg 下标不变。*/
    static const int   START[] = { 0, 4, 5, 7 };
    static const int   END[]   = { 4, 5, 6, 10 };
    unsigned int       g;

    for (g = 0; g < sizeof(START) / sizeof(START[0]); g++)
    {
        int i;

        page_printf("<section class=\"card\"><h2>%s</h2>", GROUP[g]);

        for (i = START[g]; i < END[g] && i < (int)ISP_WEB_CTL_NUM; i++)
        {
            isp_web_ctl_t *c = &g_ctl[i];

            if (c->opts)
            {
                const char *p = c->opts;
                int         opt = 0;

                page_printf("<div class=\"row\"><span class=\"lb\">%s</span></div>"
                            "<div class=\"seg\" data-k=\"%s\" data-d=\"%d\">",
                            c->name, c->key, c->def);

                while (*p)
                {
                    const char *bar  = strchr(p, '|');
                    int         nlen = bar ? (int)(bar - p) : (int)strlen(p);

                    page_printf("<button type=\"button\" class=\"%s\" data-v=\"%d\">%.*s</button>",
                                (opt == c->value) ? "on" : "", opt, nlen, p);

                    if (!bar)
                        break;
                    p   = bar + 1;
                    opt++;
                }
                page_puts("</div>");
            }
            else
            {
                page_printf("<div class=\"row\"><span class=\"lb\">%s</span>"
                            "<input type=\"range\" data-k=\"%s\" data-d=\"%d\" "
                            "min=\"%d\" max=\"%d\" step=\"1\" value=\"%d\">"
                            "<span class=\"val\" id=\"v_%s\">%d</span></div>",
                            c->name, c->key, c->def,
                            c->min, c->max, c->value,
                            c->key, c->value);
            }
        }

        page_puts("</section>");
    }
}

static const char ISP_PAGE_TAIL[] =
    "<section class=\"card\"><div class=\"bar\">"
    "<button type=\"button\" id=\"rf\">刷新</button>"
    "<button type=\"button\" id=\"rs\">恢复默认</button>"
    "</div></section>"
    "<div id=\"st\">载入中…</div>"
    "<script>"
    "var S={},P={},fl=false,tm=null;"
    "function st(s){document.getElementById('st').textContent=s}"
    "function parse(t){var o={};t.trim().split('&').forEach(function(kv){"
    "var p=kv.split('=');if(p.length>1)o[p[0]]=+p[1]});return o}"
    "function render(k){var v=S[k];"
    "var r=document.querySelector('input[data-k=\"'+k+'\"]');"
    "if(r){r.value=v;var e=document.getElementById('v_'+k);if(e)e.textContent=v;return}"
    "var s=document.querySelector('.seg[data-k=\"'+k+'\"]');"
    "if(s){var b=s.children;for(var i=0;i<b.length;i++)"
    "b[i].className=(+b[i].getAttribute('data-v')===v)?'on':''}}"
    "function draw(){for(var k in S)render(k)}"
    "function set(k,v){S[k]=v;P[k]=v;render(k);clearTimeout(tm);tm=setTimeout(flush,200)}"
    "function flush(){"
    "if(fl)return;var q=[];for(var k in P)q.push(k+'='+P[k]);"
    "if(!q.length)return;P={};fl=true;"
    "fetch('/api/isp?'+q.join('&')).then(function(r){return r.text()})"
    ".then(function(t){apply(parse(t));st('已保存')})"
    ".catch(function(e){st('错误 '+e)})"
    ".then(function(){fl=false;for(var k in P){flush();return}})}"
    "function apply(o){for(var k in o){"
    "if(Object.prototype.hasOwnProperty.call(P,k))continue;"   /* 用户正在拖的不回写 */
    "S[k]=o[k];render(k)}}"
    "function load(){fetch('/api/isp').then(function(r){return r.text()})"
    ".then(function(t){S=parse(t);draw();st('就绪')})"
    ".catch(function(e){st('错误 '+e)})}"
    "document.addEventListener('input',function(e){"
    "var t=e.target;if(t.type==='range')set(t.getAttribute('data-k'),+t.value)});"
    "document.addEventListener('change',function(e){"
    "if(e.target.type==='range')flush()});"
    "function camPaint(m){var b=document.querySelectorAll('#camseg button');"
    "for(var i=0;i<b.length;i++)b[i].className=(+b[i].getAttribute('data-v')===m)?'on':'';"
    "document.getElementById('camnote').textContent="
    "(m===0)?'摄像头 1：带人形框 / 绊线':'摄像头 2：原始画面';}"
    "function camLoad(){fetch('/api/cam').then(function(r){return r.text()})"
    ".then(function(t){var m=parseInt(t.split('=')[1],10);if(m===0||m===1)camPaint(m)})"
    ".catch(function(){})}"
    "document.addEventListener('click',function(e){"
    "var b=e.target.closest?e.target.closest('.seg button'):null;if(!b)return;"
    "var k=b.parentNode.getAttribute('data-k');"
    "if(k){set(k,+b.getAttribute('data-v'));return}"
    "if(b.parentNode.id!=='camseg')return;"
    "var m=+b.getAttribute('data-v');camPaint(m);"
    "fetch('/api/cam?m='+m).then(function(r){return r.text()})"
    ".then(function(t){var x=parseInt(t.split('=')[1],10);if(x===0||x===1)camPaint(x)})"
    ".catch(function(){})});"
    "document.getElementById('rf').onclick=load;"
    "document.getElementById('rs').onclick=function(){var q=[];"
    "var els=document.querySelectorAll('[data-d]');"
    "for(var i=0;i<els.length;i++)q.push(els[i].getAttribute('data-k')+'='+els[i].getAttribute('data-d'));"
    "fetch('/api/isp?'+q.join('&')).then(function(r){return r.text()})"
    ".then(function(t){S=parse(t);draw();st('已恢复默认')})};"
    "function snapSave(){fetch('/api/snap?save=1&cam=1').then(function(r){return r.text()})"
    ".then(function(t){document.getElementById('snaptxt').textContent=t;evLoad();galLoad()})"
    ".catch(function(e){document.getElementById('snaptxt').textContent='抓拍失败 '+e})}"
    "function ovlPaint(v){document.getElementById('ovlbtn').textContent=v?'隐藏叠加':'显示叠加'}"
    "function ovlLoad(){fetch('/api/overlay').then(function(r){return r.text()})"
    ".then(function(t){ovlPaint(parseInt(t.split('=')[1],10)===1)})}"
    "function ovlToggle(){var off=document.getElementById('ovlbtn').textContent.indexOf('隐藏')>=0;"
    "fetch('/api/overlay?on='+(off?0:1)).then(function(r){return r.text()})"
    ".then(function(t){ovlPaint(parseInt(t.split('=')[1],10)===1)})}"
    "function autoPaint(v){document.getElementById('autobtn').textContent='自动抓拍：'+(v?'开':'关')}"
    "function autoLoad(){fetch('/api/auto').then(function(r){return r.text()})"
    ".then(function(t){autoPaint(parseInt(t.split('=')[1],10)===1)})}"
    "function autoToggle(){var on=document.getElementById('autobtn').textContent.indexOf('开')<0;"
    "fetch('/api/auto?on='+(on?1:0)).then(function(r){return r.text()})"
    ".then(function(t){autoPaint(parseInt(t.split('=')[1],10)===1)})}"
    "function evLoad(){fetch('/api/events?n=5').then(function(r){return r.text()})"
    ".then(function(t){var L=t.trim().split('\\n'),h='';for(var i=1;i<L.length;i++){"
    "var m=L[i].match(/seq=(\\d+)/);var s=m?m[1]:'0';var ok=/ok=1/.test(L[i]);"
    "var c=L[i].match(/clean=([^ ]+)/);var cf=c?c[1]:'';"
    "h+='<div>'+(ok?'<a href=\"/api/event.jpg?id='+s+'\" target=\"_blank\">图</a> ':'')"
    "+(cf?'<a href=\"/api/file?name='+encodeURIComponent(cf)+'\" target=\"_blank\">原图</a> ':'')"
    "+L[i]+'</div>'}"
    "document.getElementById('evs').innerHTML=h})"
    ".catch(function(){})}"
    "function galLoad(){fetch('/api/files?n=256').then(function(r){return r.text()})"
    ".then(function(t){var L=t.trim().split('\\n'),h='',l='',c=0,sd=0;"
    "for(var i=0;i<L.length;i++){"
    "if(L[i].indexOf('sd=')===0){sd=parseInt(L[i].split(' ')[0].split('=')[1],10);continue}"
    "var p=L[i].split(' ');if(p.length<2)continue;var n=p[0];"
    "var u='/api/file?name='+encodeURIComponent(n);"
    "if(c<12)h+='<div class=\"gi\"><a href=\"'+u+'\" target=\"_blank\">"
    "<img loading=\"lazy\" src=\"'+u+'\"></a>"
    "<button type=\"button\" class=\"gdel\" data-n=\"'+n+'\">&times;</button></div>';"
    "l+='<div><a href=\"'+u+'\" target=\"_blank\">'+n+'</a> '+((+p[1]/1024)|0)+' KB"
    "<button type=\"button\" class=\"ldel\" data-n=\"'+n+'\">删除</button></div>';c++}"
    "document.getElementById('gal').innerHTML=h;"
    "document.getElementById('gallist').innerHTML=l;"
    "document.getElementById('galtxt').textContent="
    "sd?(c?'共 '+c+' 个文件':'还没有照片'):'SD 卡未就绪'})"
    ".catch(function(){document.getElementById('galtxt').textContent='读取失败'})}"
    "document.addEventListener('click',function(e){"
    "var b=e.target.closest?e.target.closest('.gdel,.ldel'):null;if(!b)return;"
    "e.preventDefault();var n=b.getAttribute('data-n');"
    "if(!confirm('删除 '+n+' ？'))return;"
    "fetch('/api/delete?name='+encodeURIComponent(n)).then(function(r){return r.text()})"
    ".then(function(){galLoad();evLoad()})"
    ".catch(function(){document.getElementById('galtxt').textContent='删除失败'})});"
    "function timeSync(){var d=new Date();"
    "fetch('/api/time?t='+Math.floor(d.getTime()/1000)+'&tz='+(-d.getTimezoneOffset()))}"
    "document.getElementById('galclean').onclick=function(){"
    "if(!confirm('只保留最近 100 个？'))return;"
    "fetch('/api/clean?keep=100').then(function(r){return r.text()})"
    ".then(function(){galLoad();evLoad()})"
    ".catch(function(){document.getElementById('galtxt').textContent='清理失败'})};"
    "function spaceLoad(){fetch('/api/space').then(function(r){return r.text()})"
    ".then(function(t){var f=t.match(/freekb=(\\d+)/);"
    "document.getElementById('galtxt').textContent=f?"
    "('剩余 '+((+f[1]/1024)|0)+' MB'):'查询失败'})"
    ".catch(function(){document.getElementById('galtxt').textContent='查询失败'})}"
    "document.getElementById('galspace').onclick=spaceLoad;"
    "document.getElementById('galmount').onclick=function(){"
    "if(!confirm('重挂 SD？'))return;"
    "fetch('/api/sd/remount?force=1').then(function(r){return r.text()})"
    ".then(function(){galLoad();evLoad()})"
    ".catch(function(){document.getElementById('galtxt').textContent='重挂失败'})};"
    "document.getElementById('galrf').onclick=galLoad;"
    "document.getElementById('autobtn').onclick=autoToggle;"
    "document.getElementById('snapai').onclick=snapSave;"
    "document.getElementById('ovlbtn').onclick=ovlToggle;"
    "ovlLoad();autoLoad();evLoad();setInterval(evLoad,3000);galLoad();timeSync();"
    "camLoad();"
    "load();"
    "</script><script src=\"/isp-arm.js\"></script></body></html>";

/*
 * 布防/撤防按钮的脚本单独下发，避免 /isp 页面超过 ISP_WEB_PAGE_MAX。
 * 页面里只多一个按钮和一行 <script src>，JS 走 /isp-arm.js。
 */
static const char ISP_ARM_JS[] =
    "function armPaint(v){var b=document.getElementById('armbtn');"
    "if(!b)return;b.textContent=v?'撤防':'布防';"
    "b.style.background=v?'#d9534f':'';b.style.color=v?'#fff':'';"
    "b.style.borderColor=v?'#d9534f':'';}"
    "function armLoad(){fetch('/api/arm').then(function(r){return r.text()})"
    ".then(function(t){var m=t.match(/armed=(\\d+)/);if(m)armPaint(+m[1]===1)})"
    ".catch(function(){})}"
    "function armToggle(){var b=document.getElementById('armbtn');"
    "var on=b.textContent==='布防';"
    "fetch('/api/arm?on='+(on?1:0)).then(function(r){return r.text()})"
    ".then(function(t){var m=t.match(/armed=(\\d+)/);if(m)armPaint(+m[1]===1);"
    "if(typeof ovlLoad==='function')ovlLoad();"
    "if(typeof autoLoad==='function')autoLoad();"
    "if(typeof evLoad==='function')evLoad();})"
    ".catch(function(){b.textContent='切换失败'})}"
    "document.getElementById('armbtn').onclick=armToggle;"
    "armLoad();setInterval(armLoad,3000);";

/* /isp 已经改成跳转到 /stats 的合并面板；页面生成函数保留下来备查，
 * 但要标 unused，否则 -Werror 下会报未使用。*/
__attribute__((unused)) static const char *isp_web_page(void)
{
    g_page_len = 0;
    g_page[0]  = '\0';

    page_puts(ISP_PAGE_HEAD);
    page_build_controls();
    page_puts(ISP_PAGE_TAIL);

    if (!g_page_logged)
    {
        g_page_logged = 1;
        printf("[osd_web] page %d bytes (max %d)\n", g_page_len, ISP_WEB_PAGE_MAX);
    }

    return g_page;
}

/* ------------------------------------------------------------------ */
/* 路由                                                                */
/* ------------------------------------------------------------------ */

int osd_web_isp_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/isp") || !strcmp(path, "/isp.html"))
    {
        /* 参数面板已经合并到 /stats 的"图像参数"页（同一套 g_ctl），
         * 旧地址直接跳过去，避免两套完全一样的控件各调一份。*/
        static const char moved[] = "moved to /stats\n";

        osd_web_send_bin_ex(fd, "302 Found", "text/plain; charset=utf-8",
                            moved, (int)sizeof(moved) - 1, "Location: /stats");
        return 1;
    }

    if (!strcmp(path, "/isp-arm.js"))
    {
        osd_web_send(fd, "200 OK", "application/javascript; charset=utf-8",
                     ISP_ARM_JS);
        return 1;
    }

    if (!strcmp(path, "/api/isp"))
    {
        isp_web_api(fd, query);
        return 1;
    }

    return 0;
}
