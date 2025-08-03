/**
 * NSF to WAV WebAssembly Interface
 * 基于nsfplay项目的核心逻辑，提供NSF文件解析和WAV转换功能
 */

#include <emscripten.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <algorithm>

#include "./xgm/xgm.h"

// WAV文件头结构 - 确保正确的字节对齐
#pragma pack(push, 1)
struct WAVHeader {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // 文件大小 - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 16
    uint16_t audio_format;  // 1 = PCM
    uint16_t num_channels;  // 声道数
    uint32_t sample_rate;   // 采样率
    uint32_t byte_rate;     // 字节率
    uint16_t block_align;   // 块对齐
    uint16_t bits_per_sample; // 位深度
    char data[4];           // "data"
    uint32_t data_size;     // 数据大小
};
#pragma pack(pop)

// NSF信息结构
struct NSFInfo {
    char title[256];
    char artist[256];
    char copyright[256];
    int total_songs;
    int default_song;
    int length_ms;
    int fade_ms;
};

// 全局变量
static xgm::NSF* g_nsf = nullptr;
static xgm::NSFPlayer* g_player = nullptr;
static xgm::NSFPlayerConfig* g_config = nullptr;

extern "C" {

/**
 * 初始化NSF播放器
 */
EMSCRIPTEN_KEEPALIVE
int init_nsf_player() {
    try {
        if (g_nsf) delete g_nsf;
        if (g_player) delete g_player;
        if (g_config) delete g_config;
        
        g_nsf = new xgm::NSF();
        g_player = new xgm::NSFPlayer();
        g_config = new xgm::NSFPlayerConfig();
        
        // 设置默认配置
        (*g_config)["MASTER_VOLUME"] = 256;
        (*g_config)["APU2_OPTION5"] = 0;  // 禁用随机噪声相位重置
        (*g_config)["APU2_OPTION7"] = 0;  // 禁用随机三角波相位重置
        
        g_player->SetConfig(g_config);
        
        return 1;
    } catch (...) {
        return 0;
    }
}

/**
 * 加载NSF文件数据
 * @param data NSF文件数据指针
 * @param size 数据大小
 * @return 成功返回1，失败返回0
 */
EMSCRIPTEN_KEEPALIVE
int load_nsf_data(const uint8_t* data, int size) {
    if (!g_nsf || !g_player) {
        if (!init_nsf_player()) return 0;
    }
    
    try {
        // 创建临时文件名（WebAssembly环境中的虚拟文件）
        const char* temp_filename = "/tmp/temp.nsf";
        
        // 将数据写入虚拟文件系统
        FILE* temp_file = fopen(temp_filename, "wb");
        if (!temp_file) return 0;
        
        fwrite(data, 1, size, temp_file);
        fclose(temp_file);
        
        // 加载NSF文件
        if (!g_nsf->LoadFile(temp_filename)) {
            return 0;
        }
        
        // 设置默认播放时间（如果NSF文件中没有指定）
        if (g_nsf->playtime_unknown) {
            g_nsf->SetDefaults(120000, 3000, 1); // 120秒播放，3秒淡出，1次循环
        }
        
        // 加载到播放器
        if (!g_player->Load(g_nsf)) {
            return 0;
        }
        
        return 1;
    } catch (...) {
        return 0;
    }
}

/**
 * 获取NSF文件信息
 * @param info 输出信息结构指针
 * @return 成功返回1，失败返回0
 */
EMSCRIPTEN_KEEPALIVE
int get_nsf_info(NSFInfo* info) {
    if (!g_nsf) return 0;
    
    try {
        // 使用安全的字符串复制
        const char* title = g_nsf->title ? g_nsf->title : "";
        const char* artist = g_nsf->artist ? g_nsf->artist : "";
        const char* copyright = g_nsf->copyright ? g_nsf->copyright : "";
        
        strncpy(info->title, title, 255);
        info->title[255] = '\0';
        
        strncpy(info->artist, artist, 255);
        info->artist[255] = '\0';
        
        strncpy(info->copyright, copyright, 255);
        info->copyright[255] = '\0';
        
        info->total_songs = g_nsf->total_songs;
        info->default_song = g_nsf->start;  // 使用正确的字段名
        info->length_ms = g_nsf->default_playtime;
        info->fade_ms = g_nsf->default_fadetime;
        
        return 1;
    } catch (...) {
        return 0;
    }
}

/**
 * 将NSF转换为WAV数据
 * @param track_number 轨道号（从1开始）
 * @param length_ms 播放长度（毫秒）
 * @param fade_ms 淡出时间（毫秒）
 * @param sample_rate 采样率
 * @param channels 声道数（1或2）
 * @param output_buffer 输出缓冲区指针
 * @param buffer_size 缓冲区大小
 * @return 实际输出的字节数，失败返回0
 */
EMSCRIPTEN_KEEPALIVE
int nsf_to_wav(int track_number, int length_ms, int fade_ms, 
               int sample_rate, int channels,
               uint8_t* output_buffer, int buffer_size) {
    
    if (!g_nsf || !g_player) return 0;
    if (channels < 1 || channels > 2) return 0;
    if (track_number < 1 || track_number > g_nsf->total_songs) return 0;
    
    try {
        // 设置播放参数
        g_player->SetPlayFreq(sample_rate);
        g_player->SetChannels(channels);
        g_player->SetSong(track_number - 1);  // 内部使用0开始的索引
        g_player->Reset();
        
        // 确保播放器状态正确
        g_player->UpdateInfinite();
        
        // 计算总帧数
        uint64_t total_frames = (uint64_t)(length_ms + fade_ms) * sample_rate / 1000;
        
        // 计算WAV文件大小
        uint32_t data_size = total_frames * channels * sizeof(int16_t);
        uint32_t total_size = sizeof(WAVHeader) + data_size;
        
        if (total_size > buffer_size) {
            return 0;  // 缓冲区太小
        }
        
        // 创建WAV头
        WAVHeader* header = reinterpret_cast<WAVHeader*>(output_buffer);
        
        // 填充WAV头
        memcpy(header->riff, "RIFF", 4);
        header->file_size = total_size - 8;
        memcpy(header->wave, "WAVE", 4);
        memcpy(header->fmt, "fmt ", 4);
        header->fmt_size = 16;
        header->audio_format = 1;  // PCM
        header->num_channels = channels;
        header->sample_rate = sample_rate;
        header->byte_rate = sample_rate * channels * sizeof(int16_t);
        header->block_align = channels * sizeof(int16_t);
        header->bits_per_sample = 16;
        memcpy(header->data, "data", 4);
        header->data_size = data_size;
        
        // 音频数据缓冲区
        const int frames_per_buffer = 4096;
        std::unique_ptr<int16_t[]> audio_buffer(new int16_t[frames_per_buffer * channels]);
        
        // 渲染音频数据
        uint8_t* data_ptr = output_buffer + sizeof(WAVHeader);
        uint64_t remaining_frames = total_frames;
        
        while (remaining_frames > 0) {
            int frames_to_render = std::min(remaining_frames, (uint64_t)frames_per_buffer);
            
            // 渲染音频帧
            g_player->Render(audio_buffer.get(), frames_to_render);
            
            // 将音频数据复制到输出缓冲区（小端序）
            for (int i = 0; i < frames_to_render * channels; i++) {
                int16_t sample = audio_buffer[i];
                *data_ptr++ = sample & 0xFF;
                *data_ptr++ = (sample >> 8) & 0xFF;
            }
            
            remaining_frames -= frames_to_render;
        }
        
        return total_size;
        
    } catch (...) {
        return 0;
    }
}

/**
 * 设置当前播放轨道
 * @param track_number 轨道号（从0开始）
 */
EMSCRIPTEN_KEEPALIVE
void nsf_set_track(int track_number) {
    if (!g_player || !g_nsf) return;
    
    try {
        if (track_number >= 0 && track_number < g_nsf->total_songs) {
            g_player->SetSong(track_number);
            g_player->Reset();
        }
    } catch (...) {
        // 忽略错误
    }
}

/**
 * 为流式播放设置播放器参数
 * @param sample_rate 采样率
 * @param channels 声道数
 */
EMSCRIPTEN_KEEPALIVE
void nsf_player_set_options(int sample_rate, int channels) {
    if (!g_player) return;
    try {
        g_player->SetPlayFreq(sample_rate);
        g_player->SetChannels(channels);
    } catch (...) {
        // 忽略错误
    }
}

/**
 * 渲染音频数据用于流式播放
 * @param track_number 轨道号（从1开始，用于兼容）
 * @param frame_count 要渲染的帧数
 * @param output_buffer 输出缓冲区指针
 * @return 实际渲染的帧数，失败返回0
 */
EMSCRIPTEN_KEEPALIVE
int nsf_render_audio(int track_number, int frame_count, int16_t* output_buffer) {
    if (!g_player || !g_nsf || !output_buffer) return 0;
    if (frame_count <= 0) return 0;
    
    try {
        // 渲染音频帧到缓冲区
        uint32_t rendered = g_player->Render(output_buffer, frame_count);
        return rendered; // 返回实际渲染的帧数
        
    } catch (...) {
        return 0;
    }
}

/**
 * 重置播放器状态
 */
EMSCRIPTEN_KEEPALIVE
void nsf_reset() {
    if (g_player) {
        try {
            g_player->Reset();
        } catch (...) {
            // 忽略错误
        }
    }
}

/**
 * 清理资源
 */
EMSCRIPTEN_KEEPALIVE
void cleanup() {
    if (g_nsf) {
        delete g_nsf;
        g_nsf = nullptr;
    }
    if (g_player) {
        delete g_player;
        g_player = nullptr;
    }
    if (g_config) {
        delete g_config;
        g_config = nullptr;
    }
}

} // extern "C"