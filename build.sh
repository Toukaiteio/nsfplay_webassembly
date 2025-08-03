#!/bin/bash

# NSF to WAV WebAssembly 构建脚本
# 需要先安装 Emscripten SDK

set -e

echo "构建 NSF to WAV WebAssembly 模块..."

# 检查 Emscripten 是否可用
if ! command -v emcc &> /dev/null; then
    echo "错误: 未找到 emcc 命令。请先安装并激活 Emscripten SDK。"
    echo "参考: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# 创建构建目录
mkdir -p build
cd build

# 使用 emcmake 配置 CMake
echo "配置 CMake..."
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建项目
echo "编译项目..."
emmake make -j$(nproc) VERBOSE=1

echo "构建完成！"
echo "输出文件:"
ls -la nsf2wav_wasm.*

cd ..