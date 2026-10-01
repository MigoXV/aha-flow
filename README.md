# aha-flow

`aha-flow` 是面向快速启动与低常驻内存的原生桌面语音转写客户端，使用 **C++17 + Qt 6 Widgets + CMake**，逐步迁移 `uxspeech-flow` 的功能。

目前完成项目初始化、原生置顶窗口、异步模型列表获取、引擎地址配置、离线测试与 GitHub Actions。**尚未实现麦克风采集、实时转写、FLAC 缓存和文本纠错。**

## 环境安装

要求 CMake 3.21+、Qt 6.4+ 和支持 C++17 的编译器。

Ubuntu 24 / Debian 13：

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git qt6-base-dev
```

当前代码仅使用 Qt Core、Widgets 和 Network。后续接入音频与实时连接时安装：

```bash
sudo apt install -y qt6-multimedia-dev qt6-websockets-dev libflac-dev libsamplerate0-dev
```

可选 IDE：`sudo apt install -y qtcreator`，打开根目录的 `CMakeLists.txt` 即可。

Windows：安装 Visual Studio 2022 的“使用 C++ 的桌面开发”，通过 Qt 安装器安装 Qt 6.8.3 的 **MSVC 2022 64-bit** 套件。Qt 与编译器套件必须匹配。

## 构建与运行

Linux，或已配置编译器和 Ninja 的终端：

```bash
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
./build/debug/bin/aha-flow
```

如果从 Qt 官方安装器安装，配置时追加 `-DCMAKE_PREFIX_PATH=/实际路径/Qt/版本/gcc_64`；Windows 的路径指向 `msvc2022_64`。

Windows PowerShell 示例（Qt 路径按实际安装位置修改）：

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build/windows --config Release --parallel 2
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"
ctest --test-dir build/windows -C Release --output-on-failure
& .\build\windows\bin\Release\aha-flow.exe
```

## 引擎连接

默认引擎地址：`https://192.168.0.222:10000`，默认模型：`default-audio`。

应用先显示窗口，再异步请求 `/v1/models`。网络失败不阻止窗口使用；可修改地址并重新获取模型。当前引擎使用自签名证书，需要勾选“允许不受信任的 TLS 证书（局域网）”，或者显式传入参数：

```bash
./build/debug/bin/aha-flow --allow-self-signed
```

证书验证默认开启。上述选项仅影响本应用发起的引擎请求，开启后忽略该请求的 TLS 证书错误，不修改系统证书设置。

无显示服务也能检查真实引擎：

```bash
./build/debug/bin/aha-flow --check-engine --allow-self-signed
./build/debug/bin/aha-flow --check-engine --engine https://其他地址:10000
```

成功打印模型列表并返回 0，失败返回 1。如需认证，可设置环境变量 `AHA_FLOW_API_KEY`；应用不保存或打印密钥。请求设有 10 秒总时限和 1 MiB 响应上限。

## 目录结构

```text
aha-flow/
├── .github/workflows/ci.yml   # Linux / Windows 构建、测试、产物
├── CMakeLists.txt            # 项目配置、测试与打包
├── CMakePresets.json         # Debug / Release 构建预设
├── src/
│   ├── app/                  # 程序入口、参数解析与组装
│   ├── ui/                   # Qt Widgets 界面
│   ├── core/                 # 引擎配置与协议地址规则
│   └── network/              # 异步引擎 HTTP 客户端
└── tests/                    # 协议地址、HTTP 响应与窗口启动验证
```

后续有实际代码时添加 `src/audio` 与 `src/storage`，分别负责采集/重采样和录音缓存。核心层不依赖 UI，网络层通过信号报告结果。当前未引入 WebEngine、QML 或额外后台进程。

## GitHub Actions 与打包

推送、PR 和手动触发均执行：

- Ubuntu 24.04：使用发行版 Qt，构建、离线测试并上传 `.tar.gz`。
- Windows 2022：使用 Qt 6.8.3 / MSVC 2022，构建、离线测试并上传包含 Qt DLL、平台插件和编译器运行库的 `.zip`。

Linux 产物需要目标机器安装对应 Qt 运行库，**当前不是独立 AppImage**。Windows 解压后运行 `bin/aha-flow.exe`。工作流不访问局域网引擎、不录音、不发布 GitHub Release，无需配置引擎密钥。

本机打包：

```bash
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release
cd build/release
cpack
```

Windows 在 `build/windows` 目录执行 `cpack -C Release -G ZIP`。

## 后续开发

1. 使用 Qt Multimedia 采集麦克风，检查设备格式并转换为 24 kHz、单声道、PCM16。
2. 使用 Qt WebSockets 接入 `/v1/realtime?intent=transcription&x_aha=v1`，完成会话配置后再采集音频；当前只实现地址生成。
3. 按轮次迁移 AHA 中间预览、物理结束边界、语义结束和正式转写事件。
4. 音频写盘与 FLAC 编码放到工作线程，使用有界队列与可靠的退出收尾。
5. 接入设置持久化、文本纠错与发布打包，测量首次窗口显示时间与空闲常驻内存。
