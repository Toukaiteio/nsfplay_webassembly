# NSF 实时播放器 WebAssembly

这是一个基于 Emscripten 的 WebAssembly 项目，它将 NSFPlay 的核心逻辑提取出来，提供了在浏览器中实时流式播放 NSF 音频以及将 NSF 文件转换为 WAV 文件的双重功能。

## 功能特性

- 🎵 **实时流式播放**：直接在浏览器中播放 NSF 文件，无需预先转换。
- 🌊 **WAV 转换**：支持将 NSF 文件离线转换为高质量的 WAV 文件。
- 🎧 **高级音频控制**：支持多轨道切换、音量调节和实时波形显示。
- ⚙️ **可配置**：可自定义采样率、播放长度、淡出时间等参数。
- 🌐 **纯浏览器端**：所有处理都在客户端完成，无需服务器。

## 项目结构


- `CMakeLists.txt`: 项目的构建配置文件。
- `nsf2wav_wasm.cpp`: C++ 核心逻辑和 WebAssembly 接口。
- `example.html`: 一个功能完整的实时播放器演示页面。
- `build.sh` / `build.bat`: 用于快速构建项目的脚本。
- `xgm/`: 核心的 NSF 播放器引擎库。
- `vcm/`: `xgm` 的配置管理依赖库。

## 构建要求

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
- CMake 3.10+
- 支持 C++17 的编译器

## 构建步骤

1.  **安装并激活 Emscripten SDK**：
    ```bash
    # 下载并安装 Emscripten
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    ```

2.  **构建 WebAssembly 模块**：
    ```bash
    # 在 emsc_webassembly 目录下
    chmod +x build.sh
    ./build.sh
    ```

    构建完成后，会在 `build/` 目录下生成：
    - `nsf2wav_wasm.js` - JavaScript 胶水代码
    - `nsf2wav_wasm.wasm` - WebAssembly 二进制文件

## 使用方法

主要的使用方式是实时流式播放，具体实现可以参考 `example.html`。以下是关键的 JavaScript 逻辑摘要：

```javascript
// 全局变量
let nsfPlayer = null;
let audioContext = null;
let scriptProcessor = null;
let isPlaying = false;
const bufferSize = 4096;
const sampleRate = 44100;
const channels = 2;

// 1. 初始化 WebAssembly 模块
async function initWasm() {
    nsfPlayer = await NSFPlayer(); // 这是从 nsf2wav_wasm.js 导出的
}

// 2. 加载 NSF 文件
async function loadNSF() {
    const file = document.getElementById('nsfFile').files[0];
    const arrayBuffer = await file.arrayBuffer();
    const nsfData = new Uint8Array(arrayBuffer);

    // 初始化 C++ 播放器
    nsfPlayer.ccall('init_nsf_player', 'number', [], []);

    // 加载数据
    const dataPtr = nsfPlayer._malloc(nsfData.length);
    nsfPlayer.HEAPU8.set(nsfData, dataPtr);
    nsfPlayer.ccall('load_nsf_data', 'number', ['number', 'number'], [dataPtr, nsfData.length]);
    nsfPlayer._free(dataPtr);
    
    // 禁用默认时间限制，允许无限循环播放
    nsfPlayer.ccall('nsf_disable_time_limit', 'void', [], []);
    
    // 或者使用自定义参数：
    // nsfPlayer.ccall('nsf_set_playback_options', 'void', ['number', 'number', 'number'], [-1, 3000, 0]);
    
    // 初始化音频并设置播放器选项
    if (await initAudio()) {
        nsfPlayer.ccall('nsf_player_set_options', 'void', ['number', 'number'], [sampleRate, channels]);
    }
}

// 3. 初始化 Web Audio API
async function initAudio() {
    audioContext = new AudioContext({ sampleRate });
    scriptProcessor = audioContext.createScriptProcessor(bufferSize, 0, channels);
    
    scriptProcessor.onaudioprocess = (e) => {
        if (isPlaying) {
            generateAudioData(e.outputBuffer);
        }
    };
    
    scriptProcessor.connect(audioContext.destination);
}

// 4. 在 onaudioprocess 回调中实时生成音频
function generateAudioData(outputBuffer) {
    const frameCount = bufferSize;
    const audioDataSize = frameCount * channels * 2; // 16-bit
    const audioPtr = nsfPlayer._malloc(audioDataSize);

    // 调用 C++ 函数渲染音频块
    const result = nsfPlayer.ccall('nsf_render_audio', 'number',
        ['number', 'number', 'number'], [currentTrack, frameCount, audioPtr]);

    if (result > 0) {
        const pcmData = new Int16Array(nsfPlayer.HEAPU8.buffer, audioPtr, frameCount * channels);
        // 将 PCM 数据复制到 outputBuffer
        // ...
    }

    nsfPlayer._free(audioPtr);
}

// 5. 控制播放
function togglePlay() {
    if (audioContext.state === 'suspended') {
        audioContext.resume();
    }
    isPlaying = !isPlaying;
    // ... 更新 UI
}
```

## C API 函数参考

- `init_nsf_player()`: 初始化播放器实例。
- `load_nsf_data(dataPtr, size)`: 加载 NSF 文件数据。
- `get_nsf_info(infoPtr)`: 获取 NSF 文件信息。
- `nsf_player_set_options(sampleRate, channels)`: **[新增]** 为流式播放设置采样率和声道。
- `nsf_set_playback_options(playtimeMs, fadeMs, loopCount)`: **[新增]** 设置播放参数，支持禁用默认时间限制。
- `nsf_disable_time_limit()`: **[新增]** 禁用默认播放时间限制，允许无限循环播放。
- `nsf_set_track(trackNumber)`: 设置当前播放的轨道。
- `nsf_render_audio(trackNumber, frameCount, outputPtr)`: **[新增]** 渲染一个音频数据块用于流式播放。
- `nsf_reset()`: 重置播放器状态。
- `nsf_to_wav(...)`: 将整个轨道转换为 WAV 数据。
- `cleanup()`: 清理并释放所有资源。

## 播放时间控制

本项目现在支持灵活控制播放时间，避免默认的120秒限制：

### 禁用时间限制
使用 `nsf_disable_time_limit()` 函数可以完全禁用默认的播放时间限制，允许NSF文件按照其原始设计进行无限循环播放。

### 自定义播放参数
使用 `nsf_set_playback_options(playtimeMs, fadeMs, loopCount)` 可以精确控制：
- `playtimeMs`: 播放时间（毫秒），传入-1表示无限播放
- `fadeMs`: 淡出时间（毫秒）
- `loopCount`: 循环次数，0表示无限循环

### 使用示例

```javascript
// 方法1: 完全禁用时间限制
nsfPlayer.ccall('nsf_disable_time_limit', 'void', [], []);

// 方法2: 自定义播放参数 - 无限循环
nsfPlayer.ccall('nsf_set_playback_options', 'void', 
    ['number', 'number', 'number'], [-1, 3000, 0]);

// 方法3: 自定义播放参数 - 10分钟播放，5秒淡出
nsfPlayer.ccall('nsf_set_playback_options', 'void', 
    ['number', 'number', 'number'], [600000, 5000, 1]);
```

## 示例

查看 `example.html` 文件获取一个功能完整的实时播放器示例，包括：
- 文件选择和加载
- NSF 信息显示
- 轨道切换和音量控制
- 实时波形图
- 播放/暂停/停止控制
- **无限循环播放支持**

## 致谢

本项目的核心音频处理和 NSF 解析逻辑大量借鉴和修改自 **nsfplay** 项目。我们对原作者 **bbbradsmith** 的出色工作表示衷心的感谢。

- **原项目地址**: [https://github.com/bbbradsmith/nsfplay](https://github.com/bbbradsmith/nsfplay)

## 许可证

本项目继承原 NSFPlay 项目的开源许可证。请参考原项目以获取详细信息。