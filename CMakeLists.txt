cmake_minimum_required(VERSION 3.10)
project(nsf2wav_wasm)

set(CMAKE_CXX_STANDARD 17)

# Emscripten specific settings
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    
    # Emscripten compiler flags - 简化配置
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s MODULARIZE=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORT_NAME=NSFPlayer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FILESYSTEM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
endif()

# Include directories
include_directories(.)
include_directories(./xgm)
include_directories(./xgm/devices)
include_directories(./xgm/player)
include_directories(./xgm/devices/CPU/km6502)

# 自动发现所有源文件
file(GLOB_RECURSE XGM_SOURCES "xgm/*.cpp" "xgm/*.c")

# Main WebAssembly interface
set(WASM_SOURCES
    nsf2wav_wasm.cpp
)

add_executable(nsf2wav_wasm
    ${WASM_SOURCES}
    ${XGM_SOURCES}
)

if(EMSCRIPTEN)
    set_target_properties(nsf2wav_wasm PROPERTIES
        LINK_FLAGS "-s WASM=1 -s EXPORTED_FUNCTIONS=['_malloc','_free','_init_nsf_player','_load_nsf_data','_get_nsf_info','_nsf_to_wav','_nsf_set_track','_nsf_player_set_options','_nsf_render_audio','_nsf_reset','_cleanup'] -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','writeArrayToMemory','UTF8ToString','HEAPU8','HEAP8','HEAP16','HEAP32'] -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME=NSFPlayer -s FILESYSTEM=1 -O2"
    )
endif()