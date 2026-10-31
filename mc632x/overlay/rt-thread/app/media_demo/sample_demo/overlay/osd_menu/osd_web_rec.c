/*
 * osd_web_rec.c —— 手动/定时录像控制 + 人流量报告页
 *
 *   /api/rec            查询状态；?on=1&cam=0|1 开始，?on=0 停止
 *   /api/rec/sched      定时设置；?enabled=0 取消
 *   /api/reports        报告列表
 *   /api/report         详情 / ?play=1 让小智播报 / ?ack=1 清待播报
 *   /api/report/pending 小智轮询
 *   /report             报告页面
 */
#include <stdio.h>
#include <string.h>

#include "osd_web_key.h"
#include "osd_web_rec.h"
#include "stream/include/rec_store.h"

static char          g_rec_text[768];
static rec_report_t  g_report_list[REC_REPORT_MAX];

static const char REC_HTML[] =
    "<!doctype html>\n"
    "<html lang=\"zh\"><head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<meta name=\"theme-color\" content=\"#fdfaf1\">\n"
    "<title>SENTINEL · 录制报告</title>\n"
    "<script>try{if(localStorage.getItem('sentinel.theme')==='dark'){document.documentElement.setAttribute('data-theme','dark');}}catch(e){}</script>\n"
    "<link rel=\"icon\" href=\"data:,\">\n"
    "<style>\n"
    ":root{--bg:#f7f1e4;--card:#fff;--ink:#2b2418;--dim:#7a6a4f;--line:#e8ddc6;--ac:#e6a700}\n"
    "html[data-theme=dark]{--bg:#0c0f13;--card:#151a21;--ink:#f0eadc;--dim:#9a9284;--line:#2a323e;--ac:#c7b6ff}\n"
    "*{box-sizing:border-box}\n"
    "body{margin:0;padding:16px;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,sans-serif}\n"
    ".wrap{max-width:760px;margin:0 auto}\n"
    ".top{display:flex;align-items:center;gap:10px;margin-bottom:12px}\n"
    ".top a{color:var(--ac);text-decoration:none;font-weight:600}\n"
    "h1{font-size:20px;margin:0 0 2px}\n"
    ".sub{color:var(--dim);font-size:12px;margin:0 0 14px}\n"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px;margin-bottom:12px}\n"
    ".row{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px dashed var(--line);font-size:14px}\n"
    ".row:last-child{border:0}\n"
    ".row b{font-variant-numeric:tabular-nums}\n"
    ".kpis{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:14px 0 4px}\n"
    ".kpi{background:linear-gradient(160deg,#fffdf7,#fff);border:1px solid var(--line);border-radius:14px;padding:12px;text-align:center}\n"
    "html[data-theme=dark] .kpi{background:#191f29}\n"
    ".kpi span{display:block;font-size:11px;color:var(--dim);letter-spacing:.08em}\n"
    ".kpi b{display:block;font-size:26px;color:var(--ac);font-variant-numeric:tabular-nums;margin-top:2px}\n"
    ".btns{display:flex;gap:9px;flex-wrap:wrap;margin-top:14px}\n"
    "button,.btn{font:inherit;border:1px solid var(--line);border-radius:11px;background:var(--card);color:var(--ink);padding:9px 14px;cursor:pointer;text-decoration:none;font-weight:600}\n"
    "button:hover,.btn:hover{border-color:var(--ac)}\n"
    ".primary{background:var(--ac);border-color:var(--ac);color:#fff}\n"
    "html[data-theme=dark] .primary{color:#1a1426}\n"
    ".msg{font-size:12px;color:var(--dim);margin-top:10px;min-height:1.2em}\n"
    ".recent{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}\n"
    ".recent a{font-size:12px;padding:6px 10px;border-radius:999px;border:1px solid var(--line);color:var(--dim);text-decoration:none}\n"
    ".recent a.on{border-color:var(--ac);color:var(--ink);font-weight:700}\n"
    "</style></head><body>\n"
    "<div class=\"wrap\">\n"
    "<div class=\"top\"><a href=\"/\">← 返回看板</a><span style=\"flex:1\"></span><a href=\"/stats\">图像参数</a></div>\n"
    "<h1>录制报告</h1>\n"
    "<div class=\"sub\" id=\"sub\">加载中…</div>\n"
    "<div class=\"card\" id=\"box\" style=\"display:none\">\n"
    "  <div class=\"row\"><span>开始</span><b id=\"t0\">--</b></div>\n"
    "  <div class=\"row\"><span>结束</span><b id=\"t1\">--</b></div>\n"
    "  <div class=\"row\"><span>摄像头</span><b id=\"cam\">--</b></div>\n"
    "  <div class=\"row\"><span>录像文件</span><b id=\"file\">--</b></div>\n"
    "  <div class=\"row\"><span>帧数 / 大小</span><b id=\"meta\">--</b></div>\n"
    "  <div class=\"kpis\">\n"
    "    <div class=\"kpi\"><span>进入</span><b id=\"kin\">--</b></div>\n"
    "    <div class=\"kpi\"><span>离开</span><b id=\"kout\">--</b></div>\n"
    "    <div class=\"kpi\"><span>画面最高</span><b id=\"kpeak\">--</b></div>\n"
    "  </div>\n"
    "  <div class=\"btns\">\n"
    "    <a class=\"btn\" id=\"play\" href=\"#\" target=\"_blank\" rel=\"opener\">播放录像</a>\n"
    "    <a class=\"btn\" id=\"dl\" href=\"#\" download>下载 AVI</a>\n"
    "    <button class=\"primary\" id=\"voice\">让小智播报</button>\n"
    "  </div>\n"
    "  <div class=\"msg\" id=\"msg\"></div>\n"
    "</div>\n"
    "<div class=\"card\"><div class=\"sub\" style=\"margin:0 0 6px\">最近报告</div><div class=\"recent\" id=\"recent\"></div></div>\n"
    "</div>\n"
    "<script>\n"
    "var $=function(id){return document.getElementById(id)};\n"
    "function kv(t){var o={};(t||'').split('&').forEach(function(x){var i=x.indexOf('=');if(i>0)o[x.slice(0,i)]=x.slice(i+1)});return o}\n"
    "var q=new URLSearchParams(location.search);var rid=q.get('id')||'';\n"
    "function camName(c){return c==='0'?'左摄（摄像头1）':'右摄（摄像头2/AI）'}\n"
    "function load(){\n"
    "  fetch('/api/report'+(rid?'?id='+encodeURIComponent(rid):''),{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){\n"
    "    var o=kv(t.trim());\n"
    "    if(o.ok!=='1'){$('sub').textContent='没有找到报告';return;}\n"
    "    rid=o.id; $('box').style.display='';\n"
    "    $('sub').textContent='报告 #'+o.id;\n"
    "    $('t0').textContent=o.t_start||'--'; $('t1').textContent=o.t_end||'--';\n"
    "    $('cam').textContent=camName(o.cam);\n"
    "    $('file').textContent=(o.file||'').split('/').pop();\n"
    "    $('meta').textContent=(o.frames||'0')+' 帧 · '+(o.bytes?Math.round(+o.bytes/1024)+' KB':'--');\n"
    "    $('kin').textContent=o.in||'0'; $('kout').textContent=o.out||'0'; $('kpeak').textContent=o.peak||'0';\n"
    "    var f=o.file||''; var u='/clip?name='+encodeURIComponent(f);\n"
    "    $('play').href=u; $('dl').href='/api/clip?name='+encodeURIComponent(f);\n"
    "    $('voice').onclick=function(){\n"
    "      $('msg').textContent='已通知小智…';\n"
    "      fetch('/api/report?play=1&id='+o.id,{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){\n"
    "        $('msg').textContent=(t.indexOf('ok=1')>=0)?'已发送给小智，稍后会播报':('发送失败：'+t.trim());\n"
    "      }).catch(function(){$('msg').textContent='发送失败'});\n"
    "    };\n"
    "  }).catch(function(){$('sub').textContent='板子没有响应';});\n"
    "  fetch('/api/reports',{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){\n"
    "    var lines=(t||'').trim().split('\\n');var h='';\n"
    "    for(var i=1;i<lines.length;i++){var o=kv(lines[i].trim().replace(/ /g,'&'));if(!o.id)continue;\n"
    "      h+='<a href=\"/report?id='+o.id+'\" class=\"'+(o.id===rid?'on':'')+'\">#'+o.id+' · '+o.in+'进/'+o.out+'出</a>';}\n"
    "    $('recent').innerHTML=h||'<span class=\"sub\">暂无报告</span>';\n"
    "  }).catch(function(){});\n"
    "}\n"
    "load();\n"
    "</script>\n"
    "</body></html>\n";

static void rec_api(int fd, const char *query)
{
    int  on   = osd_web_query_get_int(query, "on", -1);
    int  cam  = osd_web_query_get_int(query, "cam", 1);
    int  mins = osd_web_query_get_int(query, "mins", 0);
    int  sec  = osd_web_query_get_int(query, "sec", -1);
    int  dur  = 0;
    int  ret  = 0;
    int  off;
    const char *err = "";

    /* 新看板用 sec=30 的快捷录制；老客户端仍可用 mins（上限 3 分钟）。
     * 两个都没给 = 0，表示不限时、手动停。*/
    if (sec >= 0)
    {
        if (sec > 180)
        {
            sec = 180;
        }
        dur = sec;
    }
    else
    {
        if (mins < 0)
        {
            mins = 0;
        }
        if (mins > 3)
        {
            mins = 3;
        }
        dur = mins * 60;
    }

    if (on == 1)
    {
        ret = rec_store_manual_start(cam == 0 ? 0 : 1, dur);
        if (ret == -1) err = "busy";
        else if (ret == -2) err = "sd_not_ready";
        else if (ret != 0) err = "open_failed";
    }
    else if (on == 0)
    {
        ret = rec_store_manual_stop();
        if (ret != 0) err = "idle";
    }

    off = snprintf(g_rec_text, sizeof(g_rec_text), "ok=%d err=%s\n",
                   ret == 0 ? 1 : 0, err);
    if (off < 0) off = 0;
    (void)rec_status_format(g_rec_text + off, (int)sizeof(g_rec_text) - off);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_rec_text);
}

static void rec_sched_api(int fd, const char *query)
{
    int enabled = osd_web_query_get_int(query, "enabled", -1);
    int ret = 0;
    int off;

    if (enabled == 0)
    {
        ret = rec_sched_clear();
    }
    else if (enabled == 1)
    {
        int daily = osd_web_query_get_int(query, "daily", 0);
        int date  = osd_web_query_get_int(query, "date", 0);
        int start = osd_web_query_get_int(query, "start", -1);
        int end   = osd_web_query_get_int(query, "end", -1);
        int cam   = osd_web_query_get_int(query, "cam", 1);

        ret = rec_sched_set(daily, date, start, end, cam);
    }

    off = snprintf(g_rec_text, sizeof(g_rec_text), "ok=%d\n", ret == 0 ? 1 : 0);
    if (off < 0) off = 0;
    (void)rec_status_format(g_rec_text + off, (int)sizeof(g_rec_text) - off);
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_rec_text);
}

static void reports_api(int fd, const char *query)
{
    rec_report_t *list = g_report_list;
    int           n;
    int           i;
    int           off;

    (void)query;

    n = rec_report_list(list, REC_REPORT_MAX);
    off = snprintf(g_rec_text, sizeof(g_rec_text), "ok=1 count=%d\n", n);
    if (off < 0) off = 0;

    for (i = 0; i < n && off < (int)sizeof(g_rec_text) - 80; i++)
    {
        int w = snprintf(g_rec_text + off, sizeof(g_rec_text) - off,
                         "id=%u cam=%d in=%u out=%u peak=%u file=%s\n",
                         list[i].id, list[i].cam, list[i].in, list[i].out,
                         list[i].peak, list[i].file);
        if (w < 0) break;
        off += w;
    }

    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_rec_text);
}

static void report_api(int fd, const char *query)
{
    int          id = osd_web_query_get_int(query, "id", -1);
    int          play = osd_web_query_get_int(query, "play", 0);
    int          ack  = osd_web_query_get_int(query, "ack", 0);
    rec_report_t r;
    int          off;

    if (ack)
    {
        rec_report_ack();
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=1\n");
        return;
    }

    if (id < 0)
    {
        id = rec_report_latest();
    }
    if (id < 0 || rec_report_get((unsigned)id, &r) != 0)
    {
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=0 err=not_found\n");
        return;
    }

    if (play)
    {
        if (rec_report_play((unsigned)id) == 0)
        {
            osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=1\n");
        }
        else
        {
            osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", "ok=0 err=report\n");
        }
        return;
    }

    off = snprintf(g_rec_text, sizeof(g_rec_text),
                   "ok=1&id=%u&cam=%d&t_start=%s&t_end=%s&in=%u&out=%u&"
                   "peak=%u&frames=%u&bytes=%u&file=%s\n",
                   r.id, r.cam, r.t_start, r.t_end, r.in, r.out, r.peak,
                   r.frames, r.bytes, r.file);
    if (off < 0)
    {
        off = 0;
    }
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_rec_text);
}

static void report_pending_api(int fd, const char *query)
{
    unsigned id = 0, in = 0, out = 0, peak = 0, req = 0;
    int      off;

    (void)query;

    if (rec_report_pending(&id, &in, &out, &peak, &req))
    {
        off = snprintf(g_rec_text, sizeof(g_rec_text),
                       "pending=1&id=%u&in=%u&out=%u&peak=%u&req=%u\n",
                       id, in, out, peak, req);
    }
    else
    {
        off = snprintf(g_rec_text, sizeof(g_rec_text), "pending=0\n");
    }

    (void)off;
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_rec_text);
}

int osd_web_rec_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/rec"))
    {
        rec_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/rec/sched"))
    {
        rec_sched_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/reports"))
    {
        reports_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/report"))
    {
        report_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/api/report/pending"))
    {
        report_pending_api(fd, query);
        return 1;
    }
    if (!strcmp(path, "/report") || !strcmp(path, "/report.html"))
    {
        osd_web_send(fd, "200 OK", "text/html; charset=utf-8", REC_HTML);
        return 1;
    }
    return 0;
}
