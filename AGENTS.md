# 开发约定

- 本项目使用 C++17、Qt 6 Widgets 和 CMake，不引入 Electron、WebEngine 或 Python 运行时。
- 修改前理解入口与调用链，优先做最小必要改动。
- `src/app` 只负责启动与组装；`src/ui` 负责界面；`src/core` 不依赖界面；`src/network` 负责异步网络请求。
- 窗口启动不等待网络；禁止在 UI 线程执行同步网络、音频编码或长时间文件操作。
- 音频缓存和界面历史必须有容量限制；不要把整段录音长期保留在内存中。
- README.md 使用中文。不得提交 API Key、录音或本机配置。
- 常规验证：`cmake --preset debug`、`cmake --build --preset debug`、`ctest --preset debug`。
- CI 不依赖局域网引擎或麦克风；真实引擎检查由开发者显式执行。
