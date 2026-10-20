# aha-flow

`aha-flow` 是 `uxspeech-flow` 的原生 Qt 桌面实现，使用 **C++17 + Qt 6 Widgets + CMake**。目标是减少客户端启动时间和内存占用，语音识别仍由原来的服务端引擎处理。

已实现置顶无边框窗口、展开/紧凑模式、拖动和缩放、自动开始识别、麦克风按钮、计时、音量条、分轮转写与设置面板。支持 AHA 中间预览、物理停顿与语义轮次状态、流式文本纠错，以及可配置的原始录音/VAD FLAC 缓存。应用不包含 WebEngine、Electron 或 Python 运行时。

## Ubuntu 24.04 直接使用

Linux 发布产物为 `aha-flow-版本-ubuntu24.04-x86_64.tar.gz`，面向 **Ubuntu 24.04 x86_64 桌面环境**。包内携带 Qt、图形平台插件、音频插件和相关运行库；无需安装 Qt、开发工具、FUSE，也无需 `sudo`。

解压后运行包根目录的 `aha-flow`：

```bash
tar -xzf aha-flow-0.2.0-ubuntu24.04-x86_64.tar.gz
./aha-flow-0.2.0-ubuntu24.04-x86_64/aha-flow
```

请保留完整目录，通过根目录启动器运行；不要单独复制 `bin/aha-flow`。启动器只为当前进程配置随包库，不修改系统目录或 shell 配置。支持 X11 和 Wayland 插件，使用桌面已有的显示、声音服务及系统驱动。删除解压目录即可移除应用和随包库；本机设置、录音缓存另行管理。不承诺其他发行版或 CPU 架构。

以下安装步骤仅用于**从源码开发和构建**，普通使用者不需要执行。

## 开发环境

要求 CMake 3.21+、Qt 6.4+、支持 C++17 的编译器；Windows 使用 Qt 6.8.3 / MSVC 2022。

Ubuntu 24.04 / Debian 13：

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git \
  qt6-base-dev qt6-multimedia-dev qt6-websockets-dev qt6-svg-dev
```

FLAC 1.5.0 和 libsamplerate 0.2.2 由 CMake 下载、校验 SHA-256 并静态编译，无需额外安装它们的开发包。首次配置需要能访问 GitHub。可选安装 `qtcreator`，打开根目录的 `CMakeLists.txt`。

Windows 安装 Visual Studio 2022 的“使用 C++ 的桌面开发”，以及 Qt 6.8.3 **MSVC 2022 64-bit** 套件，包含 Qt Multimedia、Qt WebSockets、Qt SVG。

## 构建与运行

Linux：

```bash
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
./build/debug/bin/aha-flow
```

从 Qt 官方安装器安装时，配置追加 `-DCMAKE_PREFIX_PATH=/实际路径/Qt/版本/gcc_64`。

Windows PowerShell（Qt 路径按实际安装位置修改）：

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build/windows --config Release --parallel 2
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
ctest --test-dir build/windows -C Release --output-on-failure
& .\build\windows\bin\Release\aha-flow.exe
```

窗口先显示，再异步连接引擎；只有收到 `session.updated` 后才开启系统默认麦克风。设备不支持目标格式时，客户端混合声道并流式重采样为 **24 kHz、单声道、PCM16LE**，每 100 ms 发送一次音频。

## 界面与设置

展开窗口待机默认 360 × 232，首次转写在默认尺寸下展开至 360 × 300，悬浮条为 360 × 44（均为逻辑像素）。标题、运行状态和固定宽度的会话计时处于同一行；底部保留真实输入电平、引擎、录音保存状态与停止操作。正文 14px、行距 22px，通过“已确认 / 实时转写”标签区分识别结果，标签不进入复制的正文。

主窗左边、右边、底边及底部两角均可拖动缩放，最低 360 × 220，不显示常驻斜线手柄。用户调整后不随文字增长改变尺寸，收起后再展开恢复原尺寸；新增内容仅在转写区滚动。

设置窗为 480 × 360，极小屏幕限制为可用高度的 85%。标题、页签和底部完成按钮固定，仅中间设置内容滚动，切换页签不改变窗口高度。模型与语言、API Key、录音切片时长采用同行排版；所有原有字段和自动保存行为保留。版本信息移至“外观”。

界面沿用「苍渊·白垣」双主题和 MANAS 的紧凑层级，主题立即生效并记住选择。外轮廓圆角 8px，输入框与普通按钮 4px，局部底板 3px；描边 1px、焦点 1.5px。输入高 32px，停止按钮 104 × 36，图标热区 28 × 28。开关轨道 32 × 16，点击区域仍为 40 × 32。图标来自 [Figma 主设计](https://www.figma.com/design/gegARxke9qjJq0xydnJ3fN?node-id=45-365)，作为本地资源打包，启动不访问 Figma。

字体优先使用已安装的 Noto Sans SC / 思源黑体与 Inter；缺少时使用系统中文黑体和西文字体，并检查中文字符支持。保留 Qt 默认字体微调策略，不附加整体缩放。

Qt 6 自动跟随系统显示缩放，Windows 默认支持每显示器 DPI 感知。200% 缩放时，14px 正文按 28 个物理像素绘制，窗口逻辑尺寸保持不变。字体使用平台默认微调策略，让 Qt 在高 DPI 下选择 DirectWrite；不强制 Full Hinting，也不叠加额外缩放。

配置自动保存，**下一次开始识别时生效**。主题切换立即生效；识别与缓存配置使用开始时的会话快照。停止/重新开始会创建新的缓存会话，旧会话的迟到事件不会覆盖新会话。

| 配置 | 默认值 / 行为 |
| --- | --- |
| 服务基地址 | `https://192.168.0.222:10000`；支持 HTTP/HTTPS、WS/WSS 和省略协议的地址，省略协议使用 HTTPS |
| 模型 | `default-audio`；打开设置时异步获取 `/v1/models`，保留已选模型 |
| 语言 | 留空使用服务端默认值 |
| API Key | 可选，用于识别引擎的 Bearer 认证 |
| 允许自签名证书 | 默认开启，与原版一致；关闭后验证引擎和纠错服务的 TLS 证书 |
| AHA 扩展协议 | 默认开启；连接附带 `x_aha=v1`，官方 OpenAI 域名除外 |
| 文本纠错 | 默认关闭；地址 `http://192.168.0.222:10002/v1/responses` |
| 录音缓存 | 默认开启，保存原始音频及有完整边界的 VAD 分段 |
| 缓存目录 | 系统缓存目录下的 `aha-flow/data-bin`，可输入绝对路径或浏览选择 |
| 原始录音切片时长 | 默认 300 秒，可设为 1–3600 秒 |
| 界面主题 | 白垣浅色 / 苍渊深色，默认白垣，即时切换并持久化 |

成功连接的服务地址保留最近 5 条。设置使用系统 `QSettings` 保存；面板中填写的 API Key 也保存在本机配置中，未接入系统密钥库。环境变量 `AHA_FLOW_API_KEY` 和 `--engine` 仅覆盖本次运行，不写回设置。

同一轮的中间预览随 revision 更新；`interim_cleared` 后保留已有预览，正式增量到达后替换预览。物理停顿显示“语义轮次未结束”，语义结束后显示“转写中”，最终结果固定为一段文本。

启用纠错后，按轮次调用 Responses SSE 接口，使用 `AgenticASR-Refiner`、`temperature=0`、`stream=true`、`store=false`。纠错增量显示进度提示，正文保持主题文字色，隐藏 `<KEY>` 实体标记；超时、截断、空结果或请求失败时恢复识别原文并显示原因。识别 API Key 不发送给纠错服务。

## 缓存格式与生命周期

例如将目录设为 `/data/aha-flow`，一次会话产生：

```text
/data/aha-flow/
├── raw/20261001_120000/
│   ├── 000001.flac
│   ├── metadata.jsonl
│   ├── session.json
│   ├── events.jsonl
│   ├── raw-slices.jsonl
│   └── boundaries.jsonl
└── vad/20261001_120000/
    ├── 000001.flac
    └── metadata.jsonl
```

同一秒重复开始时追加目录后缀，避免覆盖。录音期间另有 `raw/.../pending.pcm`，持续写入已经发送的 PCM。正常停止或关闭窗口时，完成最后一个不足整片的 FLAC，并删除暂存 PCM；写盘/编码失败时保留暂存文件并提示错误。异常终止可能留下 PCM 或 `.tmp` 文件，目前没有自动恢复工具。

原始 FLAC 连续覆盖录音时间轴，解码后与发送的 PCM 一致。原始 `metadata.jsonl` 每行包含 `file_name`、`id`、`seconds`；`seconds` 是相对该切片的秒数区间，服务端边界经过裁剪、排序与合并。VAD 元数据包含 `file_name`、`id`、识别原始 `text`；VAD 文件在真实语义轮次边界前后各保留最多 1 秒已有音频。缺少有效开始/结束时间或非空原文的轮次不生成 VAD 文件，原始录音仍保存。

事件日志只记录协议类型、事件/轮次标识、时间边界和处理阶段，不记录 API Key 或音频载荷。缓存禁用时不创建录音目录、元数据或日志。目录不存在时自动创建；目录不可写、音频积压超过 5 秒或事件队列达到上限时，明确报错并停止识别。

采集/网络与缓存编码分别在工作线程运行。原始 PCM、边界索引在磁盘上，FLAC 编码分块进行；界面仅保留最近 1000 轮、约 1 MiB 文字，超出时显示历史截断提示。**缓存没有自动清理策略**，历史录音需自行管理。

## 命令行与连接检查

不自动开始识别：

```bash
./build/debug/bin/aha-flow --no-autostart
```

无显示服务也可以检查引擎和 Realtime 配置，不打开麦克风：

```bash
./build/debug/bin/aha-flow --check-engine
./build/debug/bin/aha-flow --check-session
./build/debug/bin/aha-flow --check-engine --engine https://其他地址:10000 --allow-self-signed
```

成功返回 0，失败返回 1。模型列表和会话配置连接的总时限为 10 秒；单个协议响应限制 1 MiB。`--allow-self-signed` 本次运行忽略证书错误，不修改系统证书设置。

支持 `HTTP_PROXY` / `HTTPS_PROXY` 及对应小写变量、`NO_PROXY` / `no_proxy`，也读取 `.bashrc` 中不含变量展开的代理字面量，最后使用系统代理。支持 HTTP CONNECT 与 SOCKS5；`HTTPS_PROXY=http://...` 可以代理 HTTPS/WSS，引擎连接会明确拒绝 `https://` 形式的加密代理地址。

回放已有的 **裸 PCM16LE / 24 kHz / 单声道** 文件，以正常节奏发送到引擎并验证缓存：

```bash
./build/debug/bin/aha-flow --replay-pcm /absolute/path/speech.pcm \
  --cache-dir /absolute/path/cache
# 或 --no-cache；两项缓存参数用于本次回放，不修改设置
```

回放结束后等待 30 秒接收最终结果，再收尾退出。GUI 的缓存配置请使用设置面板。

## 验证与性能

离线测试覆盖设置持久化、原版 VAD 事件序列、预览与正式文本、音频重采样尾帧、FLAC 无损回读、VAD 跨切片、禁用缓存、停止/重启、窗口行为，以及 SSE 碎片、失败恢复和取消。测试使用本机 HTTP/WebSocket 模拟服务，不依赖局域网引擎或麦克风。

可选验证：

```bash
# 模拟一小时录音，检查缓存内存不持续增长（Linux）
AHA_FLOW_LONG_CACHE_TEST=1 ./build/release/tests/aha_cache_tests longRecordingMemory
# 显式检查真实文本纠错接口
AHA_FLOW_LIVE_CORRECTION_URL=http://192.168.0.222:10002/v1/responses \
  ./build/release/tests/aha_correction_tests liveEngine
# 输出窗口首帧耗时；不连接引擎，5 秒后退出
./build/release/bin/aha-flow --benchmark
```

2026-10-01，在 Debian 13、Qt 6.8.2、Electron 41.2.0 和同一 Xvfb 显示环境下，Release 版本的一次离线启动测量：

| 客户端 | 启动至窗口首帧 | 启动后进程树 PSS | 进程数 |
| --- | ---: | ---: | ---: |
| aha-flow | 72.4 ms | 46.0 MiB | 1 |
| uxspeech-flow | 988.2 ms | 309.8 MiB | 7 |

首帧耗时从外部进程启动计时；显示首帧后等待 0.5 秒，累加 `/proc/.../smaps_rollup` 的 PSS，避免重复计算共享内存。两者关闭自动录音，使用独立配置目录；文件系统缓存未清空。这是单次参考值，不代表冷启动或录音期间的内存。模拟一小时连续音频的缓存模块 RSS 从 5 分钟时的 12.4 MiB 增至 13.0 MiB，此项没有实时等待一小时，也未包含真实录音设备和大量转写事件。

本机 Debug/Release 的 8 组离线测试、真实识别引擎 PCM 回放和真实纠错请求通过；真实回放生成的原始 FLAC 解码后与输入 PCM 逐字节一致。当前开发环境没有可用麦克风，实际设备采集与 Windows 包仍需在对应桌面环境验证。

## 目录结构

```text
aha-flow/
├── .github/workflows/ci.yml   # Linux / Windows 构建、测试、产物
├── cmake/                   # 固定版本音频依赖
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── app/                 # 入口、命令行与组装
│   ├── ui/                  # Qt Widgets 窗口与设置
│   ├── core/                # 配置、协议状态与会话消息
│   ├── network/             # HTTP、WebSocket、SSE 和代理
│   ├── audio/               # PCM 转换与重采样
│   ├── session/             # 采集、工作线程与会话生命周期
│   └── storage/             # FLAC、元数据与缓存队列
└── tests/                   # 离线测试与原版事件回放样本
```

## GitHub Actions 与打包

推送、PR 和手动触发执行离线构建/测试并上传产物：Ubuntu 24.04 x86_64 自带运行库的 `.tar.gz`，Windows 2022 / Qt 6.8.3 / MSVC 2022 的 `.zip`。Windows 部署步骤收集 Qt DLL、平台和多媒体插件及编译器运行库。工作流不访问局域网引擎，无需引擎密钥；未自动发布 GitHub Release。

Windows 还通过原生 `windows` 平台验证 100%、125%、200% 缩放，上传 `aha-flow-windows-ui-checks`：包含浅色/深色窗口截图、`fonts.json` 中各控件的请求字体与中文实际字形字体、字体引擎日志及测试结果。常规离线测试继续使用 `offscreen`；原生截图用于核查 Windows 字体路径，不替代实际显示器上的视觉确认。

Linux 便携包通过 CMake 递归收集动态库，并显式部署 Qt、GStreamer 和 TLS 动态加载依赖；不捆绑 glibc 和系统动态加载器。CI 在没有 Qt 的全新 Ubuntu 24.04 容器中检查所有随包 ELF 文件的依赖、包含空格和中文的解压路径、TLS/多媒体后端及无显示启动，再安装 Xvfb 和 PulseAudio 检查 xcb 窗口启动与虚拟音源采集。验证不访问引擎、不打开真实麦克风；这些测试工具只安装在验证容器中。Windows 解压后运行 `bin/aha-flow.exe`。依赖的许可证和 Ubuntu 软件包版本清单随包附在 `share/aha-flow/licenses`。

应用图标沿用「苍渊·白垣」配色，将字母 a、语音波形与流动尾笔结合；矢量源文件位于 `src/app/assets/aha-flow.svg`。窗口图标内置 16–512px PNG，Windows EXE 内嵌多尺寸 ICO。Linux 包携带桌面入口及 hicolor 图标；如需安装到用户应用菜单，先以 `cmake --preset release -DCMAKE_INSTALL_PREFIX="$HOME/.local"` 配置，再构建并执行 `cmake --install build/release --component Runtime`。仅解压并直接运行时，窗口仍使用内置图标。

普通源码构建仍可使用系统 Qt。本机生成 Linux 便携包必须在 Ubuntu 24.04 x86_64 中执行，额外构建依赖如下（只安装在构建机）：

```bash
sudo apt install -y qt6-wayland libssl-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

便携包构建：

```bash
cmake --preset release -DAHA_PORTABLE_LINUX=ON
cmake --build --preset release --parallel 2
ctest --preset release
cd build/release
cpack
```

Windows 在 `build/windows` 执行 `cpack -C Release -G ZIP`。项目已经初始化 Git；设置 GitHub remote 后推送即可触发工作流。
