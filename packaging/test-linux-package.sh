#!/bin/bash
# 在没有 Qt 的 Ubuntu 24.04 中执行；由 CI 和本地 Docker 验证共用。
set -euo pipefail
archive=$(readlink -f "$1")
probe=$(readlink -f "$2")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/包含 空格" "$work/home"
tar -xzf "$archive" -C "$work/包含 空格"
app=$(find "$work/包含 空格" -mindepth 1 -maxdepth 1 -type d)
test -x "$app/aha-flow"
export HOME="$work/home"
export XDG_RUNTIME_DIR="$work/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
export LD_LIBRARY_PATH="$app/lib"
# 不只检查主程序：插件缺失的间接依赖也必须使验证失败。
while IFS= read -r -d '' binary; do
    dependencies=$(ldd "$binary" 2>&1) || {
        printf '%s\n%s\n' "$binary" "$dependencies"
        exit 1
    }
    if [[ "$dependencies" == *"not found"* ]]; then
        printf '%s\n%s\n' "$binary" "$dependencies"
        exit 1
    fi
done < <(find "$app/lib" "$app/plugins" "$app/libexec" "$app/bin" -type f \
    \( -name '*.so*' -o -name aha-flow -o -name gst-plugin-scanner \) -print0)
unset LD_LIBRARY_PATH
cd /
"$app/aha-flow" --version
LD_LIBRARY_PATH="$app/lib" QT_PLUGIN_PATH="$app/plugins" QT_MEDIA_BACKEND=gstreamer \
    G_DEBUG=fatal-criticals \
    GST_PLUGIN_SYSTEM_PATH_1_0="$app/lib/gstreamer-1.0" \
    GST_PLUGIN_SCANNER_1_0="$app/libexec/gst-plugin-scanner" \
    timeout 20 "$probe"
QT_QPA_PLATFORM=offscreen timeout 20 "$app/aha-flow" --smoke-test
# 第二阶段安装 Xvfb 后验证真实 xcb 平台路径，不依赖局域网或麦克风。
if command -v xvfb-run >/dev/null; then
    QT_QPA_PLATFORM=xcb timeout 20 xvfb-run -a "$app/aha-flow" --smoke-test
fi
