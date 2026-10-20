#!/bin/sh
set -eu
app_dir=$(CDPATH= cd -- "$(dirname -- "$(readlink -f -- "$0")")" && pwd)
# 这些设置只影响当前进程；不修改用户的 shell 配置或系统搜索路径。
export LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$app_dir/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$app_dir/plugins/platforms"
export QT_MEDIA_BACKEND=gstreamer
export GST_PLUGIN_SYSTEM_PATH_1_0="$app_dir/lib/gstreamer-1.0"
export GST_PLUGIN_PATH_1_0="$app_dir/lib/gstreamer-1.0"
export GST_PLUGIN_SCANNER_1_0="$app_dir/libexec/gst-plugin-scanner"
export GST_REGISTRY_1_0="${XDG_CACHE_HOME:-$HOME/.cache}/aha-flow/gstreamer-registry.bin"
exec "$app_dir/bin/aha-flow" "$@"
