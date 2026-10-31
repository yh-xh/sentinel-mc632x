/*
 * osd_web_stats.c —— 安防统计页面
 *
 *   GET /stats        图像参数面板（紧凑版，完整版在 /isp）
 *   GET /api/stats    安防统计文本键值，主看板卡片在拉；?reset=1 清零今日
 */
#include <stdio.h>
#include <string.h>

#include "osd_web_key.h"
#include "osd_web_stats.h"
#include "stream/include/stats.h"

static char g_stats_text[4096];

static const char STATS_HTML[] =
    "<!doctype html>\n"
    "<html lang=\"zh\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">\n"
    "<meta name=\"theme-color\" content=\"#fdfaf1\">\n"
    "<title>SENTINEL · 图像参数</title>\n"
    "<script>try{if(localStorage.getItem('sentinel.theme')==='dark'){document.documentElement.setAttribute('data-theme','dark');document.querySelector('meta[name=\"theme-color\"]').setAttribute('content','#0d1014');}}catch(e){}</script>\n"
    "<link rel=\"icon\" href=\"data:,\">\n"
    "<style>\n"
    ":root{--bg:#f7f1e4;--card:#fff;--ink:#2b2418;--dim:#7a6a4f;--line:#e8ddc6;--ac:#e6a700}\n"
    "html[data-theme=dark]{--bg:#0c0f13;--card:#151a21;--ink:#f0eadc;--dim:#9a9284;--line:#2a323e;--ac:#c7b6ff}\n"
    "*{box-sizing:border-box}\n"
    "body{margin:0;padding:16px;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,sans-serif}\n"
    ".wrap{max-width:980px;margin:0 auto}\n"
    ".top{display:flex;align-items:center;gap:10px;margin-bottom:12px}\n"
    ".top a{color:var(--ac);text-decoration:none;font-weight:600}\n"
    ".top .sp{flex:1}\n"
    "button{font:inherit;border:1px solid var(--line);border-radius:10px;background:var(--card);color:var(--ink);padding:8px 12px;cursor:pointer}\n"
    "button:hover{border-color:var(--ac)}\n"
    "h1{font-size:20px;margin:0 0 4px}\n"
    ".sub{color:var(--dim);font-size:12px;margin:0 0 12px}\n"
    ".presets{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px}\n"
    ".presets button{font-size:13px;padding:7px 15px;border-radius:999px;color:var(--dim);transition:border-color .2s,background .2s,color .2s}\n"
    ".presets button:hover{color:var(--ink)}\n"
    ".presets button.on{background:var(--ac);border-color:var(--ac);color:#fff;font-weight:700;box-shadow:0 6px 16px rgba(230,167,0,.22)}\n"
    "html[data-theme=dark] .presets button.on{color:#1a1426}\n"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px}\n"
    ".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px 24px}\n"
    ".ctl{min-width:0}\n"
    ".row{display:flex;justify-content:space-between;font-size:12px;color:var(--dim);margin-bottom:4px}\n"
    ".row b{color:var(--ink);font-weight:600}\n"
    ".row span{font-variant-numeric:tabular-nums;color:var(--ac);font-weight:700}\n"
    "input[type=range]{width:100%;accent-color:var(--ac)}\n"
    "select{width:100%;padding:7px 8px;border:1px solid var(--line);border-radius:9px;background:var(--card);color:var(--ink);font:inherit}\n"
    ".foot{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:12px}\n"
    ".foot .sp{flex:1}\n"
    ".selrow{display:flex;align-items:center;gap:10px;margin:0 0 12px;font-size:13px;color:var(--dim)}\n"
    ".selrow select{width:auto;min-width:220px}\n"
    "@media(max-width:640px){body{padding:10px}.grid{grid-template-columns:1fr}}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<div class=\"wrap\">\n"
    "<div class=\"top\"><a href=\"/\" id=\"back\">← 返回看板</a><span class=\"sp\"></span><button id=\"th\">☾</button></div>\n"
    "<h1>图像参数</h1>\n"
    "<div class=\"sub\" id=\"sub\">每路独立参数 · EV 仍同步两路（防曝光呼吸） · 调节实时下发</div>\n"
    "<div class=\"selrow\"><label for=\"sdev\">应用画面</label><select id=\"sdev\"><option value=\"0\">摄像头 1 · 左摄（G0）</option><option value=\"1\" selected>摄像头 2 · 右摄（G1 / AI）</option></select></div>\n"
    "<div class=\"presets\" id=\"presets\"></div>\n"
    "<div class=\"card\"><div class=\"grid\" id=\"grid\"></div></div>\n"
    "<div class=\"sub\" style=\"margin:10px 0 0\">提示：红/蓝增益 50 = 当前自动白平衡基准（相对调整：0=0.5×、100=1.5×），两路都回到 50 自动恢复自动白平衡。</div>\n"
    "<div class=\"foot\">\n"
    "  <span class=\"sub\" id=\"msg\" style=\"margin:0\">--</span>\n"
    "  <span class=\"sp\"></span>\n"
    "  <button id=\"def\">恢复默认</button>\n"
    "</div>\n"
    "</div>\n"
    "<script>\n"
    "var $=function(id){return document.getElementById(id)};\n"
    "function theme(v){document.documentElement.setAttribute('data-theme',v);try{localStorage.setItem('sentinel.theme',v)}catch(e){};$('th').textContent=v==='dark'?'☀':'☾';var m=document.querySelector('meta[name=\"theme-color\"]');if(m)m.setAttribute('content',v==='dark'?'#0d1014':'#fdfaf1')}\n"
    "var t0='light';try{t0=localStorage.getItem('sentinel.theme')==='dark'?'dark':'light'}catch(e){}\n"
    "theme(t0);$('th').onclick=function(){theme(document.documentElement.getAttribute('data-theme')==='dark'?'light':'dark')};\n"
    "function goBack(){try{if(window.opener)window.close();}catch(e){}setTimeout(function(){if(history.length>1)history.back();else location.href='/';},160);}\n"
    "$('back').onclick=function(e){e.preventDefault();goBack();};\n"
    "var NUM=[['br','亮度',-100,100],['ct','对比度',-100,100],['sat','饱和度',-100,100],['sp','锐度',-100,100],['ev','曝光补偿',-8,8],['rg','红增益',0,100],['bg','蓝增益',0,100]];\n"
    "var SEL=[['dn','日夜模式',['彩色','黑白']],['wb','白平衡',['自动','锁定']]];\n"
    "var DEV=1;\n"
    "var PRESETS=[\n"
    " ['标准',{br:0,ct:0,sat:0,sp:0,ev:0,dn:0,wb:0},'出厂默认，适合大多数场景'],\n"
    " ['明亮',{br:45,ct:8,sat:12,sp:8,ev:0,dn:0,wb:0},'整体提亮，适合光线不足'],\n"
    " ['柔和',{br:12,ct:-30,sat:-25,sp:-30,ev:0,dn:0,wb:0},'低对比低锐度，适合人脸近景'],\n"
    " ['鲜艳',{br:6,ct:30,sat:50,sp:18,ev:0,dn:0,wb:0},'色彩更浓，适合展厅/户外'],\n"
    " ['夜间',{br:40,ct:25,sat:0,sp:30,ev:0,dn:1,wb:0},'黑白高增益，弱光监控'],\n"
    " ['逆光',{br:35,ct:-18,sat:8,sp:8,ev:0,dn:0,wb:0},'提曝光压对比，适合背光门口']\n"
    "];\n"
    "function kv(t){var o={};(t||'').trim().split('&').forEach(function(x){var i=x.indexOf('=');if(i>0)o[x.slice(0,i)]=x.slice(i+1)});return o}\n"
    "function markPreset(){\n"
    "  var cur={};\n"
    "  NUM.forEach(function(c){var r=$('r_'+c[0]);if(r)cur[c[0]]=+r.value;});\n"
    "  SEL.forEach(function(c){var s=$('s_'+c[0]);if(s)cur[c[0]]=+s.value;});\n"
    "  var idx=-1;\n"
    "  PRESETS.forEach(function(p,i){if(idx>=0)return;var o=p[1],ok=true;for(var k in o){if(cur[k]!==o[k])ok=false;}if(ok)idx=i;});\n"
    "  var bs=document.querySelectorAll('#presets button');\n"
    "  for(var i=0;i<bs.length;i++)bs[i].classList.toggle('on',+bs[i].getAttribute('data-p')===idx);\n"
    "}\n"
    "function paint(o){\n"
    "  NUM.forEach(function(c){var k=c[0];if(o[k]===undefined)return;var r=$('r_'+k);if(r)r.value=o[k];var v=$('v_'+k);if(v)v.textContent=o[k];});\n"
    "  SEL.forEach(function(c){var k=c[0];if(o[k]===undefined)return;var s=$('s_'+k);if(s)s.value=o[k];});\n"
    "  markPreset();\n"
    "}\n"
    "function build(){\n"
    "  var h='';\n"
    "  NUM.forEach(function(c){h+='<div class=\"ctl\"><div class=\"row\"><b>'+c[1]+'</b><span id=\"v_'+c[0]+'\">0</span></div><input type=\"range\" id=\"r_'+c[0]+'\" min=\"'+c[2]+'\" max=\"'+c[3]+'\" step=\"1\" value=\"0\" data-k=\"'+c[0]+'\"></div>';});\n"
    "  SEL.forEach(function(c){var o='';c[2].forEach(function(t,i){o+='<option value=\"'+i+'\">'+t+'</option>';});h+='<div class=\"ctl\"><div class=\"row\"><b>'+c[1]+'</b><span>选择</span></div><select id=\"s_'+c[0]+'\" data-k=\"'+c[0]+'\">'+o+'</select></div>';});\n"
    "  $('grid').innerHTML=h;\n"
    "}\n"
    "function buildPresets(){\n"
    "  var h='';\n"
    "  PRESETS.forEach(function(p,i){h+='<button type=\"button\" data-p=\"'+i+'\" title=\"'+p[2]+'\">'+p[0]+'</button>';});\n"
    "  $('presets').innerHTML=h;\n"
    "}\n"
    "function applyPreset(i){\n"
    "  var p=PRESETS[i][1],q=[];\n"
    "  for(var k in p)q.push(k+'='+p[k]);\n"
    "  fetch('/api/isp?'+q.join('&')+'&dev='+DEV,{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){paint(kv(t));$('msg').textContent='已应用：'+PRESETS[i][0]}).catch(function(){$('msg').textContent='下发失败'});\n"
    "}\n"
    "function load(){fetch('/api/isp?dev='+DEV+'&t='+Date.now(),{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){paint(kv(t));$('msg').textContent='已同步'}).catch(function(){$('msg').textContent='板子没有响应'})}\n"
    "var timers={};\n"
    "function apply(k,v){fetch('/api/isp?k='+k+'&v='+v+'&dev='+DEV,{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){paint(kv(t));$('msg').textContent='已下发'}).catch(function(){})}\n"
    "function send(k,v){clearTimeout(timers[k]);timers[k]=setTimeout(function(){apply(k,v)},400)}\n"
    "build();buildPresets();load();\n"
    "$('sdev').onchange=function(){DEV=+this.value;load();};\n"
    "$('presets').addEventListener('click',function(e){var b=e.target.closest('button');if(!b)return;applyPreset(+b.getAttribute('data-p'));});\n"
    "$('grid').addEventListener('input',function(e){var el=e.target;if(el.type!=='range')return;var k=el.getAttribute('data-k');if(!k)return;$('v_'+k).textContent=el.value;send(k,+el.value)});\n"
    "$('grid').addEventListener('change',function(e){var el=e.target;if(el.tagName!=='SELECT')return;apply(el.getAttribute('data-k'),+el.value)});\n"
    "$('def').onclick=function(){if(!confirm('恢复 ISP 默认参数？'))return;fetch('/api/isp?br=0&ct=0&sat=0&sp=0&ev=0&dn=0&wb=0&dev='+DEV,{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){paint(kv(t));$('msg').textContent='已恢复默认'})};\n"
    "</script>\n"
    "</body>\n"
    "</html>\n"
    ;



int osd_web_stats_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/stats"))
    {
        if (osd_web_query_get_int(query, "reset", 0))
        {
            stats_reset();
        }

        stats_format(g_stats_text, (int)sizeof(g_stats_text));
        osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", g_stats_text);
        return 1;
    }

    if (!strcmp(path, "/stats") || !strcmp(path, "/stats.html"))
    {
        osd_web_send(fd, "200 OK", "text/html; charset=utf-8", STATS_HTML);
        return 1;
    }

    return 0;
}
