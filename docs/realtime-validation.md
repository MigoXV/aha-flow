# OpenAI Realtime 官方接口验证记录

验证时间：2026-11-03。客户端使用本分支的 `RealtimeClient`，连接 `https://api.openai.com`，开启证书校验；未连接 AHA 引擎。密钥仅通过进程环境提供，未保存密钥或录音到仓库。

## 配置验证

对 8 个模型标识执行 85 次独立会话配置检查：67 次接受，18 次拒绝，其中 1 次为组织无模型访问权限。每个可建立基础会话的模型覆盖基础配置、音量 VAD、语义 VAD、高积极程度、音量 VAD 数值参数、关闭 VAD、提示词、关键词、出字延迟、近场降噪、远场降噪与强度、关闭降噪、请求对数概率。无访问权限的模型仅检查基础配置。

| 模型 | 音量 / 语义 VAD | 关闭 VAD | 提示词 | 关键词 | 出字延迟 |
| --- | --- | --- | --- | --- | --- |
| `whisper-1` | 接受 / 接受 | 接受 | 接受 | 拒绝 | 拒绝 |
| `gpt-4o-mini-transcribe` | 接受 / 接受 | 接受 | 接受 | 拒绝 | 拒绝 |
| `gpt-4o-mini-transcribe-2025-12-15` | 接受 / 接受 | 接受 | 接受 | 拒绝 | 拒绝 |
| `gpt-4o-transcribe` | 接受 / 接受 | 接受 | 接受 | 拒绝 | 拒绝 |
| `gpt-4o-transcribe-diarize` | 未验证：组织无模型访问权限 | 同左 | 同左 | 同左 | 同左 |
| `gpt-live-transcribe` | 拒绝 / 拒绝 | 接受 | 接受 | 接受 | 接受 |
| `gpt-transcribe` | 接受 / 接受 | 接受 | 接受 | 接受 | 拒绝 |
| `gpt-realtime-whisper` | 拒绝 / 拒绝 | 接受 | 拒绝 | 拒绝 | 接受 |

上述 7 个可建立基础会话的模型均接受近场降噪、远场降噪与 `strength=0.5`、关闭降噪，以及请求 `logprobs`。接受的音量 VAD 数值组合为 `threshold=0.6`、`prefix_padding_ms=300`、`silence_duration_ms=500`；语义 VAD 使用 `eagerness=high`。

“接受”仅表示收到 `session.updated`，不表示已验证参数的识别效果或实际概率输出。部分被接受的字段（例如出字延迟）没有在有效配置中回显，本次不据此推断其已生效。此表是该账号在验证时的结果，不用于在客户端硬编码模型限制。

拒绝示例：

- 不支持 VAD：`invalid_value`，参数路径 `session.audio.input.turn_detection`，消息为 `Turn detection is not supported for this transcription model.`
- 不支持关键词：`invalid_parameter`，参数路径 `session.audio.input.transcription.keywords`。
- 不支持出字延迟：`invalid_value`，参数路径 `session.audio.input.transcription.delay`。

每次配置独立建立连接；无 VAD 模型在被拒绝后仍能以关闭 VAD 的配置连接。离线模拟测试另外验证了同一客户端对象取消被拒绝的字段后重新连接。

## 音频与生产入口

使用一段短英文合成语音、24 kHz 单声道 PCM16LE，结尾追加 2 秒静音。仅验证协议闭环，不作为真实麦克风、口音或噪声环境下的准确率证据。

- `gpt-4o-mini-transcribe`：音量 VAD 返回非空最终转写，通过。
- `gpt-4o-mini-transcribe`：关闭 VAD、手动提交返回非空最终转写，通过。
- `gpt-live-transcribe`：通过实际应用的 `--replay-pcm` 入口，在默认关闭 VAD 的情况下接收增量，回放结束提交后收到最终转写，并以退出码 0 收尾，通过。

本次未实测其他模型的音频、语义 VAD 的实际断句质量、中文、多语言、概率值、真实麦克风或 Windows 桌面。相关配置接受情况与实际效果分开记录。

## 可复现入口

构建后参考 README 的 `AHA_FLOW_LIVE_REALTIME`、`AHA_FLOW_API_KEY`、`AHA_FLOW_LIVE_PCM` 和 `AHA_FLOW_LIVE_MODEL` 运行官方接口测试。常规 CTest 跳过真实接口用例，不依赖密钥或麦克风。
