#ifndef MCB_CLIENT_H
#define MCB_CLIENT_H

/*
 * MC632X 控制客户端 —— 通过 HTTP 调用摄像头板自带的 8080 接口。
 *
 * 这些接口就是网页面板在用的那几个，语音和网页共享同一份设备状态：
 *
 *   GET /api/cam              -> "mode=0\n"       0=G1 带 AI 叠加, 1=G0 原始画面
 *   GET /api/cam?m=0|1        -> 切换，返回实际模式（失败会回滚，所以能以此判断真假）
 *   GET /api/isp              -> "br=0&ct=0&sat=0&sp=0&ev=0&dn=1&ir=0&wb=0&rg=50&bg=50\n"
 *   GET /api/isp?br=10&dn=3   -> 设置，只改传入的键，返回新快照
 *
 * 只需要 esp_http_client，不用新增源文件（板级目录的 *.cc 由 CMake 自动收集）。
 */

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "device_state.h"
#include "mcb_alert_sounds.h"
#include "mcb_report_sounds.h"
#include "mcp_server.h"
#include <esp_netif.h>
#include <wifi_manager.h>

/*
 * 这里填的不是板子的地址，而是 Windows 上 portproxy 的监听地址。
 *
 * 小智在电脑热点 (192.168.137.x)，板子在以太网段，两者不同网段，
 * 由电脑做 TCP 转发：
 *
 *   netsh interface portproxy add v4tov4 listenaddress=192.168.137.1 \
 *       listenport=8080 connectaddress=<板子IP> connectport=8080
 *
 * 所以小智只认 192.168.137.1（电脑热点网关），板子 IP 变了也不用改固件，
 * 只改 Windows 上的转发规则。
 */
#ifndef MCB_BASE_URL
#define MCB_BASE_URL "http://192.168.137.1:8080"
#endif

namespace mcb {

static const char* kTag = "MCB";

inline std::string HttpGet(const std::string& path, int timeout_ms = 3000)
{
    std::string url = std::string(MCB_BASE_URL) + path;

    esp_http_client_config_t cfg = {};
    cfg.url        = url.c_str();
    cfg.timeout_ms = timeout_ms;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr) {
        ESP_LOGE(kTag, "http client init failed: %s", path.c_str());
        return {};
    }

    std::string body;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        char buf[64];
        int  n;
        while ((n = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
            body.append(buf, n);
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (body.empty()) {
        ESP_LOGW(kTag, "GET %s failed (board offline?)", path.c_str());
    } else {
        ESP_LOGI(kTag, "GET %s -> %u bytes", path.c_str(), (unsigned)body.size());
    }
    return body;
}

/* 带 X-Term-Since 游标的 GET，用于分批读网页终端输出 */
inline std::string HttpGetCursor(const std::string& path, unsigned int& cursor,
                                 int timeout_ms = 3000)
{
    std::string url = std::string(MCB_BASE_URL) + path;

    esp_http_client_config_t cfg = {};
    cfg.url        = url.c_str();
    cfg.timeout_ms = timeout_ms;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr) {
        ESP_LOGE(kTag, "http client init failed: %s", path.c_str());
        return {};
    }

    std::string body;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);

        char* value = nullptr;
        if (esp_http_client_get_header(client, "X-Term-Since", &value) == ESP_OK &&
            value != nullptr) {
            cursor = (unsigned int)std::strtoul(value, nullptr, 10);
        }

        char buf[64];
        int  n;
        while ((n = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
            body.append(buf, n);
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return body;
}

/* 从 "br=0&ct=0&..." 里取某个键的值，找不到返回 def */
inline int GetValue(const std::string& body, const std::string& key, int def)
{
    std::string pattern = key + "=";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) {
        return def;
    }
    return std::atoi(body.c_str() + pos + pattern.size());
}

/* 取字符串值，读到 & 或换行为止（允许值里面有空格） */
inline std::string GetString(const std::string& body, const std::string& key,
                             const std::string& def = "")
{
    std::string pattern = key + "=";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) {
        return def;
    }
    size_t start = pos + pattern.size();
    size_t end   = body.find_first_of("&\r\n", start);
    return body.substr(start, end == std::string::npos ? std::string::npos
                                                       : end - start);
}

/* time=YYYY-MM-DD HH:MM:SS 本身带空格，读到 " kind=" 为止 */
inline std::string GetEventTime(const std::string& line)
{
    size_t pos = line.find("time=");
    if (pos == std::string::npos) {
        return "--";
    }
    size_t start = pos + 5;
    size_t end   = line.find(" kind=", start);
    if (end == std::string::npos) {
        end = line.find_first_of("\r\n", start);
    }
    return line.substr(start, end == std::string::npos ? std::string::npos
                                                       : end - start);
}

inline std::string Trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return "";
    }
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

inline std::string Truncate(const std::string& s, size_t max_len)
{
    if (s.size() <= max_len) {
        return s;
    }
    return s.substr(0, max_len) + "\n...(已截断)";
}

inline std::string Tail(const std::string& s, size_t max_len)
{
    if (s.size() <= max_len) {
        return s;
    }
    return "...(前文省略)\n" + s.substr(s.size() - max_len);
}

/* 去掉终端输出里的 \r 和 ANSI 转义 */
inline std::string CleanTerm(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\r') {
            continue;
        }
        if (c == 0x1b) {
            if (i + 1 < s.size() && s[i + 1] == '[') {
                i += 2;
                while (i < s.size() && !((unsigned char)s[i] >= 0x40 &&
                                         (unsigned char)s[i] <= 0x7e)) {
                    i++;
                }
            } else if (i + 1 < s.size()) {
                i++;
            }
            continue;
        }
        out.push_back((char)c);
    }
    return out;
}

/* URL 编码，用于把命令/文件名放进 query */
inline std::string UrlEncode(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0f]);
        }
    }
    return out;
}

inline const char* DayNightName(int dn)
{
    switch (dn) {
        case 0: return "彩色";
        case 1: return "黑白";
        default: return "未知";
    }
}

inline const char* WhiteBalanceName(int wb)
{
    switch (wb) {
        case 0: return "自动";
        case 1: return "锁定";
        case 2: return "白炽灯";
        case 3: return "暖光灯";
        case 4: return "自然光";
        case 5: return "日光灯";
        default: return "未知";
    }
}

/*
 * 事件轮询：摄像头板不会主动推，只能由小智每 2 秒拉一次。
 * 首次拉取只记录 seq，不补播旧事件；板子重启 seq 变小时也只更新。
 * 板子端不再出声，全部由小智的喇叭播报。
 *
 * 事件和播报内容的对应：
 *   kind=0 有人从左到右 / 右到左通过绊线  -> alert_person.ogg（"检测到有人通过，已开始录像"）
 *   kind=1 有人在周界框内停留（区域入侵） -> alert_area.ogg（"检测到区域内有人员停留，已拍照取证"）
 *
 * 上电时 RegisterTools() 在板级构造函数里就跑，那时 lwIP 还没初始化，
 * 连 getaddrinfo 都会触发断言 (tcpip_send_msg_wait_sem: Invalid mbox)。
 * 所以轮询前先确认 WiFi 已连上并拿到 IP，没准备好就等，绝不碰网络层。
 */
inline bool NetworkReady()
{
    if (!WifiManager::GetInstance().IsConnected()) {
        return false;
    }
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return false;
    }
    esp_netif_ip_info_t info = {};
    return esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0;
}

/* 自动检测播报开关：默认开启。板子只有在布防或自动抓拍开启时才产生事件，
 * 有事件就播；撤防后不再产生事件，自然也不会响。*/
inline bool& AnnounceFlag()
{
    static bool on = true;
    return on;
}

inline void SetAnnounce(bool on) { AnnounceFlag() = on; }
inline bool GetAnnounce() { return AnnounceFlag(); }

/* 播放一小段并等它播完；报告是十几段拼起来的，不等会全塞进解码队列。*/
inline void PlayClipWait(const std::string_view& clip)
{
    auto& app = Application::GetInstance();
    app.PlaySound(clip);
    auto& audio = app.GetAudioService();
    for (int i = 0; i < 100; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (audio.IsPlaybackIdle()) {
            vTaskDelay(pdMS_TO_TICKS(40));
            return;
        }
    }
}

/* 数字转中文读法（0~9999），按片段拼。*/
inline void AppendNumber(std::vector<std::string_view>& out, int n)
{
    if (n < 0) n = 0;
    if (n > 9999) n = 9999;
    if (n == 0) {
        out.push_back(mcb_report::NUM[0]);
        return;
    }

    int digits[4] = {n / 1000, (n / 100) % 10, (n / 10) % 10, n % 10};
    const std::string_view* units[3] = {
        &mcb_report::UNIT_THOUSAND, &mcb_report::UNIT_HUNDRED, &mcb_report::UNIT_TEN
    };
    bool started = false;
    bool zero_pending = false;

    for (int i = 0; i < 4; i++) {
        int d = digits[i];
        if (i == 3) {
            if (d) {
                if (zero_pending) out.push_back(mcb_report::NUM[0]);
                out.push_back(mcb_report::NUM[d]);
            }
            break;
        }
        if (d == 0) {
            if (started) zero_pending = true;
            continue;
        }
        if (zero_pending) {
            out.push_back(mcb_report::NUM[0]);
            zero_pending = false;
        }
        if (i == 2 && d == 1 && !started) {
            out.push_back(mcb_report::UNIT_TEN);
        } else {
            out.push_back(mcb_report::NUM[d]);
            out.push_back(*units[i]);
        }
        started = true;
    }
}

/* “本次拍摄期间，进入 X 人，离开 Y 人。画面最高 Z 人。”*/
inline void PlayReport(int in, int out, int peak)
{
    std::vector<std::string_view> seq;

    seq.push_back(mcb_report::RP_START);
    seq.push_back(mcb_report::RP_IN);
    AppendNumber(seq, in);
    seq.push_back(mcb_report::RP_MID);
    seq.push_back(mcb_report::RP_OUT);
    AppendNumber(seq, out);
    seq.push_back(mcb_report::RP_END);

    if (peak > 0) {
        seq.push_back(mcb_report::RP_PEAK);
        AppendNumber(seq, peak);
        seq.push_back(mcb_report::RP_END);
    }

    for (const auto& clip : seq) {
        PlayClipWait(clip);
    }
}

inline void EventWatcherTask(void* arg)
{
    (void)arg;
    int        last_seq       = -1;
    unsigned   last_report_req = 0;
    TickType_t last_announce = 0;

    while (true) {
        if (!NetworkReady()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        std::string body = HttpGet("/api/events?n=1", 3000);
        if (!body.empty()) {
            size_t pos = body.rfind("ev ");

            if (pos != std::string::npos) {
                std::string last = body.substr(pos);
                int seq  = GetValue(last, "seq", -1);
                int kind = GetValue(last, "kind", -1);

                if (seq >= 0) {
                    if (last_seq < 0 || seq < last_seq) {
                        last_seq = seq;
                    } else if (seq > last_seq) {
                        last_seq = seq;
                        if (GetAnnounce() && (kind == 0 || kind == 1)) {
                            TickType_t now = xTaskGetTickCount();
                            /* 事件最短 3 秒一条，区域入侵会连续触发，
                             * 播报至少间隔 8 秒，免得喇叭一直响。*/
                            if (last_announce == 0 ||
                                (now - last_announce) >= pdMS_TO_TICKS(8000)) {
                                last_announce = now;
                                ESP_LOGI(kTag, "announce seq=%d kind=%d (%s)", seq, kind,
                                         kind == 0 ? "通过/录像" : "区域停留/拍照");
                                Application::GetInstance().PlaySound(
                                    kind == 0 ? mcb_alert::ALERT_PERSON
                                              : mcb_alert::ALERT_AREA);
                            }
                        }
                    }
                }
            }
        }
        /* 网页“让小智播报”按钮：板子侧置 pending，这里取到后拼片段播报。*/
        std::string rp = HttpGet("/api/report/pending", 3000);
        if (rp.find("pending=1") != std::string::npos) {
            unsigned id  = (unsigned)GetValue(rp, "id", 0);
            unsigned req = (unsigned)GetValue(rp, "req", 0);
            if (id != 0 && req != 0 && req != last_report_req) {
                int in   = GetValue(rp, "in", 0);
                int out  = GetValue(rp, "out", 0);
                int peak = GetValue(rp, "peak", 0);
                ESP_LOGI(kTag, "report id=%u req=%u in=%d out=%d peak=%d",
                         id, req, in, out, peak);
                PlayReport(in, out, peak);
                last_report_req = req;
                HttpGet("/api/report?ack=1&id=" + std::to_string(id), 3000);
            } else if (id != 0 && req == last_report_req) {
                /* ack 没送到时别每 2 秒重播一遍，补一次 ack 就行 */
                HttpGet("/api/report?ack=1&id=" + std::to_string(id), 3000);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

inline void StartEventWatcher()
{
    static bool started = false;

    if (started) {
        return;
    }
    started = true;
    xTaskCreate(EventWatcherTask, "mcb_event", 8192, nullptr, 2, nullptr);
}

inline void RegisterTools()
{
    auto& mcp = McpServer::GetInstance();

    ESP_LOGI(kTag, "registering MC632X tools, base=%s", MCB_BASE_URL);

    /* ---------- 切换 LCD 画面源 ---------- */
    mcp.AddTool("self.mc632x.switch_screen",
        "切换 MC632X 摄像头板 LCD 屏幕显示的画面源。"
        "cam=1 显示带 AI 人形检测框和绊线叠加的画面（默认），"
        "cam=2 显示另一路摄像头的原始画面。"
        "当用户说“切换摄像头”“换一路画面”“看看另一个画面”时调用。",
        PropertyList({Property("cam", kPropertyTypeInteger, 1, 1, 2)}),
        [](const PropertyList& properties) -> ToolResult {
            int cam = properties["cam"].value<int>();
            if (cam < 1 || cam > 2) {
                cam = 1;
            }

            std::string body = HttpGet("/api/cam?m=" + std::to_string(cam - 1));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            /* 返回的是实际模式，切换失败会回滚，所以这里能判断真假 */
            int mode = GetValue(body, "mode", -1);
            if (mode != cam - 1) {
                return std::string("切换失败，画面仍停在另一路");
            }
            return std::string(cam == 1 ? "已切换到带 AI 叠加的画面"
                                        : "已切换到另一路原始画面");
        });

    /* ---------- 相对调整画面参数 ---------- */
    mcp.AddTool("self.mc632x.adjust_image",
        "调整 MC632X 画面的参数。param 只能是 brightness（亮度）、contrast（对比度）、"
        "saturation（饱和度）、sharpness（锐度）、exposure（曝光补偿）之一；"
        "delta 是增量，范围 -30 到 30，正值表示更亮或更强。"
        "当用户说“亮一点”“太暗了”“对比强一点”时调用。",
        PropertyList({
            Property("param", kPropertyTypeString),
            Property("delta", kPropertyTypeInteger, 10, -30, 30),
        }),
        [](const PropertyList& properties) -> ToolResult {
            struct Entry {
                const char* name;
                const char* key;
            };
            static const Entry kMap[] = {
                {"brightness", "br"},
                {"contrast",   "ct"},
                {"saturation", "sat"},
                {"sharpness",  "sp"},
                {"exposure",   "ev"},
            };

            std::string name = properties["param"].value<std::string>();
            const char* key  = nullptr;
            for (const auto& entry : kMap) {
                if (name == entry.name) {
                    key = entry.key;
                    break;
                }
            }
            if (key == nullptr) {
                return std::unexpected(
                    "不支持的参数，只能是 brightness/contrast/saturation/sharpness/exposure");
            }

            /* 先读全量状态拿当前值，再做相对调整：LLM 不用记住旧值 */
            std::string state = HttpGet("/api/isp");
            if (state.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int cur  = GetValue(state, key, 0);
            int want = cur + properties["delta"].value<int>();

            /* 板子会按每个控件的真实范围钳位，所以这里不钳；
             * 返回的快照里就是钳位后的真实值 */
            std::string body = HttpGet("/api/isp?" + std::string(key) + "=" +
                                       std::to_string(want));
            if (body.empty()) {
                return std::unexpected("设置失败");
            }

            int now = GetValue(body, key, want);
            return std::string("已调整，") + name + " 现在是 " + std::to_string(now);
        });

    /* ---------- 查询状态 ---------- */
    mcp.AddTool("self.mc632x.get_status",
        "查询 MC632X 摄像头板当前显示哪一路画面，以及亮度、对比度、饱和度、锐度、"
        "曝光补偿、日夜模式、白平衡。"
        "当用户问“现在是什么状态”“画面怎么样”“亮度多少”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string cam = HttpGet("/api/cam");
            std::string isp = HttpGet("/api/isp");
            if (cam.empty() || isp.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int mode = GetValue(cam, "mode", -1);
            char buf[320];
            snprintf(buf, sizeof(buf),
                     "当前显示%s。亮度 %d，对比度 %d，饱和度 %d，锐度 %d，"
                     "曝光补偿 %d，日夜模式 %s，白平衡 %s，红增益 %d，蓝增益 %d。",
                     mode == 0 ? "带 AI 叠加的画面" : "原始画面",
                     GetValue(isp, "br", 0), GetValue(isp, "ct", 0),
                     GetValue(isp, "sat", 0), GetValue(isp, "sp", 0),
                     GetValue(isp, "ev", 0),
                     DayNightName(GetValue(isp, "dn", 0)),
                     WhiteBalanceName(GetValue(isp, "wb", 0)),
                     GetValue(isp, "rg", 50), GetValue(isp, "bg", 50));
            return std::string(buf);
        });

    /* ---------- 日夜模式 ---------- */
    mcp.AddTool("self.mc632x.set_daynight",
        "设置 MC632X 的画面色彩模式。mode=color 彩色，mono 黑白。"
        "当用户说“切到夜间模式”“切黑白画面”“切回彩色”时调用。",
        PropertyList({Property("mode", kPropertyTypeString)}),
        [](const PropertyList& properties) -> ToolResult {
            std::string mode = properties["mode"].value<std::string>();

            int dn;
            const char* label;
            if (mode == "color") {
                dn = 0;
                label = "彩色";
            } else if (mode == "mono") {
                dn = 1;
                label = "黑白";
            } else {
                return std::unexpected("mode 只能是 color 或 mono");
            }

            std::string body = HttpGet("/api/isp?dn=" + std::to_string(dn));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            return std::string("已设置为") + label;
        });

    /* ---------- 抓拍 ---------- */
    mcp.AddTool("self.mc632x.snapshot",
        "让 MC632X 立刻抓拍一张照片并保存到 SD 卡。"
        "cam=1 抓拍带 AI 人形框和绊线/周界叠加的画面（默认），"
        "cam=2 抓拍不带叠加的原始画面。"
        "当用户说“拍张照”“抓拍一张”“保存当前画面”时调用。",
        PropertyList({Property("cam", kPropertyTypeInteger, 1, 1, 2)}),
        [](const PropertyList& properties) -> ToolResult {
            int cam = properties["cam"].value<int>();
            if (cam < 1 || cam > 2) {
                cam = 1;
            }

            /* 用户语义：cam=1 带叠加、cam=2 原始。
             * API 语义：cam=1 = G1 AI，cam=0 = G0 干净。*/
            int api_cam = (cam == 1) ? 1 : 0;

            /* 第一次抓拍可能要等 SD 挂载/建目录，超时放宽到 8 秒 */
            std::string body = HttpGet("/api/snap?save=1&cam=" + std::to_string(api_cam), 8000);
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            if (GetValue(body, "ok", 0) != 1) {
                return std::string("抓拍失败：") + body;
            }

            int size = GetValue(body, "size", 0);
            return std::string("已抓拍并保存到 SD 卡") +
                   (cam == 1 ? "（带 AI 叠加）" : "（原始画面）") +
                   "，大小 " + std::to_string(size / 1024) + " KB";
        });

    /* ---------- AI 叠加开关 ---------- */
    mcp.AddTool("self.mc632x.set_overlay",
        "打开或关闭 MC632X 画面上的 AI 叠加框（人形框、绊线、周界）。"
        "on=1 显示，on=0 隐藏。隐藏后 AI 检测和事件抓拍仍然继续。"
        "当用户说“关掉检测框”“隐藏框”“显示检测框”时调用。",
        PropertyList({Property("on", kPropertyTypeInteger, 1, 0, 1)}),
        [](const PropertyList& properties) -> ToolResult {
            int on = properties["on"].value<int>();
            std::string body = HttpGet("/api/overlay?on=" + std::to_string(on ? 1 : 0));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            return std::string(GetValue(body, "overlay", 0)
                                   ? "已显示 AI 叠加框"
                                   : "已隐藏 AI 叠加框，检测仍在运行");
        });

    /* ---------- 事件查询 ---------- */
    mcp.AddTool("self.mc632x.get_events",
        "查询 MC632X 的安全状态和最近事件。返回 AI 叠加开关、SD 卡状态、"
        "事件数量，以及最近一次绊线/周界/手动抓拍事件。"
        "当用户问“最近有没有报警”“有没有人经过”“查看事件记录”“SD 卡正常吗”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet("/api/events?n=5");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int count = GetValue(body, "count", 0);
            int ov    = GetValue(body, "overlay", -1);
            int sd    = GetValue(body, "sd", 0);

            char buf[320];
            snprintf(buf, sizeof(buf),
                     "AI 叠加%s，SD 卡%s，最近 %d 条事件。",
                     ov == 1 ? "已开启" : "已关闭",
                     sd ? "正常" : "未就绪", count);
            std::string out = buf;

            size_t pos = body.rfind("ev ");
            if (pos != std::string::npos) {
                std::string last = body.substr(pos);
                int kind = GetValue(last, "kind", -1);
                const char* kn = kind == 0 ? "绊线" : kind == 1 ? "周界" : "手动抓拍";
                out += std::string("最近一次：") + kn +
                       (GetValue(last, "ok", 0) ? "，已保存到 SD 卡。" : "，保存失败。");
            }
            return out;
        });

    /* ---------- 布防 / 撤防 ---------- */
    mcp.AddTool("self.mc632x.set_arm",
        "布防或撤防 MC632X 摄像头板。on=1 布防：检测到人时自动存图；"
        "on=0 撤防：只保留检测和画面，不存证。"
        "当用户说“布防”“撤防”“开启安防”“关闭安防”时调用。",
        PropertyList({Property("on", kPropertyTypeInteger, 1, 0, 1)}),
        [](const PropertyList& properties) -> ToolResult {
            int on = properties["on"].value<int>();
            std::string body = HttpGet("/api/arm?on=" + std::to_string(on ? 1 : 0));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            int armed = GetValue(body, "armed", -1);
            if (armed != (on ? 1 : 0)) {
                return std::string("布防状态切换失败");
            }
            return std::string(armed ? "已布防，检测到人会自动存图"
                                      : "已撤防，只保留检测和画面");
        });

    /* ---------- 自动检测播报开关 ---------- */
    mcp.AddTool("self.mc632x.set_announce",
        "开启或关闭小智的自动检测播报：on=1 有人通过绊线或有人在周界内停留时用小智喇叭播报，"
        "on=0 静音（事件照常记录）。默认开启。播报依赖板子产生事件，也就是布防或自动抓拍要开着。"
        "当用户说“开启自动检测播报”“开始播报”“打开播报模式”“别播报了”“静音”时调用。",
        PropertyList({Property("on", kPropertyTypeInteger, 1, 0, 1)}),
        [](const PropertyList& properties) -> ToolResult {
            int on = properties["on"].value<int>();
            SetAnnounce(on != 0);
            if (on == 0) {
                return std::string("已关闭自动检测播报，事件仍会照常记录");
            }
            std::string body = HttpGet("/api/status");
            if (!body.empty() && GetValue(body, "armed", 0) == 0 &&
                GetValue(body, "auto", 0) == 0) {
                return std::string("已开启自动检测播报，但板子现在未布防、自动抓拍也关着，"
                                   "不会产生检测事件；需要我先布防吗？");
            }
            return std::string("已开启自动检测播报，检测到人或区域入侵就播报");
        });

    /* ---------- 当前人数 ---------- */
    mcp.AddTool("self.mc632x.get_people",
        "查询 MC632X 当前画面里有几个人。用户问“现在有人吗”“画面里有几个人”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet("/api/status");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            int people = GetValue(body, "people", 0);
            int armed  = GetValue(body, "armed", 0);
            char buf[96];
            snprintf(buf, sizeof(buf), "当前画面里有 %d 个人，%s。",
                     people, armed ? "已布防" : "已撤防");
            return std::string(buf);
        });

    /* ---------- 设备状态 ---------- */
    mcp.AddTool("self.mc632x.get_sysinfo",
        "查询 MC632X 设备状态：CPU 占用、内存、芯片温度、运行时间、网卡 IP、"
        "系统版本、固件版本。"
        "用户问“设备状态”“CPU 多少”“内存用了多少”“温度多少”“IP 是多少”"
        "“固件什么版本”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet("/api/sysinfo");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int cpu       = GetValue(body, "cpu", -1);
            int mem_total = GetValue(body, "mem_total", 0);
            int mem_used  = GetValue(body, "mem_used", 0);
            int temp      = GetValue(body, "temp", -1);
            int uptime    = GetValue(body, "uptime", 0);
            std::string ip     = GetString(body, "ip", "未知");
            std::string sysver = GetString(body, "sysver", "未知");
            std::string fwver  = GetString(body, "fwver", "未知");
            std::string build  = GetString(body, "build", "未知");

            int up_h = uptime / 3600;
            int up_m = (uptime % 3600) / 60;
            char buf[640];
            snprintf(buf, sizeof(buf),
                     "CPU %d%%，内存 %.1f/%.1f MB，温度 %d°C，"
                     "运行 %d 小时 %d 分，IP %s，系统 %s，固件 %s（%s）。",
                     cpu, mem_used / 1048576.0, mem_total / 1048576.0,
                     temp, up_h, up_m, ip.c_str(), sysver.c_str(),
                     fwver.c_str(), build.c_str());
            return std::string(buf);
        });

    /* ---------- 精确设置 ISP 参数 ---------- */
    mcp.AddTool("self.mc632x.set_isp",
        "把 MC632X 的某个 ISP 参数设到指定值。key 只能用："
        "br 亮度(-100~100)、ct 对比度(-100~100)、sat 饱和度(-100~100)、"
        "sp 锐度(-100~100)、ev 曝光补偿(-8~8)、dn 日夜(0 彩色 1 黑白)、"
        "ir 红外(0 智能 1 开 2 关)、wb 白平衡(0 自动 1 锁定)、rg 红增益(0~100)、bg 蓝增益(0~100)。"
        "红蓝增益是相对自动白平衡的微调，50 = 基准值。"
        "用户说“亮度调到 30”“对比度改成 -10”“白平衡设为锁定”“红外关掉”时调用；"
        "只说“亮一点/暗一点”这种相对调整请用 adjust_image。",
        PropertyList({
            Property("key", kPropertyTypeString),
            Property("value", kPropertyTypeInteger, 0, -100, 100),
        }),
        [](const PropertyList& properties) -> ToolResult {
            std::string key = Trim(properties["key"].value<std::string>());
            int value = properties["value"].value<int>();

            static const char* keys[] = {
                "br", "ct", "sat", "sp", "ev", "dn", "ir", "wb", "rg", "bg",
            };
            bool valid = false;
            for (const char* k : keys) {
                if (key == k) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                return std::unexpected(
                    "不支持的参数，key 只能是 br/ct/sat/sp/ev/dn/ir/wb/rg/bg");
            }

            std::string body = HttpGet("/api/isp?k=" + key + "&v=" +
                                       std::to_string(value));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int now = GetValue(body, key, value);
            char buf[160];
            if (key == "dn") {
                snprintf(buf, sizeof(buf), "日夜模式现在是%s",
                         now ? "黑白" : "彩色");
            } else if (key == "ir") {
                const char* v = now == 0 ? "智能红外"
                              : now == 1 ? "红外开" : "红外关";
                snprintf(buf, sizeof(buf), "红外现在是%s", v);
            } else if (key == "wb") {
                static const char* wb[] = {
                    "自动", "锁定", "白炽灯", "暖光灯", "自然光", "日光灯",
                };
                const char* v = (now >= 0 && now <= 5) ? wb[now] : "未知";
                snprintf(buf, sizeof(buf), "白平衡现在是%s", v);
            } else {
                static const struct {
                    const char* k;
                    const char* n;
                } names[] = {
                    {"br", "亮度"}, {"ct", "对比度"}, {"sat", "饱和度"},
                    {"sp", "锐度"}, {"ev", "曝光补偿"},
                    {"rg", "红增益"}, {"bg", "蓝增益"},
                };
                const char* name = key.c_str();
                for (const auto& e : names) {
                    if (key == e.k) {
                        name = e.n;
                        break;
                    }
                }
                snprintf(buf, sizeof(buf), "%s现在是%d", name, now);
            }
            return std::string(buf);
        });

    /* ---------- ISP 恢复默认 ---------- */
    mcp.AddTool("self.mc632x.reset_isp",
        "把 MC632X 的 ISP 参数恢复默认（亮度、对比度、饱和度、锐度、曝光补偿、"
        "日夜模式、白平衡、红蓝增益）。用户说“恢复默认画面”“ISP 重置”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet(
                "/api/isp?br=0&ct=0&sat=0&sp=0&ev=0&dn=0&wb=0&rg=50&bg=50");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            return std::string("ISP 参数已恢复默认");
        });

    /* ---------- 自动抓拍开关 ---------- */
    mcp.AddTool("self.mc632x.set_auto",
        "打开或关闭绊线/周界的自动抓拍存证。on=1 开启，on=0 关闭；关闭时也会撤防。"
        "用户说“打开自动抓拍”“关掉自动存图”时调用。",
        PropertyList({Property("on", kPropertyTypeInteger, 1, 0, 1)}),
        [](const PropertyList& properties) -> ToolResult {
            int on = properties["on"].value<int>();
            std::string body = HttpGet("/api/auto?on=" +
                                       std::to_string(on ? 1 : 0));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            int now = GetValue(body, "auto", on);
            return std::string(now ? "已开启自动抓拍存证"
                                   : "已关闭自动抓拍存证");
        });

    /* ---------- SD 卡相册 / 容量 / 自检 ---------- */
    mcp.AddTool("self.mc632x.files",
        "管理 MC632X 的 SD 卡相册。action 取值："
        "list 列出最近照片；delete 删除指定 name（name 为空时删最新一张）；"
        "clean 只保留最新 keep 张；space 查询剩余容量；remount 重新挂载 SD；"
        "selftest 做一次 64KB 读写自检。"
        "用户说“看看相册”“删掉最后一张”“清理到 100 张”“SD 还剩多少空间”"
        "“重新挂载 SD”“SD 自检”时调用。删除和清理前先向用户确认。",
        PropertyList({
            Property("action", kPropertyTypeString),
            Property("name", kPropertyTypeString, std::string("")),
            Property("keep", kPropertyTypeInteger, 100, 1, 10000),
            Property("n", kPropertyTypeInteger, 12, 1, 50),
        }),
        [](const PropertyList& properties) -> ToolResult {
            std::string action = Trim(properties["action"].value<std::string>());
            std::string name   = Trim(properties["name"].value<std::string>());
            int keep = properties["keep"].value<int>();
            int n    = properties["n"].value<int>();

            if (action == "list") {
                std::string body = HttpGet("/api/files?n=" + std::to_string(n),
                                           5000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                if (body.find("sd=0") != std::string::npos) {
                    return std::string("SD 卡未就绪");
                }

                int total = GetValue(body, "total", 0);
                std::string out = "SD 卡共 " + std::to_string(total) +
                                  " 个文件，最近 " + std::to_string(n) +
                                  " 个：\n";
                size_t pos = body.find('\n');
                int shown = 0;
                while (pos != std::string::npos && shown < n) {
                    size_t start = pos + 1;
                    size_t end   = body.find('\n', start);
                    std::string line = body.substr(
                        start, end == std::string::npos ? std::string::npos
                                                        : end - start);
                    line = Trim(line);
                    if (line.empty()) {
                        pos = end;
                        continue;
                    }
                    size_t sp = line.find(' ');
                    std::string fn = line.substr(0, sp);
                    int size = sp == std::string::npos
                                   ? 0
                                   : std::atoi(line.c_str() + sp + 1);
                    out += "· " + fn + " (" + std::to_string(size / 1024) +
                           " KB)\n";
                    shown++;
                    pos = end;
                }
                if (shown == 0) {
                    return std::string("SD 卡上没有照片");
                }
                return Truncate(out, 1400);
            }

            if (action == "delete") {
                if (name.empty()) {
                    std::string list = HttpGet("/api/files?n=1", 5000);
                    if (list.empty()) {
                        return std::unexpected("摄像头板没有响应，请检查网络");
                    }
                    if (list.find("sd=0") != std::string::npos) {
                        return std::string("SD 卡未就绪");
                    }
                    size_t p = list.find('\n');
                    if (p == std::string::npos) {
                        return std::string("没有可删除的文件");
                    }
                    size_t start = p + 1;
                    size_t e = list.find(' ', start);
                    name = Trim(list.substr(
                        start, e == std::string::npos ? std::string::npos
                                                      : e - start));
                    if (name.empty()) {
                        return std::string("没有可删除的文件");
                    }
                }

                std::string body = HttpGet("/api/delete?name=" +
                                           UrlEncode(name), 5000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                if (body.find("ok=1") != std::string::npos) {
                    return "已删除 " + name;
                }
                return std::string("删除失败：") + Trim(body);
            }

            if (action == "clean") {
                std::string body = HttpGet("/api/clean?keep=" +
                                           std::to_string(keep), 8000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                int deleted = GetValue(body, "deleted", -1);
                if (body.find("ok=1") != std::string::npos) {
                    return "已清理 " + std::to_string(deleted) +
                           " 个文件，保留最新 " + std::to_string(keep) + " 个";
                }
                return std::string("清理失败：") + Trim(body);
            }

            if (action == "space") {
                std::string body = HttpGet("/api/space", 8000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                int freekb = GetValue(body, "freekb", -1);
                int total  = GetValue(body, "totalkb", -1);
                if (freekb < 0 || total <= 0) {
                    return std::string("容量查询失败");
                }
                char buf[128];
                snprintf(buf, sizeof(buf), "SD 卡剩余 %.1f MB / 共 %.2f GB",
                         freekb / 1024.0, total / 1048576.0);
                return std::string(buf);
            }

            if (action == "remount") {
                std::string body = HttpGet("/api/sd/remount?force=1", 8000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                return std::string(body.find("ok=1") != std::string::npos
                                       ? "SD 卡已重新挂载"
                                       : "重挂失败");
            }

            if (action == "selftest") {
                std::string body = HttpGet("/api/sd/selftest", 8000);
                if (body.empty()) {
                    return std::unexpected("摄像头板没有响应，请检查网络");
                }
                return std::string("SD 自检：") + Trim(body);
            }

            return std::unexpected(
                "action 只能是 list/delete/clean/space/remount/selftest");
        });

    /* ---------- 事件标注 / 漏报 ---------- */
    mcp.AddTool("self.mc632x.label_event",
        "给事件打标：seq 是事件序号，v=1 表示检对了，v=0 表示误报。"
        "用户说“这条是对的”“这是误报”时调用；seq 可以先用 list_events 查。",
        PropertyList({
            Property("seq", kPropertyTypeInteger, 1, 1, 1000000),
            Property("v", kPropertyTypeInteger, 1, 0, 1),
        }),
        [](const PropertyList& properties) -> ToolResult {
            int seq = properties["seq"].value<int>();
            int v   = properties["v"].value<int>();
            std::string body = HttpGet("/api/label?seq=" + std::to_string(seq) +
                                       "&v=" + std::to_string(v ? 1 : 0));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            if (body.find("ok=1") != std::string::npos) {
                return std::string(v ? "已标注为正确" : "已标注为误报");
            }
            return std::string("标注失败：") + Trim(body);
        });

    mcp.AddTool("self.mc632x.record_missed_event",
        "记录一次漏报（有人经过但没触发事件）。"
        "用户说“记一次漏报”“刚才没报警”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet("/api/label?fn=1");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            return std::string(body.find("ok=1") != std::string::npos
                                   ? "已记录一次漏报"
                                   : "记录失败");
        });

    /* ---------- 测试模式 ---------- */
    mcp.AddTool("self.mc632x.set_test_mode",
        "切换测试模式。on=1 把事件冷却缩短到 300ms 并写 detect.csv，用于连续测试；"
        "on=0 恢复正常。用户说“打开测试模式”“关掉测试模式”时调用。",
        PropertyList({Property("on", kPropertyTypeInteger, 1, 0, 1)}),
        [](const PropertyList& properties) -> ToolResult {
            int on = properties["on"].value<int>();
            std::string body = HttpGet("/api/test?on=" +
                                       std::to_string(on ? 1 : 0));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            int now = GetValue(body, "test", on);
            return std::string(now ? "已进入测试模式（冷却 300ms，写 detect.csv）"
                                   : "已退出测试模式");
        });

    /* ---------- 时间同步 ---------- */
    mcp.AddTool("self.mc632x.sync_time",
        "把小智的当前时间同步给 MC632X，用于事件时间戳和文件命名。"
        "tz 是时区分钟数，默认 480（东八区）。用户说“对一下时间”“同步时间”时调用。",
        PropertyList({Property("tz", kPropertyTypeInteger, 480, -720, 840)}),
        [](const PropertyList& properties) -> ToolResult {
            time_t now = time(nullptr);
            if (now < 1600000000) {
                return std::unexpected("小智自己的时间还没同步，请先联网校时");
            }
            int tz = properties["tz"].value<int>();
            std::string body = HttpGet(
                "/api/time?t=" + std::to_string((long long)now) +
                "&tz=" + std::to_string(tz));
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            if (GetValue(body, "synced", 0) != 1) {
                return std::unexpected("板子没有接受时间同步");
            }
            return std::string("已同步时间");
        });

    /* ---------- 网页终端（shell） ---------- */
    mcp.AddTool("self.mc632x.run_shell",
        "在 MC632X 的 msh 终端执行命令（和网页终端同权限）。"
        "只读查询（ls、free、ps、date、ifconfig）可以直接执行；删除、格式化、"
        "重启、改配置等危险命令板端会返回 confirm=1，这时必须先征得用户口头同意，"
        "再把 force 设为 1 重试。用户说“查一下板子的进程”“执行 ls”“重启板子”时调用。",
        PropertyList({
            Property("cmd", kPropertyTypeString),
            Property("force", kPropertyTypeInteger, 0, 0, 1),
        }),
        [](const PropertyList& properties) -> ToolResult {
            std::string cmd = Trim(properties["cmd"].value<std::string>());
            int force = properties["force"].value<int>();

            if (cmd.empty()) {
                return std::unexpected("命令不能为空");
            }
            if (cmd.size() > 120) {
                return std::unexpected("命令太长，最多 120 字符");
            }
            std::string enc = UrlEncode(cmd);
            if (enc.size() > 420) {
                return std::unexpected("命令编码后太长");
            }

            std::string path = "/api/term?cmd=" + enc +
                               (force ? "&force=1" : "");
            std::string body = HttpGet(path, 5000);
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            if (body.find("confirm=1") != std::string::npos) {
                return std::string("这条命令可能改变设备状态。请先问用户是否继续；"
                                   "用户确认后把 force 设为 1 再调用一次。");
            }
            if (body.find("busy=1") != std::string::npos) {
                return std::unexpected("终端正忙，上一条命令还没执行完");
            }
            if (body.find("ok=1") != std::string::npos) {
                return std::string("已提交到终端执行；可以用 term_output 查看输出");
            }
            return std::string("执行失败：") + Trim(body);
        });

    mcp.AddTool("self.mc632x.term_output",
        "读取 MC632X 网页终端最近的输出，用于查看上一条终端命令的执行结果。",
        PropertyList({Property("max", kPropertyTypeInteger, 1500, 200, 3000)}),
        [](const PropertyList& properties) -> ToolResult {
            int max_len = properties["max"].value<int>();
            unsigned int since = 0;
            std::string last;

            /* 终端输出是 32KB 环形缓冲，每次最多返回 4KB；
             * 从 since=0 开始跟着 X-Term-Since 走到最新一块。 */
            for (int i = 0; i < 8; i++) {
                unsigned int next = since;
                std::string chunk = HttpGetCursor(
                    "/api/term/out?since=" + std::to_string(since), next, 3000);
                if (chunk.empty()) {
                    break;
                }
                last = chunk;
                if (next <= since) {
                    break;
                }
                since = next;
                if (chunk.size() < 4096) {
                    break;
                }
            }
            if (last.empty()) {
                return std::string("终端没有输出");
            }
            last = CleanTerm(last);
            return Tail(last, (size_t)max_len);
        });

    /* ---------- 通用网页 API ---------- */
    mcp.AddTool("self.mc632x.web_api",
        "通用网页 API 调用（GET），path 必须以 /api/ 开头。常用："
        "/api/status、/api/sysinfo、/api/events?n=5、/api/files?n=12、/api/stats、"
        "/api/space、/api/clean?keep=100、/api/auto?on=1、/api/arm?on=1、"
        "/api/overlay?on=1、/api/cam?m=1、/api/time?t=<epoch>&tz=480、"
        "/api/test?on=1、/api/label?fn=1。"
        "需要执行 msh 命令时用 run_shell，不要直接拼 /api/term。"
        "删除、清理、重挂等操作前先向用户确认。",
        PropertyList({Property("path", kPropertyTypeString)}),
        [](const PropertyList& properties) -> ToolResult {
            std::string path = Trim(properties["path"].value<std::string>());
            if (path.rfind("/api/", 0) != 0) {
                return std::unexpected("path 必须以 /api/ 开头");
            }
            if (path.rfind("/api/term", 0) == 0) {
                return std::unexpected(
                    "终端命令请用 run_shell 工具，避免绕过确认");
            }
            if (path == "/api/file" || path.rfind("/api/file?", 0) == 0 ||
                path.rfind("/api/event.jpg", 0) == 0 ||
                path.rfind("/api/clip", 0) == 0) {
                return std::unexpected("图片/视频是二进制，请用网页查看");
            }

            std::string body = HttpGet(path, 5000);
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            body = CleanTerm(body);
            return Truncate(body, 1800);
        });

    /* ---------- 测试记录 ---------- */
    mcp.AddTool("self.mc632x.get_report",
        "读取 MC632X 的测试记录。type=detect 读检测记录 detect.csv，"
        "type=labels 读事件标注记录 labels.csv；返回最近一部分内容。"
        "用户问“检测记录”“标注记录”“测试日志”时调用。",
        PropertyList({
            Property("type", kPropertyTypeString, std::string("detect")),
        }),
        [](const PropertyList& properties) -> ToolResult {
            std::string type = Trim(properties["type"].value<std::string>());
            std::string path;
            if (type == "labels") {
                path = "/api/labels";
            } else if (type == "detect") {
                path = "/api/testlog";
            } else {
                return std::unexpected("type 只能是 detect 或 labels");
            }

            std::string body = HttpGet(path, 5000);
            if (body.empty()) {
                return std::string("没有记录或板子没有响应");
            }
            body = CleanTerm(body);
            return Tail(body, 1200);
        });

    /* ---------- 最近事件列表 ---------- */
    mcp.AddTool("self.mc632x.list_events",
        "列出 MC632X 最近的 N 条事件（最新在前）。n 范围 1~10。"
        "返回布防、自动抓拍、AI 叠加、SD 卡状态，以及事件类型、时间、人数、"
        "置信度、是否存证、标注结果。"
        "用户问“最近几条事件”“有没有人经过”“查看事件记录”时调用。",
        PropertyList({Property("n", kPropertyTypeInteger, 3, 1, 10)}),
        [](const PropertyList& properties) -> ToolResult {
            int n = properties["n"].value<int>();
            std::string body = HttpGet("/api/events?n=" + std::to_string(n), 5000);
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }

            int total   = GetValue(body, "count", 0);
            int armed   = GetValue(body, "armed", 0);
            int auto_on = GetValue(body, "auto", 0);
            int ov      = GetValue(body, "overlay", -1);
            int sd      = GetValue(body, "sd", 0);
            int test    = GetValue(body, "test", 0);

            char head[256];
            snprintf(head, sizeof(head),
                     "共 %d 条事件；%s，自动抓拍%s，AI 叠加%s，SD 卡%s%s。",
                     total,
                     armed ? "已布防" : "已撤防",
                     auto_on ? "已开" : "已关",
                     ov == 1 ? "已开" : "已关",
                     sd ? "正常" : "未就绪",
                     test ? "，测试模式已开" : "");
            std::string out = head;

            std::vector<std::string> lines;
            size_t pos = body.find("ev ");
            while (pos != std::string::npos) {
                size_t end = body.find('\n', pos);
                lines.emplace_back(body.substr(
                    pos, end == std::string::npos ? std::string::npos : end - pos));
                if (end == std::string::npos) {
                    break;
                }
                pos = body.find("ev ", end + 1);
            }

            int shown = 0;
            for (int i = (int)lines.size() - 1; i >= 0 && shown < n; i--) {
                const std::string& line = lines[i];
                int seq  = GetValue(line, "seq", -1);
                int kind = GetValue(line, "kind", -1);
                int conf = GetValue(line, "conf", 0);
                int cnt  = GetValue(line, "count", 0);
                int size = GetValue(line, "size", 0);
                int ok   = GetValue(line, "ok", 0);
                int label= GetValue(line, "label", -1);
                std::string tm = GetEventTime(line);
                const char* kn = kind == 0 ? "绊线" : kind == 1 ? "周界" : "手动抓拍";
                char buf[256];
                if (kind == 2) {
                    snprintf(buf, sizeof(buf),
                             "\n%d) [seq=%d] %s %s，%s，%d KB%s",
                             ++shown, seq, tm.c_str(), kn,
                             ok ? "已存证" : "未存证", size / 1024,
                             label == 1 ? "，已标正确"
                                        : label == 0 ? "，已标误报" : "");
                } else {
                    snprintf(buf, sizeof(buf),
                             "\n%d) [seq=%d] %s %s，置信度 %d%%，%d 人，%s，%d KB%s",
                             ++shown, seq, tm.c_str(), kn, conf, cnt,
                             ok ? "已存证" : "未存证", size / 1024,
                             label == 1 ? "，已标正确"
                                        : label == 0 ? "，已标误报" : "");
                }
                out += buf;
            }
            if (shown == 0) {
                out += "\n当前没有事件。";
            } else {
                out += "\n（最新在前）";
            }
            return Truncate(out, 1600);
        });

    /* ---------- 安防统计 ---------- */
    mcp.AddTool("self.mc632x.get_stats",
        "查询 MC632X 今天的安防统计：进入/离开人数、当前画面人数、"
        "区域进入/离开次数、最长停留时间。"
        "用户问“今天进来多少人”“统计一下”“有人停留多久”时调用。",
        PropertyList(),
        [](const PropertyList&) -> ToolResult {
            std::string body = HttpGet("/api/stats");
            if (body.empty()) {
                return std::unexpected("摄像头板没有响应，请检查网络");
            }
            int in_today = GetValue(body, "in_today", 0);
            int out_today = GetValue(body, "out_today", 0);
            int visible = GetValue(body, "visible", 0);
            int area_in = GetValue(body, "area_in_today", 0);
            int area_out = GetValue(body, "area_out_today", 0);
            int dwell_ms = GetValue(body, "max_dwell_ms", 0);
            char buf[320];
            snprintf(buf, sizeof(buf),
                     "今天进入 %d 人，离开 %d 人，当前画面 %d 人；"
                     "区域进入 %d 次，离开 %d 次，最长停留 %d 秒。",
                     in_today, out_today, visible, area_in, area_out,
                     dwell_ms / 1000);
            return std::string(buf);
        });


    StartEventWatcher();
}

}  // namespace mcb

#endif  // MCB_CLIENT_H
