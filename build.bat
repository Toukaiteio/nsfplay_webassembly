@echo off
REM NSF to WAV WebAssembly 构建脚本 (Windows)
REM 需要先安装 Emscripten SDK

echo 构建 NSF to WAV WebAssembly 模块...

REM 检查 Emscripten 是否可用
where emcc >nul 2>nul
if %errorlevel% neq 0 (
    echo 错误: 未找到 emcc 命令。请先安装并激活 Emscripten SDK。
    echo 参考: https://emscripten.org/docs/getting_started/downloads.html
    pause
    exit /b 1
)

REM 创建构建目录
if not exist build mkdir build
cd build

REM 使用 emcmake 配置 CMake
echo 配置 CMake...
call emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake 配置失败
    pause
    exit /b 1
)

REM 构建项目
echo 编译项目...
call emmake make VERBOSE=1
if %errorlevel% neq 0 (
    echo 编译失败
    pause
    exit /b 1
)

echo 构建完成！
echo 输出文件:
dir nsf2wav_wasm.*

cd ..
pause