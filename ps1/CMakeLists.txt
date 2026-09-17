cmake_minimum_required(VERSION 3.21)
set(CMAKE_TOOLCHAIN_FILE "$ENV{PSN00BSDK_LIBS}/cmake/sdk.cmake" CACHE FILEPATH "PSn00bSDK toolchain")
list(APPEND CMAKE_MODULE_PATH "$ENV{PSN00BSDK_LIBS}/cmake")
include(sdk)
project(butterscotch_ps1 LANGUAGES C ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(UPSTREAM "${CMAKE_CURRENT_LIST_DIR}/../upstream/src")

file(GLOB CORE_SOURCES
    "${UPSTREAM}/*.c"
)
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/data_win_print\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/gettime\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug_overlay\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/overlay_file_system\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/.*")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/cli/.*")

set(PS1_SOURCES
    main.c
    platform.c
    compat/stdio.c
    compat/math.c
    compat/libc.c
    gettime.c
    "${UPSTREAM}/src/debug_overlay.c"
    "${UPSTREAM}/../vendor/md5/md5.c"
    "${UPSTREAM}/../vendor/base64/base64.c"
    "${UPSTREAM}/../vendor/sha1/sha1.c"
)

psn00bsdk_add_executable(butterscotch_ps1 GPREL ${CORE_SOURCES} ${PS1_SOURCES})
target_include_directories(butterscotch_ps1 BEFORE PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/compat"
    "${UPSTREAM}"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/stb/ds"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/stb/vorbis"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/md5"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/sha1"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/base64"
    "${CMAKE_CURRENT_LIST_DIR}/../upstream/vendor/bzip2"
    "${UPSTREAM}/debug_font"
)
target_compile_definitions(butterscotch_ps1 PRIVATE
    ENABLE_NOOP_RENDERER
    USE_NOOP_FILE_SYSTEM
    ENABLE_WAD14
    ENABLE_WAD16
    ENABLE_WAD17
    PLATFORM_PS1
    __wasi__
    DISABLE_SCREENSHOTS
)
target_compile_options(butterscotch_ps1 PRIVATE -ffunction-sections -fdata-sections)
target_link_options(butterscotch_ps1 PRIVATE -Wl,--gc-sections)

