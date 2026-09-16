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
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/profiler\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/input_recording\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/debug_overlay\\.c$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/backends/.*")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/cli/.*")

set(PS1_SOURCES
    main.c
    platform.c
    compat/stdio.c
    compat/math.c
)

psn00bsdk_add_executable(butterscotch_ps1 GPREL ${CORE_SOURCES} ${PS1_SOURCES})
target_include_directories(butterscotch_ps1 BEFORE PRIVATE "${CMAKE_CURRENT_LIST_DIR}/compat" "${UPSTREAM}" "${UPSTREAM}/debug_font")
target_compile_definitions(butterscotch_ps1 PRIVATE
    ENABLE_NOOP_RENDERER
    USE_NOOP_FILE_SYSTEM
    NO_FMIN
    NO_FMAX
    NO_ROUND
    NO_LOG2
    NO_LROUND
    NO_SQRTF
    NO_FABSF
    NO_FMODF
    NO_SINF
    NO_COSF
    NO_FLOORF
    NO_ROUNDF
)
target_compile_options(butterscotch_ps1 PRIVATE -ffunction-sections -fdata-sections)
target_link_options(butterscotch_ps1 PRIVATE -Wl,--gc-sections)
