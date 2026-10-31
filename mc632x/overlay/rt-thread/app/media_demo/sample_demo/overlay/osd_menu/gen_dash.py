import json, os

html = r'''<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#fdfaf1">
<title>SENTINEL · 双摄 AI 终端</title>
<script>try{if(localStorage.getItem("sentinel.theme")==="dark"){document.documentElement.setAttribute("data-theme","dark");document.querySelector('meta[name="theme-color"]').setAttribute("content","#0d1014");}}catch(e){}</script>
<link rel="icon" href="data:,">
<link rel="stylesheet" href="/dash.css?v=14">
<link rel="stylesheet" href="/dash2.css?v=14">
<link rel="stylesheet" href="/dash3.css?v=14">
</head>
<body>
<div class="bg"></div>
<header class="top">
  <div class="topinner">
    <div class="brand">
      <div class="mark"><i></i><i></i><i></i></div>
      <div class="brandtext">
        <h1>SENTINEL</h1>
        <p>MC632X · 双摄 AI 终端</p>
      </div>
    </div>
    <div class="topright">
      <div class="signal" aria-hidden="true"><i></i><i></i><i></i><i></i><i></i></div>
      <div class="pill off" id="armpill"><i></i><span>已撤防</span></div>
      <button type="button" id="themebtn" class="themebtn" aria-label="切换夜间模式" title="切换夜间模式">☾</button>
      <a class="themebtn" href="/stats" aria-label="图像参数" title="图像参数">⚙</a>
      <div class="clock" id="clock">--:--:--</div>
    </div>
  </div>
</header>

<main>
  <section class="hero card">
    <div class="screens">
      <div class="screen clean">
        <img id="pv0" alt="原始画面">
        <div class="tag">原始画面</div>
      </div>
      <div class="screen ai">
        <img id="pv1" alt="AI 追踪画面">
        <div class="ri" id="rulelay"><i class="ln"></i><i class="bx a"></i><i class="bx b"></i></div>
        <div class="tag"><i></i>AI 追踪</div>
        <div class="scan"></div>
      </div>
    </div>
    <div class="herofoot">
      <div class="lcd">
        <span class="label">LCD 画面源</span>
        <div class="seg" id="lcdseg">
          <button type="button" data-v="0" class="on">AI 叠加</button>
          <button type="button" data-v="1">干净原图</button>
        </div>
      </div>
      <div class="hint" id="lcdhint">LCD：AI 叠加 · 检测框可见</div>
    </div>
  </section>

  <div class="rail">
  <section class="actions">
    <button type="button" id="armbtn" class="btn arm">
      <span class="ico">◇</span>
      <span class="bt"><b>布防</b><small>自动存证 + 语音告警</small></span>
    </button>
    <button type="button" id="snapbtn" class="btn">
      <span class="ico">◎</span>
      <span class="bt"><b>抓拍存证</b><small>当前选中摄像头</small></span>
    </button>
    <button type="button" id="ovlbtn" class="btn ghost">
      <span class="ico">◉</span>
      <span class="bt"><b>隐藏叠加</b><small>检测仍在运行</small></span>
    </button>
  </section>

  <section class="metrics">
    <div class="metric"><span>当前人数</span><b id="mpeople">--</b><small>实时 AI 结果</small></div>
    <div class="metric"><span>本次事件</span><b id="mevents">--</b><small>绊线 / 周界 / 手动</small></div>
    <div class="metric"><span>存储状态</span><b id="msd">--</b><small>SD /mnt</small></div>
    <div class="metric"><span>AI 模型</span><b id="mmodel">--</b><small id="mmodelnote">NBG case id</small></div>
  </section>

  </div>

  <section class="card reccard">
    <div class="cardhead">
      <h2>录制</h2>
      <span class="dim small" id="rec_sub">未录制</span>
    </div>
    <div class="recgrid">
      <div class="recblock">
        <div class="reclabel">手动录制（快捷 30 秒）</div>
        <div class="recrowline">
          <div class="seg" id="recseg">
            <button type="button" data-cam="0">左摄</button>
            <button type="button" data-cam="1" class="on">右摄</button>
          </div>
          <button type="button" id="recbtn" class="recbtn">● 录制 30 秒</button>
        </div>
      </div>
      <div class="recblock">
        <div class="reclabel" id="schedlabel">定时录制（到点自动 AI 追踪）</div>
        <div class="recrowline">
          <input type="date" id="sched_date">
          <input type="time" id="sched_start" value="09:00">
          <span class="dim small">—</span>
          <input type="time" id="sched_end" value="10:00">
          <label class="chk"><input type="checkbox" id="sched_daily">每天</label>
          <button type="button" id="schedbtn" class="recbtn ghost">启用</button>
          <button type="button" id="schedclr" class="recbtn ghost">取消</button>
        </div>
      </div>
      <div class="recblock recside">
        <div class="sub" id="sched_hint">未设置定时</div>
        <a id="reportlink" href="#" style="display:none">查看报告 →</a>
      </div>
    </div>
  </section>

  <section class="card syscard">
    <div class="cardhead">
      <h2>设备状态</h2>
      <div class="syshead">
        <span class="dim small" id="sysmsg">实时采样 · 2s</span>
      </div>
    </div>
    <div class="sysgrid">
      <div class="sysitem">
        <div class="syslabel">CPU 占用</div>
        <div class="sysval"><b id="s_cpu">--</b><span>%</span></div>
        <div class="bar"><i id="b_cpu"></i></div>
        <div class="sysnote">系统负载</div>
      </div>
      <div class="sysitem">
        <div class="syslabel">内存</div>
        <div class="sysval"><b id="s_mem">--</b><span>%</span></div>
        <div class="bar"><i id="b_mem"></i></div>
        <div class="sysnote" id="s_mem_note">-- / -- MB</div>
      </div>
      <div class="sysitem">
        <div class="syslabel">芯片温度</div>
        <div class="sysval"><b id="s_temp">--</b><span>°C</span></div>
        <div class="bar temp"><i id="b_temp"></i></div>
        <div class="sysnote">RTC 温度传感器</div>
      </div>
      <div class="sysitem">
        <div class="syslabel">网卡 IP</div>
        <div class="sysval ip"><b id="s_ip">--</b></div>
        <div class="sysnote" id="s_uptime">运行 --</div>
      </div>
      <div class="sysitem">
        <div class="syslabel">系统版本</div>
        <div class="sysval sm"><b id="s_sys">--</b></div>
        <div class="sysnote">RT-Thread 内核</div>
      </div>
      <div class="sysitem">
        <div class="syslabel">固件版本</div>
        <div class="sysval sm"><b id="s_fw">--</b></div>
        <div class="sysnote" id="s_build">--</div>
      </div>
    </div>
  </section>

  <section class="card statscard">
    <div class="cardhead">
      <h2>安防统计</h2>
      <div class="statshead">
        <span class="dim small" id="st_sub">加载中...</span>
        <button type="button" id="st_reset" class="mini">清零今日</button>
      </div>
    </div>
    <div class="statkpis">
      <div class="statkpi"><span>今日进入</span><b id="st_in">--</b><small>绊线</small></div>
      <div class="statkpi"><span>今日离开</span><b id="st_out">--</b><small>绊线</small></div>
      <div class="statkpi"><span>区域进入</span><b id="st_ain">--</b><small>周界</small></div>
      <div class="statkpi"><span>区域离开</span><b id="st_aout">--</b><small>周界</small></div>
      <div class="statkpi"><span>最长停留</span><b id="st_dwell">--</b><small>区域</small></div>
      <div class="statkpi"><span>今日峰值</span><b id="st_max">--</b><small>画面人数</small></div>
    </div>
    <div class="stwrap">
      <div class="stlegend">
        <span><i class="c1"></i>进入</span>
        <span><i class="c2"></i>离开</span>
        <span class="stpeak" id="st_peak"></span>
      </div>
      <canvas id="stch" width="900" height="230"></canvas>
      <div class="sttip" id="st_tip"></div>
    </div>
    <div class="stfoot"><span id="st_last">最近事件：--</span><span class="dim">每 5 秒刷新</span></div>
  </section>

  <div class="cols">
    <div class="colstack">
    <section class="card">
      <div class="cardhead">
        <h2>事件时间线</h2>
        <div class="evtools">
          <span class="dim small" id="evsum">LIVE</span>
          <button type="button" id="missbtn" class="mini">记录漏报</button>
        </div>
      </div>
      <div id="events" class="timeline"><div class="empty">暂无事件</div></div>
    </section>

    <section class="card termcard">
      <div class="cardhead">
        <h2>设备终端 · msh</h2>
        <div class="termhead">
          <span class="termstate" id="termstate"><i></i>空闲</span>
          <button type="button" id="termclear" class="mini">清屏</button>
        </div>
      </div>
      <div class="termwrap">
        <pre id="termout" class="termout">MC632X 网页终端已就绪。&#10;命令在板端 msh 线程中执行；输入 help 查看命令列表。&#10;</pre>
        <form id="termform" class="termline" autocomplete="off">
          <span class="termprompt">msh &gt;</span>
          <input id="terminput" type="text" spellcheck="false" autocapitalize="off" autocomplete="off" placeholder="输入命令，回车执行">
          <button type="submit" class="mini">执行</button>
        </form>
        <div class="termquick">
          <button type="button" class="mini" data-cmd="help">help</button>
          <button type="button" class="mini" data-cmd="ps">ps</button>
          <button type="button" class="mini" data-cmd="free">free</button>
          <button type="button" class="mini" data-cmd="ifconfig">ifconfig</button>
          <button type="button" class="mini" data-cmd="media_info">media_info</button>
          <button type="button" class="mini" data-cmd="ls /mnt/snap">ls /mnt/snap</button>
          <button type="button" class="mini" data-cmd="df">df</button>
          <button type="button" class="mini" data-cmd="date">date</button>
        </div>
        <div class="termnote dim small">终端仅限局域网使用；reboot / fal / rm 等命令会二次确认。</div>
      </div>
    </section>
    </div>

    <section class="card">
      <div class="cardhead">
        <h2>证据相册</h2>
        <div class="chips" id="filter">
          <button type="button" data-f="all" class="on">全部</button>
          <button type="button" data-f="manual">手动</button>
          <button type="button" data-f="trip">绊线</button>
          <button type="button" data-f="peri">周界</button>
          <button type="button" data-f="left">左摄</button>
          <button type="button" data-f="right">右摄</button>
          <button type="button" data-f="img">图片</button>
          <button type="button" data-f="vid">视频</button>
        </div>
      </div>
      <div id="gallery" class="grid"></div>
      <div class="dim small" id="galtxt">正在读取...</div>
      <div class="maint">
        <button type="button" id="galrf" class="mini">刷新</button>
        <button type="button" id="galclean" class="mini">保留最近 100</button>
        <button type="button" id="galspace" class="mini">容量</button>
        <button type="button" id="galmount" class="mini">重挂 SD</button>
        <button type="button" id="sdtest" class="mini">SD 自检</button>
        <button type="button" id="testbtn" class="mini">测试模式：关</button>
        <a class="mini" href="/api/testlog" target="_blank">检测CSV</a>
        <a class="mini" href="/api/labels" target="_blank">标签CSV</a>
      </div>
    </section>
  </div>

  <footer>
    <div class="footline"><i class="live"></i><span id="host">--</span><span class="dim">· 本地边缘 AI</span></div>
    <div class="links">
      <a href="/stats">图像参数</a>
      <span class="dim">RTSP 1240 / 1246 / 1248</span>
    </div>
    <div class="dim small">SENTINEL / MC632X · 数据本地存储 · 断网可用</div>
  </footer>
</main>
<div class="toast" id="toast"></div>
<script src="/dash.js?v=14" defer></script>
<script src="/dash2.js?v=14" defer></script>
<script src="/dash3.js?v=14" defer></script>
</body>
</html>
'''

css = r''':root{
--bg:#f7f1e4;--panel:#ffffff;--panel2:#fffaf0;--line:#e8ddc6;--line2:#f2ead9;
--text:#22272e;--dim:#857b6a;--dim2:#a89c87;
--accent:#f0b429;--accent2:#ffd75e;--accent3:#fff3cf;--ink:#7a4d00;
--red:#d9534f;--redbg:#fdecea;--green:#3e9b63;--greenbg:#eaf7ee;
--chart2:#7c6bd6;
--radius:20px;--shadow:0 10px 30px rgba(120,94,36,.10),0 2px 8px rgba(120,94,36,.05);
--shadow-lg:0 22px 60px rgba(120,94,36,.16);
}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font:15px/1.55 ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,"Noto Sans SC",sans-serif;-webkit-text-size-adjust:100%;color-scheme:light}
body{overflow-x:hidden}
.bg{position:fixed;inset:0;z-index:-1;overflow:hidden;background:linear-gradient(rgba(160,130,60,.055) 1px,transparent 1px),linear-gradient(90deg,rgba(160,130,60,.055) 1px,transparent 1px),linear-gradient(180deg,#fdfaf1 0%,#f7f0df 55%,#f3ead7 100%);background-size:48px 48px,48px 48px,auto}
.bg::before,.bg::after{content:"";position:absolute;width:52vw;height:52vw;border-radius:50%;filter:blur(80px);opacity:.72;pointer-events:none}
.bg::before{background:radial-gradient(circle at 30% 30%,#ffe7a3 0%,rgba(255,231,163,0) 66%);top:-22vw;left:-14vw;animation:drift1 20s ease-in-out infinite alternate}
.bg::after{background:radial-gradient(circle at 60% 60%,#ffd6a8 0%,rgba(255,214,168,0) 66%);right:-16vw;bottom:-24vw;animation:drift2 24s ease-in-out infinite alternate}
.top{position:sticky;top:0;z-index:20;background:rgba(253,250,241,.84);backdrop-filter:blur(18px);border-bottom:1px solid var(--line);box-shadow:0 1px 0 rgba(255,255,255,.85)}
.top::after{content:"";display:block;height:3px;background:linear-gradient(90deg,transparent,var(--accent),#ffe28a,var(--accent),transparent);background-size:200% 100%;animation:slide 9s linear infinite}
.topinner{width:min(1800px,100%);margin:0 auto;padding:13px clamp(14px,2.6vw,36px);display:flex;align-items:center;justify-content:space-between;gap:14px}
.brand{display:flex;align-items:center;gap:12px;min-width:0}
.mark{width:42px;height:42px;border-radius:14px;position:relative;background:linear-gradient(145deg,#fff8e2,#ffe9a8);border:1px solid #e9c96a;box-shadow:0 6px 18px rgba(214,164,24,.22),inset 0 1px 0 #fff;flex:0 0 auto}
.mark i{position:absolute;inset:8px;border:1.5px solid rgba(186,130,10,.32);border-radius:50%}
.mark i:first-child{border-color:rgba(240,180,41,.25);border-top-color:#dda000;border-right-color:rgba(221,160,0,.7);animation:spin 7s linear infinite}
.mark i:nth-child(2){inset:13px;border-color:rgba(186,130,10,.5)}
.mark i:nth-child(3){inset:17px;background:#f0b429;border:0;box-shadow:0 0 12px rgba(240,180,41,.85);animation:pulse 2.4s infinite}
.brandtext{min-width:0}
.brandtext h1{font-size:17px;margin:0;letter-spacing:.2em;font-weight:800;color:#2b2414;white-space:nowrap}
.brandtext p{margin:1px 0 0;color:var(--dim);font-size:11px;letter-spacing:.08em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.topright{display:flex;align-items:center;gap:12px;flex:0 0 auto}
.signal{display:flex;align-items:flex-end;gap:2px;height:16px}
.signal i{width:3px;height:7px;border-radius:2px;background:linear-gradient(180deg,var(--accent),#dd9a00);transform-origin:bottom;animation:eq 1.2s ease-in-out infinite}
.signal i:nth-child(2){height:11px;animation-delay:.18s}
.signal i:nth-child(3){height:6px;animation-delay:.34s}
.signal i:nth-child(4){height:13px;animation-delay:.52s}
.signal i:nth-child(5){height:8px;animation-delay:.7s}
.pill{display:flex;align-items:center;gap:7px;padding:7px 12px;border-radius:999px;font-size:12px;font-weight:700;border:1px solid var(--line);background:var(--panel);color:var(--dim);box-shadow:0 4px 14px rgba(120,94,36,.07);transition:.25s}
.pill i{width:7px;height:7px;border-radius:50%;background:var(--dim2)}
.pill.off i{background:var(--dim2);box-shadow:none}
.pill.on{color:#a33b2f;border-color:#f2c4bd;background:var(--redbg)}
.pill.on i{background:var(--red);box-shadow:0 0 0 4px rgba(217,83,79,.14);animation:pulse 1.4s infinite}
.clock{font-variant-numeric:tabular-nums;color:#6d6350;font-size:12px;letter-spacing:.1em;font-weight:600;white-space:nowrap}
main{width:min(1800px,100%);margin:0 auto;padding:18px clamp(14px,2.6vw,36px) 56px;display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:12px 14px;align-items:start}
.hero{grid-column:span 8}
.rail{grid-column:span 4;display:flex;flex-direction:column;gap:10px;min-width:0}
.ispcard{grid-column:1/-1;margin:0}
.cols{grid-column:1/-1}
footer{grid-column:1/-1}
.card{position:relative;background:linear-gradient(180deg,#ffffff,#fffdf8);border:1px solid var(--line);border-radius:var(--radius);box-shadow:var(--shadow);transition:box-shadow .3s,border-color .3s,transform .3s}
.card:hover{box-shadow:var(--shadow-lg);border-color:#e2d3b2}
.hero{padding:10px;overflow:hidden}
.screens{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.screen{position:relative;border-radius:14px;overflow:hidden;background:#0b0f14;border:1px solid #e7dcc4;aspect-ratio:5/3;box-shadow:inset 0 0 0 1px rgba(255,255,255,.04)}
.screen img{width:100%;height:100%;object-fit:cover;display:block;opacity:.98}
.screen .tag{position:absolute;left:10px;top:10px;display:flex;align-items:center;gap:6px;font-size:11px;font-weight:700;letter-spacing:.06em;color:#5c4300;background:rgba(255,250,235,.92);border:1px solid rgba(240,180,41,.55);padding:4px 9px;border-radius:999px;backdrop-filter:blur(8px);box-shadow:0 4px 14px rgba(80,60,10,.18)}
.screen .tag i{width:6px;height:6px;border-radius:50%;background:#f0b429;box-shadow:0 0 0 3px rgba(240,180,41,.2);animation:pulse 1.8s infinite}
.screen.clean .tag{color:#4a4f56;border-color:rgba(255,255,255,.5);background:rgba(255,255,255,.84)}
.screen .scan{position:absolute;inset:0;background:linear-gradient(180deg,transparent 0%,rgba(255,205,80,.10) 48%,transparent 55%);background-size:100% 10px;animation:scan 6s linear infinite;pointer-events:none;opacity:.45}
body.armed .screen.ai{border-color:rgba(217,83,79,.65);box-shadow:0 0 0 2px rgba(217,83,79,.12),0 0 34px rgba(217,83,79,.18)}
.herofoot{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 4px 2px;flex-wrap:wrap}
.lcd{display:flex;align-items:center;gap:10px}
.label{font-size:11px;color:var(--dim);letter-spacing:.1em;text-transform:uppercase;font-weight:700}
.seg{display:flex;background:#fbf5e6;border:1px solid var(--line);border-radius:11px;padding:3px;gap:3px}
.seg button{border:0;background:transparent;color:var(--dim);font:inherit;font-size:12px;padding:6px 11px;border-radius:8px;cursor:pointer;transition:.2s}
.seg button.on{background:#fff;color:var(--ink);box-shadow:0 2px 8px rgba(160,120,20,.16),inset 0 0 0 1px rgba(240,180,41,.45);font-weight:700}
.hint{font-size:11px;color:var(--dim)}
.reccard{grid-column:1/-1;margin:0}
.recgrid{display:grid;grid-template-columns:minmax(0,auto) minmax(0,1fr) minmax(170px,auto);gap:24px;align-items:start;padding:2px 16px 16px}
.recblock{min-width:0}
.recrowline{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-top:7px}
.recside{display:flex;flex-direction:column;align-items:flex-end;gap:8px;text-align:right}
.recside .sub{padding:0;margin:0}
@media(max-width:900px){.recgrid{grid-template-columns:1fr}.recside{align-items:flex-start;text-align:left}}
.recrow{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:7px 16px}
.reclabel{font-size:11px;color:var(--dim);letter-spacing:.08em;text-transform:uppercase;font-weight:700;min-width:34px}
.recbtn{border:1px solid var(--line);background:#fff;color:var(--ink);font:inherit;font-size:12px;font-weight:700;padding:7px 12px;border-radius:10px;cursor:pointer;transition:.2s}
.recbtn:hover{border-color:#e6c66d}
.recbtn.recording{background:#d9534f;border-color:#d9534f;color:#fff}
.recbtn.ghost{background:transparent;color:var(--dim)}
.recdurwrap{display:inline-flex;align-items:center;gap:6px}
.recdur{font:inherit;font-size:12px;padding:6px 8px;border:1px solid var(--line);border-radius:9px;background:var(--card);color:var(--ink);width:64px;text-align:center}
.recdurun{font-size:12px;color:var(--dim)}
.reccard input[type=date],.reccard input[type=time]{font:inherit;font-size:12px;padding:6px 8px;border:1px solid var(--line);border-radius:9px;background:var(--card);color:var(--ink)}
.chk{display:flex;align-items:center;gap:4px;font-size:12px;color:var(--dim)}
.reccard .sub{padding:0 16px 10px;margin:0}
#reportlink{padding:6px 12px;border:1px solid var(--ac);border-radius:10px;color:var(--ac);font-weight:700;font-size:12px;text-decoration:none}
html[data-theme=dark] .recbtn{background:#191f29;color:var(--ink)}
html[data-theme=dark] .recbtn.recording{background:#d9534f;color:#fff}
html[data-theme=dark] .recbtn.ghost{background:transparent}
.actions{display:flex;flex-direction:column;gap:9px;margin:0}
.btn{position:relative;display:flex;align-items:center;gap:12px;text-align:left;padding:13px 15px;border-radius:15px;border:1px solid var(--line);background:#fff;color:var(--text);font:inherit;cursor:pointer;overflow:hidden;transition:transform .2s,border-color .2s,box-shadow .2s,background .2s;box-shadow:0 4px 14px rgba(120,94,36,.05)}
.btn:hover{transform:translateY(-2px);border-color:#e6c66d;box-shadow:0 10px 24px rgba(180,140,30,.14)}
.btn:active{transform:translateY(0)}
.btn::after{content:"";position:absolute;top:-40%;bottom:-40%;width:40%;left:-60%;background:linear-gradient(100deg,transparent,rgba(255,255,255,.85),transparent);transform:skewX(-18deg);transition:left .55s ease;pointer-events:none;opacity:.8}
.btn:hover::after{left:120%}
.btn .ico{font-size:21px;color:#e0a000;width:28px;text-align:center;flex:0 0 auto}
.btn .bt{display:flex;flex-direction:column;gap:1px;min-width:0}
.btn b{font-size:14px;font-weight:700}
.btn small{font-size:11px;color:var(--dim)}
.btn.arm{border-color:#efd48a;background:linear-gradient(180deg,#fffdf4,#fff5d6)}
.btn.arm .ico{color:#d18f00}
.btn.arm.on{border-color:#f0bdb6;background:linear-gradient(180deg,#fff8f7,#fdeeea)}
.btn.arm.on .ico{color:var(--red)}
.btn.ghost{background:#fffdf7}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin:0}
.metric{position:relative;padding:12px 14px 12px 17px;border-radius:15px;border:1px solid var(--line);background:#fff;box-shadow:0 4px 14px rgba(120,94,36,.05);transition:.25s;overflow:hidden}
.metric::before{content:"";position:absolute;left:0;top:0;bottom:0;width:3px;background:linear-gradient(180deg,var(--accent),var(--accent2));opacity:.7}
.metric:hover{transform:translateY(-2px);box-shadow:0 12px 24px rgba(180,140,30,.12)}
.metric span{display:block;font-size:10px;letter-spacing:.1em;color:var(--dim);text-transform:uppercase;font-weight:700}
.metric b{display:inline-block;font-size:24px;line-height:1.2;margin:3px 0 1px;font-variant-numeric:tabular-nums;color:#2b2414;transform-origin:left center;transition:color .3s}
.metric b.bump{animation:bump .4s ease}
.metric small{display:block;font-size:10px;color:var(--dim2)}
.metric b.good{color:var(--green)}.metric b.bad{color:var(--red)}
.ispcard{margin:0}
.isphead{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.isphead a.mini{text-decoration:none;display:inline-flex;align-items:center}
.ispgrid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px 16px;padding:2px 16px 4px}
.ispctl{display:flex;flex-direction:column;gap:4px;min-width:0}
.ispctl .row{display:flex;align-items:center;justify-content:space-between;gap:8px;font-size:11px;color:var(--dim)}
.ispctl .row b{font-size:11px;color:#4b4333;font-weight:700}
.ispctl .row span{font-variant-numeric:tabular-nums;color:#9a6b00;font-weight:700}
.ispctl input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:20px;margin:0;background:transparent;cursor:pointer}
.ispctl input[type=range]::-webkit-slider-runnable-track{height:7px;border-radius:4px;background:#f1e7cf;box-shadow:inset 0 1px 2px rgba(120,90,20,.08)}
.ispctl input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:17px;height:17px;margin-top:-5px;border-radius:50%;background:var(--accent);border:2px solid #fff;box-shadow:0 2px 7px rgba(160,120,20,.35)}
.ispctl input[type=range]::-moz-range-track{height:7px;border-radius:4px;background:#f1e7cf}
.ispctl input[type=range]::-moz-range-thumb{width:15px;height:15px;border:2px solid #fff;border-radius:50%;background:var(--accent);box-shadow:0 2px 7px rgba(160,120,20,.35)}
.ispctl select{width:100%;background:#fffdf7;border:1px solid var(--line);border-radius:9px;color:var(--text);font:inherit;font-size:11px;padding:6px 8px;outline:none}
.ispctl select:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(240,180,41,.15)}
.ispnote{padding:2px 16px 14px}
.cols{display:grid;grid-template-columns:1fr 1.15fr;gap:14px;align-items:start}
.colstack{display:flex;flex-direction:column;gap:14px;min-width:0}
.termcard{margin:0}
.termhead{display:flex;align-items:center;gap:8px}
.termstate{display:flex;align-items:center;gap:6px;font-size:11px;color:var(--dim);font-weight:700}
.termstate i{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 0 3px rgba(62,155,99,.14);animation:pulse 1.8s infinite}
.termstate.busy{color:#9a6b00}
.termstate.busy i{background:var(--accent);box-shadow:0 0 0 3px rgba(240,180,41,.18);animation:pulse .9s infinite}
.termwrap{padding:0 16px 14px}
.termout{height:clamp(150px,20vh,220px);overflow:auto;background:#fffdf5;border:1px solid var(--line);border-radius:12px;padding:10px 12px;font:12px/1.55 ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;color:#3b3425;white-space:pre-wrap;word-break:break-word;box-shadow:inset 0 1px 3px rgba(120,90,20,.06)}
.termline{display:flex;align-items:center;gap:8px;margin-top:8px}
.termprompt{font:12px/1 ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;color:#c47f00;font-weight:700;flex:0 0 auto}
.termline input{flex:1;min-width:0;border:1px solid var(--line);background:#fff;border-radius:9px;padding:8px 10px;font:12px/1.4 ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;color:#2b2414;outline:none;transition:.2s}
.termline input:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(240,180,41,.15)}
.termquick{display:flex;gap:5px;flex-wrap:wrap;margin-top:8px}
.termnote{margin-top:7px}
.cardhead{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:14px 16px 8px}
.cardhead h2{font-size:13px;margin:0;letter-spacing:.09em;color:#4b4333;font-weight:800;text-transform:uppercase}
.dim{color:var(--dim)}.small{font-size:11px}
.timeline{padding:0 16px 14px;max-height:320px;overflow:auto}
.ev{display:grid;grid-template-columns:10px 1fr auto;gap:10px;padding:9px 2px;border-bottom:1px solid var(--line2);align-items:start;border-radius:8px;transition:background .2s}
.ev:hover{background:#fffaf0}
.ev:last-child{border-bottom:0}
.ev .dot{width:7px;height:7px;border-radius:50%;margin-top:6px;background:var(--accent);box-shadow:0 0 0 3px rgba(240,180,41,.18)}
.ev.k0 .dot{background:#e09a00;box-shadow:0 0 0 3px rgba(224,154,0,.16)}
.ev.k1 .dot{background:var(--red);box-shadow:0 0 0 3px rgba(217,83,79,.14)}
.ev.k2 .dot{background:var(--green);box-shadow:0 0 0 3px rgba(62,155,99,.14)}
.ev .t{font-size:12px;color:#3b3425;font-variant-numeric:tabular-nums;font-weight:600}
.ev .m{font-size:10px;color:var(--dim);margin-top:1px}
.ev .r{display:flex;gap:5px;flex-wrap:wrap;align-items:center;justify-content:flex-end}
.evtools{display:flex;align-items:center;gap:6px}
.ev .lab{border:1px solid var(--line);background:#fffdf7;color:var(--dim);font:inherit;font-size:10px;padding:2px 6px;border-radius:6px;cursor:pointer;transition:.2s}
.ev .lab.ok.on{background:var(--greenbg);color:var(--green);border-color:#bfe3cc}
.ev .lab.no.on{background:var(--redbg);color:var(--red);border-color:#f0c2bd}
.ev .lab:hover{border-color:var(--accent)}
.ev a{font-size:10px;color:#9a6b00;text-decoration:none;border:1px solid #f0d9a0;background:#fffaf0;padding:2px 7px;border-radius:7px;transition:.2s}
.ev a:hover{background:var(--accent2);border-color:var(--accent)}
.empty{padding:18px 0;color:var(--dim);font-size:12px;text-align:center}
.chips{display:flex;gap:5px;flex-wrap:wrap}
.chips button{border:1px solid var(--line);background:#fffdf7;color:var(--dim);font:inherit;font-size:10px;padding:3px 9px;border-radius:999px;cursor:pointer;transition:.2s}
.chips button:hover{border-color:var(--accent);color:#8a5c00}
.chips button.on{color:var(--ink);border-color:#efd48a;background:var(--accent3);font-weight:700}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;padding:0 16px}
.gi{position:relative;border-radius:10px;overflow:hidden;background:#eee;border:1px solid var(--line);aspect-ratio:5/3;transition:transform .25s,box-shadow .25s}
.gi:hover{transform:translateY(-2px);box-shadow:0 10px 24px rgba(120,94,36,.16)}
.gi img{width:100%;height:100%;object-fit:cover;display:block;transition:transform .4s}
.gi:hover img{transform:scale(1.04)}
.gi .x{position:absolute;top:4px;right:4px;width:21px;height:21px;border:0;border-radius:50%;background:rgba(35,28,14,.68);color:#fff;font-size:13px;line-height:21px;padding:0;cursor:pointer;backdrop-filter:blur(4px)}
.gi .nm{position:absolute;left:0;right:0;bottom:0;padding:3px 6px;font-size:8px;color:#fff;background:linear-gradient(transparent,rgba(30,22,8,.75));white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.gi .tag{position:absolute;top:4px;left:4px;z-index:2;font-size:9px;line-height:1;padding:3px 7px;border-radius:999px;color:#fff;background:rgba(35,28,14,.62);backdrop-filter:blur(4px);letter-spacing:.4px}
.gi.vid{border-color:#d8b464;background:#241b0b}
.gi.vid .tag{background:#6b4f16;color:#ffe9b0;font-weight:700}
.gi .play{display:flex;align-items:center;justify-content:center;width:100%;height:100%;font-size:32px;color:#fff;background:linear-gradient(135deg,#2f2510,#6b4f16)}
#galtxt{padding:6px 16px 0}
.maint{display:flex;gap:6px;flex-wrap:wrap;padding:10px 16px 14px}
.mini{border:1px solid var(--line);background:#fffdf7;color:#6d6350;font:inherit;font-size:10px;padding:5px 10px;border-radius:9px;cursor:pointer;transition:.2s}
.mini.on{color:var(--ink);border-color:#efd48a;background:var(--accent3);font-weight:700}
.maint a.mini{text-decoration:none;display:inline-flex;align-items:center}
.mini:hover{color:var(--ink);border-color:var(--accent);background:var(--accent3)}
footer{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-top:2px;padding:14px 4px;border-top:1px solid var(--line);font-size:11px;color:var(--dim)}
.footline{display:flex;align-items:center;gap:7px}
.live{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 0 4px rgba(62,155,99,.14);animation:pulse 1.8s infinite}
.links{display:flex;gap:12px;flex-wrap:wrap}
.links a{color:#9a6b00;text-decoration:none;font-weight:600}
.links a:hover{color:#c47f00}
.toast{position:fixed;left:50%;bottom:26px;transform:translate(-50%,18px);opacity:0;pointer-events:none;background:#fffdf7;border:1px solid #efd48a;color:#4b3a12;padding:10px 18px;border-radius:13px;font-size:12px;box-shadow:0 18px 40px rgba(120,94,36,.18);z-index:30;transition:.28s;max-width:86vw;text-align:center}
.toast.show{opacity:1;transform:translate(-50%,0)}
@keyframes pulse{50%{opacity:.4}}
@keyframes spin{to{transform:rotate(360deg)}}
@keyframes eq{0%,100%{transform:scaleY(.45);opacity:.55}50%{transform:scaleY(1);opacity:1}}
@keyframes scan{to{background-position:0 100%}}
@keyframes drift1{to{transform:translate3d(7vw,5vh,0) scale(1.15)}}
@keyframes drift2{to{transform:translate3d(-6vw,-5vh,0) scale(1.12)}}
@keyframes slide{to{background-position:200% 0}}
@keyframes bump{0%{transform:scale(1)}45%{transform:scale(1.22);color:#d18f00}100%{transform:scale(1)}}
@media(max-width:1180px){main{display:block}.rail{margin:12px 0}.actions{display:grid;grid-template-columns:repeat(3,1fr);margin:0 0 10px}.metrics{grid-template-columns:repeat(4,1fr);margin:0}}
@media(max-width:900px){.cols{grid-template-columns:1fr}.actions{grid-template-columns:1fr 1fr}.metrics{grid-template-columns:1fr 1fr}.ispgrid{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media(max-width:640px){main{padding:10px 10px 36px}.topinner{padding:10px 12px}.brandtext p{display:none}.clock{display:none}.signal{display:none}.screens{grid-template-columns:1fr}.rail{display:block}.actions{display:grid;grid-template-columns:1fr 1fr;margin:0 0 10px}.btn{padding:11px 12px}.btn small{display:none}.metrics{grid-template-columns:1fr 1fr}.herofoot{flex-direction:column;align-items:flex-start}.grid{grid-template-columns:repeat(2,1fr)}.ispgrid{grid-template-columns:1fr}.ispnote{display:none}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{animation-duration:.001ms!important;animation-iteration-count:1!important;transition-duration:.001ms!important}}
'''

js = r'''(function(){
"use strict";
var $=function(id){return document.getElementById(id);};
var host=location.hostname;
var state={armed:0,auto:0,overlay:1,sd:0,people:0,model:0,count:0,test:0,files:[],total:0,filter:"all",rec:0,recCam:1,recReport:0,sched:0,schedDaily:0,schedWindow:0,schedText:"--",schedCam:1,schedFilled:0,recLeft:0,recLeftAt:0};
function toast(msg){var t=$("toast");t.textContent=msg;t.classList.add("show");clearTimeout(t._h);t._h=setTimeout(function(){t.classList.remove("show");},2600);}
function get(url){return fetch(url,{cache:"no-store"}).then(function(r){return r.text();});}
function kv(text){var o={};(text||"").trim().split(/\s+/).forEach(function(x){var i=x.indexOf("=");if(i>0)o[x.slice(0,i)]=x.slice(i+1);});return o;}
function ampkv(text){var o={};(text||"").trim().split("&").forEach(function(x){var i=x.indexOf("=");if(i>0)o[x.slice(0,i)]=x.slice(i+1);});return o;}
function esc(s){return String(s).replace(/[&<>"']/g,function(c){return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c];});}
function setNum(id,v){var el=$(id);if(!el)return;if(el.textContent!==String(v)){el.textContent=v;el.classList.remove("bump");void el.offsetWidth;el.classList.add("bump");}}
function tick(){var d=new Date();$("clock").textContent=d.toLocaleTimeString("zh-CN",{hour12:false});}
function syncTime(){var d=new Date();get("/api/time?t="+Math.floor(d.getTime()/1000)+"&tz="+(-d.getTimezoneOffset())).catch(function(){});}
function setArmUI(a){state.armed=a;document.body.classList.toggle("armed",!!a);var p=$("armpill");p.classList.toggle("on",!!a);p.classList.toggle("off",!a);p.querySelector("span").textContent=a?"布防中":"已撤防";var b=$("armbtn");b.classList.toggle("on",!!a);b.querySelector("b").textContent=a?"撤防":"布防";b.querySelector("small").textContent=a?"停止存证与语音告警":"自动存证 + 语音告警";}
function setOverlayUI(on){state.overlay=on;var b=$("ovlbtn");if(b){b.querySelector("b").textContent=on?"隐藏叠加":"显示叠加";b.querySelector("small").textContent=on?"检测仍在运行":"叠加已隐藏 · 检测继续";var ic=b.querySelector(".ico");if(ic)ic.textContent=on?"◉":"○";b.classList.toggle("off",!on);}var rl=$("rulelay");if(rl)rl.classList.toggle("off",!on);}
function loadStatus(){get("/api/status").then(function(t){var o=kv(t);if(o.armed!==undefined)setArmUI(+o.armed);if(o.overlay!==undefined)setOverlayUI(+o.overlay);if(o.people!==undefined)setNum("mpeople",o.people);if(o.count!==undefined)setNum("mevents",o.count);if(o.model!==undefined)$("mmodel").textContent=(o.model&&+o.model)?"#"+o.model:"--";if(o.conf!==undefined){var mn=$("mmodelnote");if(mn)mn.textContent="NBG case id"+((+o.conf)?" · "+o.conf+"%":" · --");}if(o.sd!==undefined){var el=$("msd");el.textContent=+o.sd?"正常":"未就绪";el.className=+o.sd?"good":"bad";}if(o.auto!==undefined)state.auto=+o.auto;if(o.test!==undefined)setTestUI(+o.test);}).catch(function(){});}
function loadOverlay(){get("/api/overlay").then(function(t){var o=kv(t);if(o.overlay!==undefined)setOverlayUI(+o.overlay);}).catch(function(){});}
function toggleArm(){var on=state.armed?0:1;get("/api/arm?on="+on).then(function(t){var o=kv(t);setArmUI(+o.armed);toast(on?"已布防，事件将自动存证并语音告警":"已撤防");loadStatus();}).catch(function(){toast("布防失败");});}
function snap(){toast("抓拍中...");get("/api/snap?save=1&cam="+state.recCam).then(function(t){var o=kv(t);if(o.ok==="1"){toast("已保存 "+(o.size?(Math.round(+o.size/1024)+" KB"):""));loadEvents();loadGallery();loadStatus();}else{toast("抓拍失败："+(t.trim()||"未知错误"));}}).catch(function(){toast("抓拍失败");});}
function toggleOverlay(){var on=state.overlay?0:1;get("/api/overlay?on="+on).then(function(t){var o=kv(t);setOverlayUI(+o.overlay);toast(on?"已显示 AI 叠加":"已隐藏 AI 叠加，检测继续");}).catch(function(){toast("操作失败");});}
function modeInfo(k,m){if(k==="2")return "手动抓拍";if(k==="0"){return m==="1"?"左→右":m==="2"?"右→左":m==="3"?"双向绊线":"绊线"}if(k==="1"){return m==="4"?"进入区域":m==="5"?"离开区域":m==="6"?"区域入侵":"周界"}return "事件";}
function loadEvents(){get("/api/events?n=6").then(function(t){var lines=(t||"").trim().split("\n");var head=kv(lines[0]||"");if(head.armed!==undefined)setArmUI(+head.armed);if(head.sd!==undefined){var el=$("msd");el.textContent=+head.sd?"正常":"未就绪";el.className=+head.sd?"good":"bad";}if(head.count!==undefined)setNum("mevents",head.count);if(head.auto!==undefined)state.auto=+head.auto;if(head.test!==undefined)setTestUI(+head.test);var html="";for(var i=1;i<lines.length;i++){var line=lines[i].trim();if(!line)continue;var o=kv(line);var k=o.kind||"2";var mi=modeInfo(k,o.mode||"0");var cls=k==="0"?"k0":k==="1"?"k1":"k2";var ok=o.ok==="1";var fpath=o.file||"";var name=fpath.split("/").pop();var cpath=o.clean||"";var cname=cpath.split("/").pop();var cleanOk=o.clean_ok==="1";var vpath=o.clip||"";var clip=vpath.split("/").pop();var clipOk=o.clip_ok==="1";var tm=line.match(/time=([0-9-]+ [0-9:]+)/);var tv=tm?tm[1]:"--";var meta=[];if(k!=="2"){meta.push("置信度 "+(o.conf?o.conf+"%":"--"));meta.push((o.count||"0")+" 人");}meta.push(ok?"已存证"+(o.size?" · "+Math.round(+o.size/1024)+" KB":""):"未保存");if(k!=="2")meta.push(cleanOk?"原图✓":"原图缺失");meta.push(clipOk?"视频✓":(state.rec?"录像中":"视频缺失"));var lab=o.label!==undefined?+o.label:-1;var labs=(k!=="2")?'<button type="button" class="lab ok'+(lab===1?" on":"")+'" data-seq="'+o.seq+'" data-v="1" title="正确">✓</button><button type="button" class="lab no'+(lab===0?" on":"")+'" data-seq="'+o.seq+'" data-v="0" title="误报">✗</button>':"";html+='<div class="ev '+cls+'"><span class="dot"></span><div><div class="t">'+esc(tv)+" · "+esc(mi)+'</div><div class="m">'+esc(meta.join(" · "))+'</div></div><div class="r">'+labs+(ok&&name?'<a href="/api/file?name='+encodeURIComponent(fpath)+'" target="_blank">AI 图</a>':"")+((cname&&cleanOk)?'<a href="/api/file?name='+encodeURIComponent(cpath)+'" target="_blank">原图</a>':"")+((clip&&clipOk)?'<a href="/clip?name='+encodeURIComponent(vpath)+'" target="_blank" rel="opener">视频</a>':"")+"</div></div>";}$("events").innerHTML=html||'<div class="empty">暂无事件</div>';$("evsum").textContent="LIVE · 最近 "+Math.max(0,lines.length-1);}).catch(function(){});}
function filteredFiles(){return state.files.filter(function(f){if(state.filter==="left")return f.name.indexOf("left/")===0;if(state.filter==="right")return f.name.indexOf("right/")===0;if(state.filter==="manual")return f.name.indexOf("snap_")>=0;if(state.filter==="trip")return f.name.indexOf("_k0_")>=0;if(state.filter==="peri")return f.name.indexOf("_k1_")>=0;if(state.filter==="img")return /\.jpg$/i.test(f.name);if(state.filter==="vid")return /\.avi$/i.test(f.name);return true;});}
function renderGallery(){var files=filteredFiles().slice(0,12);var html="";files.forEach(function(f){var isC=/\.avi$/i.test(f.name);var u=(isC?"/clip?name=":"/api/file?name=")+encodeURIComponent(f.name);var body=isC?'<span class="play">▶</span>':'<img loading="lazy" src="'+u+'" alt="">';var bname=f.name.split("/").pop();var ctag=f.name.indexOf("left/")===0?"左":(f.name.indexOf("right/")===0?"右":"旧");var tag='<span class="tag">'+ctag+(isC?" · ▶ 视频":" · 图片")+'</span>';var tgt=isC?' target="_blank" rel="opener"':' target="_blank"';html+='<div class="gi '+(isC?"vid":"pic")+'"><a href="'+u+'"'+tgt+'>'+body+'</a>'+tag+'<button type="button" class="x" data-n="'+esc(f.name)+'">×</button><span class="nm">'+esc(bname)+"</span></div>";});$("gallery").innerHTML=html||'<div class="empty">暂无照片</div>';$("galtxt").textContent=state.sd?("共 "+state.total+" 个文件 · 显示最近 "+files.length+" 项"):"SD 卡未就绪";}
function loadGallery(){get("/api/files?n=48").then(function(t){var lines=(t||"").trim().split("\n");var head=kv(lines[0]||"");state.sd=head.sd==="1";state.total=+head.total||0;state.files=[];for(var i=1;i<lines.length;i++){var p=lines[i].trim().split(" ");if(p.length<2)continue;state.files.push({name:p[0],size:+p[1]||0});}renderGallery();}).catch(function(){$("galtxt").textContent="读取失败";});}
function deleteFile(name){if(!confirm("删除 "+name+" ?"))return;get("/api/delete?name="+encodeURIComponent(name)).then(function(t){toast(t.indexOf("ok=1")>=0?"已删除":"删除失败");loadGallery();loadEvents();loadStatus();}).catch(function(){toast("删除失败");});}
function cleanFiles(){if(!confirm("只保留最近 100 个文件？"))return;get("/api/clean?keep=100").then(function(t){toast(t.indexOf("ok=1")>=0?"已清理":"清理失败");loadGallery();loadEvents();});}
function showSpace(){get("/api/space").then(function(t){var o=kv(t);if(o.ok==="1"){var f=+o.freekb||0,tot=+o.totalkb||0,u=Math.max(0,tot-f);toast("已用 "+(u<10240?Math.round(u/1024)+" MB":(u/1048576).toFixed(1)+" GB")+" · 剩余 "+(f/1048576).toFixed(2)+" GB / 共 "+(tot/1048576).toFixed(2)+" GB");}else toast("容量查询失败");}).catch(function(){toast("容量查询失败");});}
function remount(){if(!confirm("重新挂载 SD 卡？会中断正在进行的录像"))return;toast("正在重新挂载...");get("/api/sd/remount?force=1").then(function(t){var o=kv((t||"").split("\n")[0]||"");if(o.ok==="1"){toast("SD 已重挂");}else{toast("重挂失败："+(o.err==="busy"?"文件占用":o.err==="recording"?"录像未停":"挂载失败"));}loadGallery();loadEvents();loadStatus();}).catch(function(){toast("重挂失败");});}
function setTestUI(on){state.test=on?1:0;var b=$("testbtn");if(b){b.textContent="测试模式："+(on?"开":"关");b.classList.toggle("on",!!on);}}
function toggleTest(){var on=state.test?0:1;get("/api/test?on="+on).then(function(t){var o=kv(t);setTestUI(+o.test);toast(on?"测试模式：冷却 300ms，写 detect.csv":"测试模式关闭");}).catch(function(){toast("切换失败");});}
function sdSelfTest(){toast("SD 自检中...");get("/api/sd/selftest").then(function(t){var o=kv(t);toast(o.ok==="1"?"SD 自检通过（64KB 读写校验）":"SD 自检失败："+(t.trim()||"未知错误"));loadGallery();loadStatus();}).catch(function(){toast("SD 自检失败");});}
function labelEvent(seq,v){get("/api/label?seq="+seq+"&v="+v).then(function(t){toast(t.indexOf("ok=1")>=0?"已标注":"标注失败");loadEvents();}).catch(function(){toast("标注失败");});}
function missEvent(){if(!confirm("记录一次漏报？"))return;get("/api/label?fn=1").then(function(t){toast(t.indexOf("ok=1")>=0?"漏报已记录":"记录失败");}).catch(function(){toast("记录失败");});}
function paintCam(m){document.querySelectorAll("#lcdseg button").forEach(function(b){b.classList.toggle("on",+b.dataset.v===m);});$("lcdhint").textContent=m===0?"LCD：AI 叠加 · 检测框可见":"LCD：干净原图 · AI 仍在后台运行";}
function loadCam(){get("/api/cam").then(function(t){var o=kv(t);if(o.mode!==undefined)paintCam(+o.mode);}).catch(function(){});}
function recPaint(){
  var b=$("recbtn");
  if(state.rec){b.textContent="■ 停止录制";b.classList.add("recording");}
  else{b.textContent="● 录制 30 秒";b.classList.remove("recording");}
  document.querySelectorAll("#recseg button").forEach(function(x){x.classList.toggle("on",+x.dataset.cam===state.recCam);});
  var sl=$("schedlabel");if(sl)sl.textContent=(state.recCam===1)?"定时录制（到点自动 AI 追踪）":"定时录制（到点录制原画）";
  $("rec_sub").textContent=recSubText();
  var rl=$("reportlink");
  if(state.recReport>0){rl.style.display="";rl.href="/report?id="+state.recReport;rl.textContent="查看报告 #"+state.recReport+" →";}
  else{rl.style.display="none";}
  var hint=$("sched_hint");
  if(state.sched){hint.textContent="定时："+(state.schedDaily?"每天 ":"")+state.schedText+" · "+(state.schedCam?"右摄 AI":"左摄 原画")+(state.schedWindow?" · 时间窗内":" · 等待开始");}
  else{hint.textContent="未设置定时";}
}
function loadRec(){
  get("/api/rec").then(function(t){
    var ls=(t||"").trim().split("\n");var o={};
    ls.forEach(function(l){var x=kv(l);for(var k in x)o[k]=x[k];});
    state.rec=o.rec==="1";state.recCam=(o.cam!==undefined&&+o.cam>=0)?+o.cam:state.recCam;state.recLeft=(state.rec&&o.left!==undefined)?+o.left:0;if(state.rec&&o.left!==undefined)state.recLeftAt=Date.now();
    state.sched=o.sched==="1";state.schedDaily=o.daily==="1";state.schedWindow=o.window==="1";if(o.scam!==undefined)state.schedCam=+o.scam;
    if(o.report!==undefined&&+o.report>0)state.recReport=+o.report;
    if(o.start!==undefined&&o.end!==undefined){state.schedText=fmtMin(o.start)+"–"+fmtMin(o.end);}
    if(state.sched&&!state.schedFilled){
      if(o.date&&+o.date>0){var d=String(o.date);$("sched_date").value=d.slice(0,4)+"-"+d.slice(4,6)+"-"+d.slice(6,8);}
      if(o.start!==undefined){$("sched_start").value=fmtMin(o.start);$("sched_end").value=fmtMin(o.end);}
      $("sched_daily").checked=state.schedDaily;state.schedFilled=1;
    }
    if(!state.sched)state.schedFilled=0;
    recPaint();
  }).catch(function(){});
}
function fmtMin(m){m=+m;var h=Math.floor(m/60),x=m%60;return (h<10?"0":"")+h+":"+(x<10?"0":"")+x;}
function fmtLeft(s){s=Math.max(0,+s||0);var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),x=s%60;return (h>0?h+":":"")+(m<10?"0":"")+m+":"+(x<10?"0":"")+x;}
function recLeftNow(){if(!state.rec)return 0;var e=state.recLeftAt?Math.floor((Date.now()-state.recLeftAt)/1000):0;return Math.max(0,state.recLeft-e);}
function recSubText(){if(!state.rec)return "未录制";var left=recLeftNow();return "录制中 · 摄像头 "+(state.recCam+1)+(left>0?" · 剩余 "+fmtLeft(left):"");}
function recTick(){if(state.rec)$("rec_sub").textContent=recSubText();}
function toggleRec(){
  var on=state.rec?0:1;
  var q="/api/rec?on="+on+"&cam="+state.recCam+(on?"&sec=30":"");
  get(q).then(function(t){var o=kv((t||"").split("\n")[0]||"");if(o.ok==="1"){toast(on?"开始录制":"已停止录制");}else{toast("录制失败："+(o.err||"未知"));}loadRec();[150,450,1000].forEach(function(d){setTimeout(loadRec,d);});}).catch(function(){toast("录制操作失败");});
}
function setSched(){
  var date=($("sched_date").value||"").replace(/-/g,"");
  var st=($("sched_start").value||"").split(":"),en=($("sched_end").value||"").split(":");
  if(st.length<2||en.length<2){toast("请填起止时间");return;}
  var sm=+st[0]*60+ +st[1],em=+en[0]*60+ +en[1];
  var daily=$("sched_daily").checked?1:0;
  if(sm>=em){toast("结束时间要晚于开始");return;}
  if(!daily&&date.length!==8){toast("请选择日期");return;}
  get("/api/rec/sched?enabled=1&daily="+daily+"&date="+date+"&start="+sm+"&end="+em+"&cam="+state.recCam).then(function(t){var o=kv((t||"").split("\n")[0]||"");if(o.ok==="1"){toast("定时已启用");}else{toast("定时设置失败");}loadRec();}).catch(function(){toast("定时设置失败");});
}
function clearSched(){get("/api/rec/sched?enabled=0").then(function(){toast("定时已取消");loadRec();}).catch(function(){});}
var ISP_NUM=[["br","亮度",-100,100,0],["ct","对比度",-100,100,0],["sat","饱和度",-100,100,0],["sp","锐度",-100,100,0],["ev","曝光补偿",-8,8,0],["rg","红增益",0,100,50],["bg","蓝增益",0,100,50]];
var ISP_SEL=[["dn","日夜模式",["彩色","黑白"]],["wb","白平衡",["自动","锁定","白炽灯","暖光灯","自然光","日光灯"]]];
var ispT={};
function ispPaint(o){ISP_NUM.forEach(function(c){var k=c[0];if(o[k]===undefined)return;var r=$("r_"+k);if(r)r.value=o[k];$("v_"+k).textContent=o[k];});ISP_SEL.forEach(function(c){var k=c[0];if(o[k]===undefined)return;var s=$("s_"+k);if(s)s.value=o[k];});}
function ispApply(k,v){get("/api/isp?k="+k+"&v="+v).then(function(t){ispPaint(ampkv(t));var m=$("ispmsg");if(m)m.textContent="ISP · G1 主摄 · 已下发";}).catch(function(){});}
function ispSend(k,v){clearTimeout(ispT[k]);ispT[k]=setTimeout(function(){ispApply(k,v);},140);}
function ispBuild(){var h="";ISP_NUM.forEach(function(c){h+='<label class="ispctl"><div class="row"><b>'+c[1]+'</b><span id="v_'+c[0]+'">'+c[4]+'</span></div><input type="range" id="r_'+c[0]+'" min="'+c[2]+'" max="'+c[3]+'" step="1" value="'+c[4]+'" data-k="'+c[0]+'"></label>';});ISP_SEL.forEach(function(c){var o="";c[2].forEach(function(t,i){o+='<option value="'+i+'">'+t+"</option>";});h+='<label class="ispctl"><div class="row"><b>'+c[1]+'</b><span>选择</span></div><select id="s_'+c[0]+'" data-k="'+c[0]+'">'+o+"</select></label>";});$("ispgrid").innerHTML=h;$("ispgrid").addEventListener("input",function(e){var el=e.target;if(el.type!=="range")return;var k=el.getAttribute("data-k");if(!k)return;var v=+el.value;$("v_"+k).textContent=v;ispSend(k,v);});$("ispgrid").addEventListener("change",function(e){var el=e.target;if(el.tagName!=="SELECT")return;var k=el.getAttribute("data-k");if(k)ispApply(k,+el.value);});$("ispmsg").textContent="ISP · G1 主摄";}
function loadIsp(){get("/api/isp").then(function(t){ispPaint(ampkv(t));}).catch(function(){$("ispmsg").textContent="ISP 未就绪";});}
function ispReset(){if(!confirm("恢复 ISP 默认参数？"))return;get("/api/isp?br=0&ct=0&sat=0&sp=0&ev=0&dn=0&wb=0&rg=50&bg=50").then(function(t){ispPaint(ampkv(t));toast("ISP 已恢复默认");}).catch(function(){toast("恢复失败");});}

/* ---- 安防统计（主页面） ---- */
var stCur=[],stFrom=[],stTarget=[],stRaf=0,stT0=0,stHover=-1,stGeo=null;
function stInit(){
  for(var i=0;i<24;i++){stCur.push({i:0,o:0});stFrom.push({i:0,o:0});stTarget.push({i:0,o:0});}
  var c=$("stch");
  if(c){c.addEventListener("mousemove",stMove);c.addEventListener("mouseleave",function(){stHover=-1;var t=$("st_tip");if(t)t.style.display="none";stDraw();});}
  window.addEventListener("resize",function(){stDraw();});
  var tb=$("themebtn");if(tb)tb.addEventListener("click",function(){setTimeout(stDraw,60);});
}
function stKv(t){var o={};(t||"").trim().split(/[&\s]+/).forEach(function(x){var i=x.indexOf("=");if(i>0)o[x.slice(0,i)]=x.slice(i+1);});return o;}
function stDwell(ms){ms=+ms||0;if(!ms)return "0 秒";var s=Math.round(ms/1000);if(s<60)return s+" 秒";var m=Math.floor(s/60);return m+" 分 "+(s%60)+" 秒";}
function stLoad(){fetch("/api/stats?t="+Date.now(),{cache:"no-store"}).then(function(r){return r.text();}).then(stPaint).catch(function(){var e=$("st_sub");if(e)e.textContent="板子没有响应";});}
function stPaint(t){
  var o=stKv(t),hours=[];
  (t||"").split("\n").forEach(function(l){var m=l.match(/^h=(\d+) in=(\d+) out=(\d+)/);if(m)hours[+m[1]]={i:+m[2],o:+m[3]};});
  var set=function(id,v){var e=$(id);if(e)e.textContent=v;};
  set("st_in",o.in_today||"0");set("st_out",o.out_today||"0");
  set("st_ain",o.area_in_today||"0");set("st_aout",o.area_out_today||"0");
  set("st_dwell",stDwell(o.max_dwell_ms));set("st_max",o.max_people||"0");
  var dt=(o.date||"").replace(/^(\d{4})(\d{2})(\d{2})$/,"$1-$2-$3");
  set("st_sub",(o.synced==="1"?"日期 "+(dt||"--"):"时间未同步")+" · 每 5 秒刷新");
  var k=o.last_kind==="0"?"绊线":o.last_kind==="1"?"周界":"--";
  set("st_last","最近事件："+(o.last_epoch&&o.last_epoch!=="0"?k+" · "+new Date(+o.last_epoch*1000).toLocaleString():"--"));
  var changed=false,peak=-1,peakv=0;
  for(var i=0;i<24;i++){var h=hours[i]||{i:0,o:0};if(h.i!==stTarget[i].i||h.o!==stTarget[i].o)changed=true;stFrom[i]={i:stCur[i].i,o:stCur[i].o};stTarget[i]={i:h.i,o:h.o};var v=h.i+h.o;if(v>peakv){peakv=v;peak=i;}}
  set("st_peak",peakv>0?("最忙时段 "+peak+":00 · 进入 "+stTarget[peak].i+" · 离开 "+stTarget[peak].o):"");
  if(changed){stT0=0;if(!stRaf)stRaf=requestAnimationFrame(stStep);}else{stDraw();}
}
function stStep(ts){
  if(!stT0)stT0=ts;
  var p=Math.min(1,(ts-stT0)/450),e=1-Math.pow(1-p,3);
  for(var i=0;i<24;i++){stCur[i].i=stFrom[i].i+(stTarget[i].i-stFrom[i].i)*e;stCur[i].o=stFrom[i].o+(stTarget[i].o-stFrom[i].o)*e;}
  stDraw();
  if(p<1){stRaf=requestAnimationFrame(stStep);}else{stRaf=0;for(var j=0;j<24;j++){stCur[j].i=stTarget[j].i;stCur[j].o=stTarget[j].o;}stDraw();}
}
function stBar(g,x,y,w,h,r){if(h<=0)return;r=Math.min(r,w/2,h);g.beginPath();g.moveTo(x,y+h);g.lineTo(x,y+r);g.quadraticCurveTo(x,y,x+r,y);g.lineTo(x+w-r,y);g.quadraticCurveTo(x+w,y,x+w,y+r);g.lineTo(x+w,y+h);g.closePath();g.fill();}
function stDraw(){
  var c=$("stch");if(!c)return;
  var d=window.devicePixelRatio||1,w=c.clientWidth||900,h=230;
  if(c.width!==Math.round(w*d)||c.height!==Math.round(h*d)){c.width=Math.round(w*d);c.height=Math.round(h*d);}
  var g=c.getContext("2d");g.setTransform(d,0,0,d,0,0);
  var cs=getComputedStyle(document.documentElement);
  var ac=cs.getPropertyValue("--accent").trim(),ac2=cs.getPropertyValue("--accent2").trim();
  var c2=cs.getPropertyValue("--chart2").trim()||"#7c6bd6";
  var dim=cs.getPropertyValue("--dim").trim(),line=cs.getPropertyValue("--line").trim();
  var dark=document.documentElement.getAttribute("data-theme")==="dark";
  var padL=34,padR=12,padT=16,padB=24;
  var maxv=0,total=0;
  for(var i=0;i<24;i++){maxv=Math.max(maxv,stCur[i].i,stCur[i].o);total+=stCur[i].i+stCur[i].o;}
  var step=Math.max(1,Math.ceil(maxv/4)),top=step*4;
  var pw=w-padL-padR,ph=h-padT-padB;
  var X=function(k){return padL+pw*(k+0.5)/24;};
  var Y=function(v){return padT+ph*(1-v/top);};
  stGeo={w:w,padL:padL,pw:pw};
  g.clearRect(0,0,w,h);
  var nowH=new Date().getHours();
  g.fillStyle=dark?"rgba(199,182,255,.08)":"rgba(240,180,41,.10)";
  g.fillRect(X(nowH)-pw/48,padT,pw/24,ph);
  if(stHover>=0){g.fillStyle=dark?"rgba(255,255,255,.05)":"rgba(120,94,36,.06)";g.fillRect(X(stHover)-pw/48,padT,pw/24,ph);}
  g.font="11px system-ui,sans-serif";g.textAlign="right";g.lineWidth=1;
  for(var q=0;q<=4;q++){var gv=step*q,py=Y(gv);g.strokeStyle=line;g.beginPath();g.moveTo(padL,py);g.lineTo(w-padR,py);g.stroke();g.fillStyle=dim;g.fillText(String(gv),padL-6,py+4);}
  g.textAlign="center";g.fillStyle=dim;
  for(var x3=0;x3<24;x3+=3){g.fillText(x3+":00",X(x3),h-8);}
  var g1=g.createLinearGradient(0,padT,0,padT+ph);g1.addColorStop(0,ac2);g1.addColorStop(1,ac);
  var g2=g.createLinearGradient(0,padT,0,padT+ph);g2.addColorStop(0,c2+"e6");g2.addColorStop(1,c2+"88");
  var bw=Math.max(3,(pw/24)*0.26);
  g.font="10px system-ui,sans-serif";
  for(var b=0;b<24;b++){
    var x=X(b),ci=stCur[b].i,co=stCur[b].o;
    if(ci>0.02){var hi=ph*ci/top;g.fillStyle=g1;stBar(g,x-bw-1.5,padT+ph-hi,bw,hi,4);g.fillStyle=dim;g.fillText(Math.round(ci),x-bw/2-1.5,padT+ph-hi-5);}
    if(co>0.02){var ho=ph*co/top;g.fillStyle=g2;stBar(g,x+1.5,padT+ph-ho,bw,ho,4);g.fillStyle=dim;g.fillText(Math.round(co),x+bw/2+1.5,padT+ph-ho-5);}
  }
  if(total<=0.02){g.fillStyle=dim;g.font="13px system-ui,sans-serif";g.fillText("暂无统计数据",w/2,h/2);}
}
function stMove(e){
  if(!stGeo)return;
  var c=$("stch"),r=c.getBoundingClientRect();
  var sx=(e.clientX-r.left)*(stGeo.w/r.width);
  var k=Math.floor((sx-stGeo.padL)/(stGeo.pw/24));
  if(k<0||k>23){stHover=-1;var t0=$("st_tip");if(t0)t0.style.display="none";stDraw();return;}
  if(stHover!==k){stHover=k;stDraw();}
  var t=$("st_tip");if(!t)return;
  t.textContent=k+":00 · 进入 "+Math.round(stCur[k].i)+" · 离开 "+Math.round(stCur[k].o);
  t.style.display="block";
  var box=c.parentNode.getBoundingClientRect();
  t.style.left=(e.clientX-box.left+12)+"px";t.style.top=(e.clientY-box.top+10)+"px";
}
function stReset(){if(!confirm("清零今日统计？累计值保留。"))return;fetch("/api/stats?reset=1&t="+Date.now(),{cache:"no-store"}).then(function(){stLoad();}).catch(function(){});}
var termSince=-1,termBusy=false,termPolling=false,termHistory=[],termHistIdx=0;
function termClean(s){return String(s).replace(/\u001b\[[0-9;]*m/g,"").replace(/\r\n/g,"\n").replace(/\r/g,"\n");}
function termAppend(s){var el=$("termout");if(!el||!s)return;var stick=el.scrollTop+el.clientHeight>=el.scrollHeight-48;el.textContent+=s;if(el.textContent.length>40000)el.textContent=el.textContent.slice(-30000);if(stick)el.scrollTop=el.scrollHeight;}
function termState(busy){termBusy=!!busy;var el=$("termstate");if(el){el.classList.toggle("busy",!!busy);el.innerHTML="<i></i>"+(busy?"执行中":"空闲");}}
function termPoll(){if(termPolling)return;termPolling=true;fetch("/api/term/out?since="+termSince,{cache:"no-store"}).then(function(r){var ns=r.headers.get("X-Term-Since"),lost=r.headers.get("X-Term-Lost"),busy=r.headers.get("X-Term-Busy");if(ns!==null)termSince=parseInt(ns,10);if(busy!==null)termState(busy==="1");return r.text().then(function(t){if(lost==="1")termAppend("\n[输出过快，部分内容已丢弃]\n");if(t)termAppend(termClean(t));});}).catch(function(){termState(false);}).then(function(){termPolling=false;setTimeout(termPoll,500);});}
function termIsDanger(cmd){return /^\s*(reboot|reset|poweroff|shutdown|fal|sf|mkfs|rm|rmdir|mv|cp|mtest|memtrace)\b/i.test(cmd);}
function termSubmit(cmd,force){cmd=(cmd||"").trim();if(!cmd)return;var enc=encodeURIComponent(cmd);if(enc.length>420){toast("命令太长，最多约 120 字符");return;}if(termBusy){toast("上一条命令还在执行");return;}if(!force&&termIsDanger(cmd)){if(!confirm("命令可能修改设备状态：\n"+cmd+"\n\n继续执行？")){termAppend("\nmsh > "+cmd+"\n[已取消]\n");return;}force=1;}termHistory.push(cmd);termHistIdx=termHistory.length;termAppend("\nmsh > "+cmd+"\n");$("terminput").value="";termState(true);fetch("/api/term?cmd="+enc+(force?"&force=1":""),{cache:"no-store"}).then(function(r){return r.text();}).then(function(t){var s=(t||"").trim();if(s==="confirm=1"){if(confirm("板端要求确认：\n"+cmd+"\n\n继续执行？")){termSubmit(cmd,1);}else{termAppend("[已取消]\n");termState(false);}}else if(s==="busy=1"){termAppend("[设备忙，命令未执行]\n");termState(false);toast("设备忙");}else if(s.indexOf("ok=1")!==0){termAppend("[提交失败] "+(s||"未知错误")+"\n");termState(false);}}).catch(function(){termAppend("[提交失败] 板子没有响应\n");termState(false);});}
function initTerm(){$("termform").addEventListener("submit",function(e){e.preventDefault();termSubmit($("terminput").value);});$("terminput").addEventListener("keydown",function(e){if(e.key==="ArrowUp"){if(termHistIdx>0){termHistIdx--;$("terminput").value=termHistory[termHistIdx]||"";e.preventDefault();}}else if(e.key==="ArrowDown"){if(termHistIdx<termHistory.length-1){termHistIdx++;$("terminput").value=termHistory[termHistIdx]||"";}else{termHistIdx=termHistory.length;$("terminput").value="";}e.preventDefault();}});$("termclear").onclick=function(){var el=$("termout");if(el)el.textContent="";};document.querySelectorAll(".termquick [data-cmd]").forEach(function(b){b.onclick=function(){termSubmit(b.getAttribute("data-cmd"));};});termSince=-1;termState(false);termPoll();}
function init(){$("pv0").src="http://"+host+":1111/";$("pv1").src="http://"+host+":1112/";$("host").textContent=host;tick();setInterval(tick,1000);syncTime();setInterval(syncTime,600000);setInterval(function(){if(!document.hidden)loadStatus();},3000);setInterval(function(){if(!document.hidden)loadEvents();},3000);setInterval(function(){if(!document.hidden)loadRec();},3000);setInterval(function(){if(!document.hidden)recTick();},1000);document.addEventListener("visibilitychange",function(){if(!document.hidden){loadStatus();loadEvents();loadRec();}});loadRec();$("armbtn").onclick=toggleArm;$("snapbtn").onclick=snap;$("recbtn").onclick=toggleRec;$("schedbtn").onclick=setSched;$("schedclr").onclick=clearSched;$("recseg").addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;state.recCam=+b.dataset.cam;recPaint();});$("ovlbtn").onclick=toggleOverlay;$("galrf").onclick=loadGallery;$("galclean").onclick=cleanFiles;$("galspace").onclick=showSpace;$("galmount").onclick=remount;$("lcdseg").addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;var m=+b.dataset.v;paintCam(m);get("/api/cam?m="+m).then(function(t){var o=kv(t);if(o.mode!==undefined)paintCam(+o.mode);});});$("filter").addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;state.filter=b.dataset.f;document.querySelectorAll("#filter button").forEach(function(x){x.classList.toggle("on",x===b);});renderGallery();});$("gallery").addEventListener("click",function(e){var b=e.target.closest(".x");if(!b)return;e.preventDefault();deleteFile(b.dataset.n);});$("testbtn").onclick=toggleTest;$("sdtest").onclick=sdSelfTest;$("missbtn").onclick=missEvent;$("events").addEventListener("click",function(e){var b=e.target.closest(".lab");if(!b)return;e.preventDefault();labelEvent(b.dataset.seq,+b.dataset.v);});loadStatus();loadOverlay();loadEvents();loadGallery();loadCam();stInit();stLoad();setInterval(function(){if(!document.hidden)stLoad();},5000);$("st_reset").onclick=stReset;initTerm();}
init();
})();
'''

css3 = r'''
/* ---- 设备状态卡片 ---- */
.syscard{grid-column:1/-1;margin:0}
.syshead{display:flex;align-items:center;gap:8px}
.sysgrid{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:10px;padding:2px 16px 16px}
.sysitem{position:relative;padding:12px 14px 13px;border:1px solid var(--line);border-radius:15px;background:var(--panel2);overflow:hidden;transition:transform .2s,box-shadow .25s,border-color .25s}
.sysitem::after{content:"";position:absolute;left:0;top:0;bottom:0;width:3px;background:linear-gradient(180deg,var(--accent),var(--accent2));opacity:.55}
.sysitem:hover{transform:translateY(-2px);box-shadow:0 12px 26px rgba(120,94,36,.12);border-color:#e2d3b2}
.syslabel{font-size:10px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);font-weight:700}
.sysval{display:flex;align-items:baseline;gap:3px;margin:4px 0 0;min-width:0}
.sysval b{font-size:26px;line-height:1.1;font-variant-numeric:tabular-nums;color:var(--text);font-weight:800;transition:color .3s}
.sysval span{font-size:11px;color:var(--dim);font-weight:700}
.sysval.sm b{font-size:14px;line-height:1.35;word-break:break-all}
.sysval.ip b{display:block;font-size:16px;line-height:1.35;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.sysnote{margin-top:6px;font-size:10px;color:var(--dim2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bar{height:6px;border-radius:99px;background:var(--line2);overflow:hidden;margin-top:9px}
.bar i{display:block;height:100%;width:0;border-radius:99px;background:linear-gradient(90deg,var(--accent),var(--accent2));transition:width .5s ease,background .3s}
.bar i.warm{background:linear-gradient(90deg,#f0b429,#ef8a2b)}
.bar i.hot{background:linear-gradient(90deg,#ef8a2b,#e0554d)}
.bar.temp i{background:linear-gradient(90deg,#4fb477,#f0b429 55%,#e0554d)}
.sysval b.bad{color:var(--red)}
.sysval b.good{color:var(--green)}
.themebtn{width:34px;height:34px;flex:0 0 auto;border-radius:50%;border:1px solid var(--line);background:var(--panel);color:var(--ink);font:16px/1 ui-sans-serif,system-ui,sans-serif;display:flex;align-items:center;justify-content:center;cursor:pointer;box-shadow:0 4px 14px rgba(120,94,36,.08);transition:transform .25s,border-color .25s,background .25s}
.themebtn:hover{transform:rotate(-14deg);border-color:var(--accent)}
@media(max-width:1180px){.sysgrid{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media(max-width:700px){.sysgrid{grid-template-columns:repeat(2,minmax(0,1fr))}.sysval b{font-size:22px}}

/* ---- 夜间主题：默认浅色不动，只覆盖深色模式下需要变的硬编码颜色 ---- */
html[data-theme="dark"]{
--bg:#0c0f13;--panel:#151a21;--panel2:#1a2029;--line:#2a323e;--line2:#222934;
--text:#e9e4d8;--dim:#9b9486;--dim2:#6f6a60;
--accent:#c7b6ff;--accent2:#e3d9ff;--accent3:#241f36;--ink:#d9ccff;
--red:#ff7269;--redbg:#2b1717;--green:#63cf92;--greenbg:#12251a;
--chart2:#7aa2ff;
--shadow:0 10px 30px rgba(0,0,0,.5),0 2px 8px rgba(0,0,0,.35);
--shadow-lg:0 22px 60px rgba(0,0,0,.6);
color-scheme:dark;
}
html[data-theme="dark"] body{color-scheme:dark}
html[data-theme="dark"] .bg{background:linear-gradient(rgba(255,255,255,.025) 1px,transparent 1px),linear-gradient(90deg,rgba(255,255,255,.025) 1px,transparent 1px),linear-gradient(180deg,#0e1218 0%,#0b0e12 60%,#090c0f 100%);background-size:48px 48px,48px 48px,auto}
html[data-theme="dark"] .bg::before{background:radial-gradient(circle at 30% 30%,rgba(199,182,255,.10) 0%,transparent 66%)}
html[data-theme="dark"] .bg::after{background:radial-gradient(circle at 60% 60%,rgba(90,140,255,.08) 0%,transparent 66%)}
html[data-theme="dark"] .top{background:rgba(12,15,19,.84);box-shadow:0 1px 0 rgba(255,255,255,.04)}
html[data-theme="dark"] .mark{background:linear-gradient(145deg,#1e232b,#2a2338);border-color:#443a63;box-shadow:0 6px 18px rgba(0,0,0,.45),inset 0 1px 0 rgba(255,255,255,.06)}
html[data-theme="dark"] .mark i{border-color:rgba(199,182,255,.3)}
html[data-theme="dark"] .mark i:first-child{border-top-color:var(--accent);border-right-color:rgba(199,182,255,.65)}
html[data-theme="dark"] .mark i:nth-child(2){border-color:rgba(199,182,255,.45)}
html[data-theme="dark"] .mark i:nth-child(3){background:var(--accent);box-shadow:0 0 12px rgba(199,182,255,.8)}
html[data-theme="dark"] .brandtext h1{color:#f2ead6}
html[data-theme="dark"] .clock{color:var(--dim)}
html[data-theme="dark"] .pill.on{color:#ffb3ac;border-color:#5a2a28}
html[data-theme="dark"] .card{background:linear-gradient(180deg,#171c23,#141920);border-color:var(--line)}
html[data-theme="dark"] .card:hover{border-color:#3a3350}
html[data-theme="dark"] .screen{border-color:#2a323e}
html[data-theme="dark"] .screen .tag{color:#e3d9ff;background:rgba(15,19,25,.84);border-color:rgba(199,182,255,.45);box-shadow:0 4px 14px rgba(0,0,0,.4)}
html[data-theme="dark"] .screen.clean .tag{color:#d8d2c4;border-color:rgba(255,255,255,.24);background:rgba(15,19,25,.72)}
html[data-theme="dark"] .gi{background:#0f141a}
html[data-theme="dark"] .gi .tag{background:rgba(0,0,0,.6)}
html[data-theme="dark"] .gi.vid{border-color:#5a4a86}
html[data-theme="dark"] .gi.vid .tag{background:#3a2f5c;color:#e3d9ff}
html[data-theme="dark"] .seg{background:#10151b}
html[data-theme="dark"] .seg button.on{background:#232a33;box-shadow:0 2px 8px rgba(0,0,0,.4),inset 0 0 0 1px rgba(199,182,255,.35)}
html[data-theme="dark"] .btn{background:var(--panel)}
html[data-theme="dark"] .btn::after{background:linear-gradient(100deg,transparent,rgba(255,255,255,.07),transparent)}
html[data-theme="dark"] .btn:hover{border-color:#443a63;box-shadow:0 10px 24px rgba(0,0,0,.35)}
html[data-theme="dark"] .btn .ico{color:var(--accent)}
html[data-theme="dark"] .btn.arm{background:linear-gradient(180deg,#1a1726,#221d30);border-color:#443a63}
html[data-theme="dark"] .btn.arm.on{background:linear-gradient(180deg,#241413,#2c1816);border-color:#5a2a28}
html[data-theme="dark"] .btn.arm .ico{color:var(--accent)}
html[data-theme="dark"] .btn.ghost{background:var(--panel2)}
html[data-theme="dark"] .metric{background:var(--panel)}
html[data-theme="dark"] .metric b{color:#f4edda}
html[data-theme="dark"] .metric:hover{box-shadow:0 12px 24px rgba(0,0,0,.35)}
html[data-theme="dark"] .ispctl .row b{color:#d8d2c4}
html[data-theme="dark"] .ispctl .row span{color:var(--accent)}
html[data-theme="dark"] .ispctl input[type=range]::-webkit-slider-runnable-track{background:#2a323e;box-shadow:inset 0 1px 2px rgba(0,0,0,.4)}
html[data-theme="dark"] .ispctl input[type=range]::-moz-range-track{background:#2a323e}
html[data-theme="dark"] .ispctl input[type=range]::-webkit-slider-thumb{border-color:#0f1318}
html[data-theme="dark"] .ispctl input[type=range]::-moz-range-thumb{border-color:#0f1318}
html[data-theme="dark"] .ispctl select{background:var(--panel2);border-color:var(--line)}
html[data-theme="dark"] .termout{background:#0f1318;border-color:var(--line);color:#cfd8c8;box-shadow:inset 0 1px 3px rgba(0,0,0,.35)}
html[data-theme="dark"] .termline input{background:var(--panel);color:#f0eadc;border-color:var(--line)}
html[data-theme="dark"] .termprompt{color:var(--accent2)}
html[data-theme="dark"] .termstate.busy{color:var(--accent)}
html[data-theme="dark"] .cardhead h2{color:#d8d2c4}
html[data-theme="dark"] .ev:hover{background:#1c222b}
html[data-theme="dark"] .ev .t{color:#e6e0d2}
html[data-theme="dark"] .ev .lab{background:var(--panel2);border-color:var(--line)}
html[data-theme="dark"] .ev .lab.ok.on{border-color:#2e5a3f}
html[data-theme="dark"] .ev .lab.no.on{border-color:#6a3330}
html[data-theme="dark"] .ev a{color:var(--accent);border-color:#443a63;background:var(--panel2)}
html[data-theme="dark"] .ev a:hover{background:#2a2540;border-color:var(--accent)}
html[data-theme="dark"] .chips button{background:var(--panel2);border-color:var(--line)}
html[data-theme="dark"] .chips button:hover{color:var(--accent)}
html[data-theme="dark"] .gi{background:#1a2029;border-color:var(--line)}
html[data-theme="dark"] .mini{background:var(--panel2);color:#b9b2a4;border-color:var(--line)}
html[data-theme="dark"] .mini:hover{color:var(--ink);border-color:var(--accent);background:var(--accent3)}
html[data-theme="dark"] .toast{background:#1c222b;border-color:#443a63;color:#f0e6cf;box-shadow:0 18px 40px rgba(0,0,0,.5)}
html[data-theme="dark"] .links a{color:var(--accent)}
html[data-theme="dark"] .links a:hover{color:var(--accent2)}
html[data-theme="dark"] .sysitem:hover{border-color:#3a3350;box-shadow:0 12px 26px rgba(0,0,0,.35)}
html[data-theme="dark"] .top::after{background:linear-gradient(90deg,transparent,var(--accent),var(--accent2),var(--accent),transparent);background-size:200% 100%}
html[data-theme="dark"] .signal i{background:linear-gradient(180deg,var(--accent),var(--accent2))}
html[data-theme="dark"] .screen .tag i{background:var(--accent);box-shadow:0 0 0 3px rgba(199,182,255,.2)}
html[data-theme="dark"] .screen .scan{background:linear-gradient(180deg,transparent 0%,rgba(199,182,255,.10) 48%,transparent 55%);background-size:100% 10px}
html[data-theme="dark"] .chips button.on{border-color:#443a63}
html[data-theme="dark"] .mini.on{border-color:#443a63}
html[data-theme="dark"] .ev.k0 .dot{background:var(--accent);box-shadow:0 0 0 3px rgba(199,182,255,.16)}
html[data-theme="dark"] .ispctl select:focus,html[data-theme="dark"] .termline input:focus{box-shadow:0 0 0 3px rgba(199,182,255,.15)}
html[data-theme="dark"] .metric b.bump{animation:bumpd .4s ease}
@keyframes bumpd{0%{transform:scale(1)}45%{transform:scale(1.22);color:#c7b6ff}100%{transform:scale(1)}}

/* ---- 叠加开关的视觉状态 ---- */
.btn.off .ico{opacity:.4;filter:grayscale(1)}
.btn.off{border-style:dashed}

/* ---- 实时预览上的规则线（只画在网页，不进视频） ---- */
.ri{position:absolute;inset:0;pointer-events:none;transition:opacity .25s}
.ri.off{opacity:0}
.ri .ln{position:absolute;left:49.78%;top:0;width:0.44%;height:100%;background:rgba(255,52,52,.92);box-shadow:0 0 10px rgba(255,60,60,.55);border-radius:2px}
.ri .bx{position:absolute;top:16.66%;height:66.66%;width:16.66%;border:2px solid rgba(255,52,52,.85);border-radius:5px;box-shadow:0 0 10px rgba(255,60,60,.32)}
.ri .bx.a{left:16.66%}
.ri .bx.b{left:66.66%}

/* ---- 安防统计卡片 ---- */
.statscard{grid-column:1/-1;margin:0}
.statshead{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.statkpis{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:9px;padding:2px 16px 6px}
.statkpi{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line2);border-radius:14px;padding:10px 12px;min-width:0;transition:border-color .2s,box-shadow .2s}
.statkpi:hover{border-color:var(--accent);box-shadow:0 8px 20px rgba(120,94,36,.10)}
.statkpi span{display:block;font-size:10px;letter-spacing:.08em;color:var(--dim);font-weight:700}
.statkpi b{display:block;font-size:24px;line-height:1.15;margin:3px 0 1px;font-variant-numeric:tabular-nums;color:var(--text)}
.statkpi b small{font-size:12px;color:var(--dim);font-weight:600}
.statkpi small{font-size:10px;color:var(--dim2)}
.stwrap{position:relative;padding:2px 16px 8px}
.stlegend{display:flex;align-items:center;gap:14px;font-size:11px;color:var(--dim);font-weight:700;margin:2px 0 4px}
.stlegend i{display:inline-block;width:9px;height:9px;border-radius:3px;margin-right:5px;vertical-align:-1px}
.stlegend i.c1{background:linear-gradient(180deg,var(--accent2),var(--accent))}
.stlegend i.c2{background:var(--chart2)}
.stpeak{margin-left:auto;color:var(--dim);font-weight:600}
#stch{width:100%;height:230px;display:block}
.sttip{position:absolute;display:none;pointer-events:none;background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:5px 9px;font-size:11px;font-weight:700;color:var(--text);box-shadow:var(--shadow);white-space:nowrap;z-index:6}
.stfoot{display:flex;justify-content:space-between;gap:10px;padding:0 16px 14px;font-size:11px;color:var(--dim)}
html[data-theme="dark"] .statkpi{background:linear-gradient(180deg,#1a2029,#151a21);border-color:var(--line)}
html[data-theme="dark"] .statkpi:hover{border-color:#3a3350}
html[data-theme="dark"] .sttip{background:#1c222b;border-color:#443a63;color:#f0e6cf}
@media(max-width:1100px){.statkpis{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media(max-width:640px){.statkpis{grid-template-columns:repeat(2,minmax(0,1fr))}}
'''

js3 = r'''(function(){
"use strict";
var root=document.documentElement;
var btn=document.getElementById("themebtn");
function setTheme(t){
  var dark=(t==="dark");
  if(dark)root.setAttribute("data-theme","dark");else root.removeAttribute("data-theme");
  try{localStorage.setItem("sentinel.theme",dark?"dark":"light");}catch(e){}
  if(btn){btn.textContent=dark?"☀":"☾";btn.title=dark?"切换到日间模式":"切换到夜间模式";}
  var m=document.querySelector('meta[name="theme-color"]');
  if(m)m.setAttribute("content",dark?"#0d1014":"#fdfaf1");
}
var saved="light";
try{saved=localStorage.getItem("sentinel.theme")||"light";}catch(e){}
setTheme(saved);
if(btn)btn.onclick=function(){setTheme(root.getAttribute("data-theme")==="dark"?"light":"dark");};

function setTxt(id,t){var el=document.getElementById(id);if(el&&el.textContent!==t)el.textContent=t;}
function fmtUp(s){s=Math.max(0,s|0);var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);if(d>0)return d+" 天 "+h+" 小时";if(h>0)return h+" 小时 "+m+" 分";return m+" 分 "+(s%60)+" 秒";}
function setBar(id,p,cls){var b=document.getElementById(id);if(!b)return;b.style.width=Math.max(2,Math.min(100,p))+"%";b.className=cls||"";}
function loadSys(){
  fetch("/api/sysinfo",{cache:"no-store"}).then(function(r){return r.text();}).then(function(t){
    var o={};(t||"").trim().split("&").forEach(function(x){var i=x.indexOf("=");if(i>0)o[x.slice(0,i)]=x.slice(i+1);});
    var cpu=parseInt(o.cpu,10);
    if(!isNaN(cpu)){setTxt("s_cpu",cpu);setBar("b_cpu",cpu,cpu>=85?"hot":cpu>=65?"warm":"");}
    var mt=parseInt(o.mem_total,10)||0,mu=parseInt(o.mem_used,10)||0;
    if(mt>0){var mp=Math.round(mu*100/mt);setTxt("s_mem",mp);setBar("b_mem",mp,mp>=85?"hot":mp>=65?"warm":"");setTxt("s_mem_note",(mu/1048576).toFixed(1)+" / "+(mt/1048576).toFixed(1)+" MB");}
    var temp=parseInt(o.temp,10);
    if(!isNaN(temp)&&temp>=-40){setTxt("s_temp",temp);setBar("b_temp",(temp-20)/60*100,"");}
    if(o.ip)setTxt("s_ip",(o.ifname?o.ifname+" · ":"")+o.ip);
    if(o.uptime)setTxt("s_uptime","运行 "+fmtUp(parseInt(o.uptime,10)));
    if(o.sysver)setTxt("s_sys",o.sysver);
    if(o.fwver)setTxt("s_fw",o.fwver);
    if(o.build)setTxt("s_build","编译 "+o.build);
    setTxt("sysmsg","实时采样 · 2s");
  }).catch(function(){setTxt("sysmsg","设备信息不可用");});
}
loadSys();
setInterval(loadSys,2000);
})();
'''

# JS 总长超过 TCP 窗口，拆成两个普通脚本按顺序执行。
# 去掉 IIFE 后，两段的 var/function 都在全局，第二段末尾的 init() 能调用第一段。
assert js.startswith('(function(){\n"use strict";\n')
assert js.endswith('\ninit();\n})();\n')
js = js.replace('(function(){\n"use strict";\n', '', 1)
js = js.replace('\ninit();\n})();\n', '\ninit();\n', 1)
js_split = js.index('var ISP_NUM=')
js1, js2 = js[:js_split], js[js_split:]

def c_string_lines(s):
    out=[]
    for line in s.splitlines(True):
        # json.dumps with ensure_ascii=False keeps UTF-8 Chinese, escapes " and backslashes
        out.append('    ' + json.dumps(line, ensure_ascii=False))
    if not out:
        out.append('    ""')
    return "\n".join(out)

split_at = css.index('.actions{')
css1, css2 = css[:split_at], css[split_at:]

c = '''/*
 * osd_web_dash.c —— SENTINEL 看板页面
 *
 * 这个文件由同目录的 gen_dash.py 生成。CSS/JS 按主题和功能区拆成多份下发，
 * 保证每个 HTTP 响应都能塞进 TCP 窗口（14600 字节以内）。
 */
#include <string.h>

#include "osd_web_key.h"
#include "osd_web_dash.h"

static const char DASH_HTML[] =
%s
    ;

static const char DASH_CSS[] =
%s
    ;

static const char DASH_CSS2[] =
%s
    ;

static const char DASH_JS[] =
%s
    ;

static const char DASH_JS2[] =
%s
    ;

static const char DASH_CSS3[] =
%s
    ;

static const char DASH_JS3[] =
%s
    ;

int osd_web_dash_handle(int fd, const char *path, const char *query)
{
    (void)query;

    if (!strcmp(path, "/") || !strcmp(path, "/index.html"))
    {
        osd_web_send(fd, "200 OK", "text/html; charset=utf-8", DASH_HTML);
        return 1;
    }
    if (!strcmp(path, "/dash.css"))
    {
        osd_web_send_cached(fd, "200 OK", "text/css; charset=utf-8", DASH_CSS, 600);
        return 1;
    }
    if (!strcmp(path, "/dash2.css"))
    {
        osd_web_send_cached(fd, "200 OK", "text/css; charset=utf-8", DASH_CSS2, 600);
        return 1;
    }
    if (!strcmp(path, "/dash.js"))
    {
        osd_web_send_cached(fd, "200 OK", "application/javascript; charset=utf-8", DASH_JS, 600);
        return 1;
    }
    if (!strcmp(path, "/dash2.js"))
    {
        osd_web_send_cached(fd, "200 OK", "application/javascript; charset=utf-8", DASH_JS2, 600);
        return 1;
    }
    if (!strcmp(path, "/dash3.css"))
    {
        osd_web_send_cached(fd, "200 OK", "text/css; charset=utf-8", DASH_CSS3, 600);
        return 1;
    }
    if (!strcmp(path, "/dash3.js"))
    {
        osd_web_send_cached(fd, "200 OK", "application/javascript; charset=utf-8", DASH_JS3, 600);
        return 1;
    }

    return 0;
}
''' % (c_string_lines(html), c_string_lines(css1), c_string_lines(css2),
       c_string_lines(js1), c_string_lines(js2), c_string_lines(css3),
       c_string_lines(js3))

here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, 'osd_web_dash.c')
open(path, 'w', encoding='utf-8').write(c)
print('wrote', path, len(c), 'bytes')
