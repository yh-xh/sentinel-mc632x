/*
 * osd_web_clip.c —— 事件短视频 AVI 下载/回放
 *
 *   GET /api/clip?name=evt_..._clip.avi   二进制 AVI
 *   GET /clip?name=...                    网页播放器（MJPEG AVI 逐帧播放）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osd_web_key.h"
#include "osd_web_clip.h"
#include "stream/include/snap_store.h"

#define CLIP_CHUNK (32 * 1024)

static const char CLIP_HTML[] =
    "<!doctype html>\n"
    "<html lang=\"zh\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">\n"
    "<meta name=\"theme-color\" content=\"#fdfaf1\">\n"
    "<title>SENTINEL · 事件视频</title>\n"
    "<script>try{if(localStorage.getItem('sentinel.theme')==='dark'){document.documentElement.setAttribute('data-theme','dark');document.querySelector('meta[name=\"theme-color\"]').setAttribute('content','#0d1014');}}catch(e){}</script>\n"
    "<link rel=\"icon\" href=\"data:,\">\n"
    "<style>\n"
    ":root{--bg:#f7f1e4;--card:#fff;--ink:#2b2418;--dim:#7a6a4f;--line:#e8ddc6;--ac:#e6a700;--ac2:#8f7bd6}\n"
    "html[data-theme=dark]{--bg:#0c0f13;--card:#151a21;--ink:#f0eadc;--dim:#9a9284;--line:#2a323e;--ac:#c7b6ff;--ac2:#e3d9ff}\n"
    "*{box-sizing:border-box}\n"
    "body{margin:0;padding:16px;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,sans-serif}\n"
    ".wrap{max-width:1180px;margin:0 auto}\n"
    ".top{display:flex;align-items:center;gap:10px;margin-bottom:12px}\n"
    ".top a{color:var(--ac);text-decoration:none;font-weight:600}\n"
    ".top .sp{flex:1}\n"
    "button,a.dl{font:inherit;border:1px solid var(--line);border-radius:10px;background:var(--card);color:var(--ink);padding:8px 12px;cursor:pointer;text-decoration:none}\n"
    "button:hover,a.dl:hover{border-color:var(--ac)}\n"
    "h1{font-size:20px;margin:0 0 4px}\n"
    ".sub{color:var(--dim);font-size:12px;word-break:break-all}\n"
    ".head{margin-bottom:12px}\n"
    ".layout{display:grid;grid-template-columns:minmax(0,1fr) 280px;gap:12px;align-items:start}\n"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:12px}\n"
    ".pv{background:#000;border-radius:12px;overflow:hidden;display:flex;align-items:center;justify-content:center;min-height:200px}\n"
    ".pv img{width:100%;height:auto;display:block}\n"
    ".seek{width:100%;margin-top:12px;accent-color:var(--ac)}\n"
    ".bar{display:flex;gap:8px;align-items:center;margin-top:10px;flex-wrap:wrap}\n"
    ".pos{color:var(--dim);font-size:12px;font-variant-numeric:tabular-nums}\n"
    ".side h2{font-size:14px;margin:2px 0 12px}\n"
    ".rows{display:flex;flex-direction:column;gap:9px;margin-bottom:14px}\n"
    ".row{display:flex;justify-content:space-between;gap:10px;font-size:12px;color:var(--dim);border-bottom:1px dashed var(--line);padding-bottom:7px}\n"
    ".row b{color:var(--ink);font-weight:600;text-align:right;word-break:break-all}\n"
    ".side .dl{display:block;text-align:center}\n"
    ".tips{color:var(--dim);font-size:11px;margin-top:12px;line-height:1.6}\n"
    "@media(max-width:900px){.layout{grid-template-columns:1fr}}\n"
    "@media(max-width:640px){body{padding:10px}}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<div class=\"wrap\">\n"
    "<div class=\"top\"><a href=\"/\" id=\"back\">← 返回看板</a><span class=\"sp\"></span><button id=\"th\">☾</button></div>\n"
    "<div class=\"head\"><h1>事件视频</h1><div class=\"sub\" id=\"sub\">加载中...</div></div>\n"
    "<div class=\"layout\">\n"
    "  <div class=\"card\">\n"
    "    <div class=\"pv\"><img id=\"pv\" alt=\"\"></div>\n"
    "    <input class=\"seek\" id=\"seek\" type=\"range\" min=\"0\" max=\"1\" value=\"0\" step=\"1\">\n"
    "    <div class=\"bar\">\n"
    "      <button id=\"play\">播放</button>\n"
    "      <button id=\"prev\">上一帧</button>\n"
    "      <button id=\"next\">下一帧</button>\n"
    "      <span class=\"pos\" id=\"pos\">--</span>\n"
    "    </div>\n"
    "  </div>\n"
    "  <aside class=\"card side\">\n"
    "    <h2>事件信息</h2>\n"
    "    <div class=\"rows\">\n"
    "      <div class=\"row\"><span>时间</span><b id=\"mtime\">--</b></div>\n"
    "      <div class=\"row\"><span>类型</span><b id=\"mkind\">--</b></div>\n"
    "      <div class=\"row\"><span>帧数</span><b id=\"mframes\">--</b></div>\n"
    "      <div class=\"row\"><span>大小</span><b id=\"msize\">--</b></div>\n"
    "      <div class=\"row\"><span>帧率</span><b>5 fps</b></div>\n"
    "    </div>\n"
    "    <a class=\"dl\" id=\"dl\" href=\"#\" download>下载 AVI</a>\n"
    "    <div class=\"tips\">视频覆盖事件前后各 5 秒，5 帧/秒，逐帧解码 MJPEG，不依赖浏览器 H.264 解码器。</div>\n"
    "  </aside>\n"
    "</div>\n"
    "</div>\n"
    "<script>\n"
    "var $=function(id){return document.getElementById(id)};\n"
    "function theme(v){document.documentElement.setAttribute('data-theme',v);try{localStorage.setItem('sentinel.theme',v)}catch(e){};$('th').textContent=v==='dark'?'☀':'☾';var m=document.querySelector('meta[name=\"theme-color\"]');if(m)m.setAttribute('content',v==='dark'?'#0d1014':'#fdfaf1')}\n"
    "var t0='light';try{t0=localStorage.getItem('sentinel.theme')==='dark'?'dark':'light'}catch(e){}\n"
    "theme(t0);$('th').onclick=function(){theme(document.documentElement.getAttribute('data-theme')==='dark'?'light':'dark')};\n"
    "var name=new URLSearchParams(location.search).get('name')||'';\n"
    "var shortName=name.replace(/^(left|right)\\//,'');\n"
    "function goBack(){try{if(window.opener)window.close();}catch(e){}setTimeout(function(){if(history.length>1)history.back();else location.href='/';},160);}\n"
    "$('back').onclick=function(e){e.preventDefault();goBack();};\n"
    "var frames=[],u=null,idx=0,timer=null,cur=null;\n"
    "function parseMeta(){\n"
    "  var mt=shortName.match(/^evt_(\\d{8})_(\\d{6})_/);\n"
    "  var mk=shortName.match(/_k(\\d+)_r(\\d+)/);\n"
    "  if(mt){var d=mt[1],t=mt[2];$('mtime').textContent=d.slice(0,4)+'-'+d.slice(4,6)+'-'+d.slice(6,8)+' '+t.slice(0,2)+':'+t.slice(2,4)+':'+t.slice(4,6);}\n"
    "  var k=mk?+mk[1]:-1;\n"
    "  $('mkind').textContent=k===1?'区域入侵':k===0?'绊线':'事件';\n"
    "}\n"
    "function show(i){\n"
    "  if(!frames.length)return;\n"
    "  if(i<0)i=frames.length-1; if(i>=frames.length)i=0; idx=i;\n"
    "  var f=frames[i];\n"
    "  var url=URL.createObjectURL(new Blob([u.subarray(f.off,f.off+f.size)],{type:'image/jpeg'}));\n"
    "  var img=$('pv');img.onload=function(){if(cur)URL.revokeObjectURL(cur);cur=url;};img.onerror=function(){URL.revokeObjectURL(url);};img.src=url;\n"
    "  $('seek').value=i;\n"
    "  $('pos').textContent='第 '+(i+1)+' / '+frames.length+' 帧 · '+(f.size/1024).toFixed(0)+' KB';\n"
    "}\n"
    "function play(){if(timer||!frames.length)return;timer=setInterval(function(){show(idx+1)},200);$('play').textContent='暂停'}\n"
    "function pause(){if(timer){clearInterval(timer);timer=null}$('play').textContent='播放'}\n"
    "$('play').onclick=function(){timer?pause():play()};\n"
    "$('prev').onclick=function(){pause();show(idx-1)};\n"
    "$('next').onclick=function(){pause();show(idx+1)};\n"
    "$('seek').oninput=function(){pause();show(+this.value)};\n"
    "function fail(msg){$('sub').textContent=msg}\n"
    "if(!name){fail('缺少文件名');}else{\n"
    "  $('dl').href='/api/clip?name='+encodeURIComponent(name);\n"
    "  $('dl').setAttribute('download',shortName);\n"
    "  $('sub').textContent=shortName;\n"
    "  parseMeta();\n"
    "  fetch('/api/clip?name='+encodeURIComponent(name),{cache:'no-store'}).then(function(r){\n"
    "    if(!r.ok)throw new Error('HTTP '+r.status);\n"
    "    return r.arrayBuffer();\n"
    "  }).then(function(buf){\n"
    "    u=new Uint8Array(buf);\n"
    "    var v=new DataView(buf),p=-1;\n"
    "    for(var i=0;i+12<u.length;i++){\n"
    "      if(u[i]===0x6D&&u[i+1]===0x6F&&u[i+2]===0x76&&u[i+3]===0x69){p=i;break}\n"
    "    }\n"
    "    if(p<0)throw new Error('AVI 没有 movi');\n"
    "    var end=p+v.getUint32(p-4,true);\n"
    "    var q=p+4;\n"
    "    while(q+8<=end&&q+8<=u.length){\n"
    "      var id=String.fromCharCode(u[q],u[q+1],u[q+2],u[q+3]);\n"
    "      var sz=v.getUint32(q+4,true);\n"
    "      if(id==='00dc'||id==='00db')frames.push({off:q+8,size:sz});\n"
    "      q+=8+sz+(sz&1);\n"
    "    }\n"
    "    if(!frames.length)throw new Error('AVI 没有视频帧');\n"
    "    $('mframes').textContent=frames.length;\n"
    "    $('msize').textContent=(buf.byteLength/1048576).toFixed(2)+' MB';\n"
    "    $('seek').max=frames.length-1;\n"
    "    show(0);play();\n"
    "  }).catch(function(e){fail('无法播放：'+e.message)});\n"
    "}\n"
    "</script>\n"
    "</body>\n"
    "</html>\n"
    ;


int osd_web_clip_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/clip"))
    {
        char           name[SNAP_FILE_NAME_MAX];
        unsigned char *buf;
        unsigned       size   = 0;
        unsigned       remain;
        int            cfd;
        int            n;

        if (osd_web_query_get_str(query, "name", name, sizeof(name)) == NULL)
        {
            osd_web_send(fd, "400 Bad Request", "text/plain; charset=utf-8",
                         "missing name\n");
            return 1;
        }

        cfd = snap_store_clip_open(name, &size);
        if (cfd < 0)
        {
            if (cfd == -3)
            {
                osd_web_send(fd, "503 Service Unavailable",
                             "text/plain; charset=utf-8", "sd not ready\n");
            }
            else if (cfd == -2)
            {
                osd_web_send(fd, "413 Payload Too Large",
                             "text/plain; charset=utf-8", "clip too large\n");
            }
            else if (cfd == -4)
            {
                osd_web_send(fd, "400 Bad Request",
                             "text/plain; charset=utf-8", "bad name\n");
            }
            else
            {
                osd_web_send(fd, "404 Not Found",
                             "text/plain; charset=utf-8", "clip not found\n");
            }
            return 1;
        }

        buf = (unsigned char *)malloc(CLIP_CHUNK);
        if (buf == NULL)
        {
            snap_store_clip_close(cfd);
            osd_web_send(fd, "503 Service Unavailable",
                         "text/plain; charset=utf-8", "no memory\n");
            return 1;
        }

        /*
         * 先发头，再按 32KB 一块边读边发。以前是整文件 malloc + 一次 read，
         * 播一个 4MB 的录像要连续读好几秒，全程占着 SD 锁，录像线程写不进去，
         * 卡忙超时后就表现成“播放直接把 SD 卡搞掉”。分块后锁只在一小块
         * 读取期间持有。
         */
        osd_web_send_bin_ex(fd, "200 OK", "video/x-msvideo", NULL,
                            (int)size, NULL);

        remain = size;
        while (remain > 0)
        {
            int want = (remain > CLIP_CHUNK) ? CLIP_CHUNK : (int)remain;

            n = snap_store_clip_read(cfd, buf, want);
            if (n <= 0)
            {
                break;
            }
            if (osd_web_send_raw(fd, buf, n) != 0)
            {
                break;      /* 播放器被关掉/网络断了 */
            }
            remain -= (unsigned)n;
        }

        snap_store_clip_close(cfd);
        free(buf);
        return 1;
    }

    if (!strcmp(path, "/clip") || !strcmp(path, "/clip.html"))
    {
        osd_web_send(fd, "200 OK", "text/html; charset=utf-8", CLIP_HTML);
        return 1;
    }

    return 0;
}
