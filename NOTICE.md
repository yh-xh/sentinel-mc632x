# 第三方组件与声明

本项目为 2026 RT-Thread 嵌入式软件大赛参赛作品，源码由项目作者编写与修改。仓库中涉及的第三方内容说明如下：

## 1. 富瀚 MC632X SDK（FH_RT V3.4.0）

- `mc632x/overlay/` 下部分文件（如 `plat_mmc.c`、`board.c`、`lwipopts.h`、`ffconf.h`、`sample_nna_ivs.c`、`sample_common_stream.c`、`libdmc_http_mjpeg.c` 等）修改自富瀚官方 SDK。
- 这些文件的版权归上海富瀚微电子股份有限公司所有，本仓库**不再分发完整 SDK**，仅提供为参赛作品所必需的修改文件，供学习交流使用。
- 使用前请通过富瀚官方或大赛渠道获取完整的 FH_RT V3.4.0 SDK，并遵守其许可协议。

## 2. xiaozhi-esp32

- `xiaozhi/` 下的 `mcb_*.h`、语音片段与补丁基于开源项目 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 修改。
- 该项目以 MIT 协议开源；本仓库对它的修改同样遵循 MIT 协议，并保留原项目的版权声明。

## 3. 语音片段

- `xiaozhi/assets/common/*.ogg` 由本项目使用 edge-tts（zh-CN-XiaoxiaoNeural）生成文本语音，再用 ffmpeg 转为 48kHz 单声道 Opus，仅包含“开始/进入/离开/峰值/结束”和数字 0–9 等通用词。
- 生成脚本与文本见项目文档；语音合成服务的使用请遵守对应服务条款。

## 4. 其他

- 本项目新增代码以 MIT 协议开源（见 `LICENSE`）。
- 演示中出现的所有人物、场景均为测试素材，不涉及真实个人信息。
- 本作品为竞赛原型，未取得安防产品认证，不建议直接用于真实安防场景。
