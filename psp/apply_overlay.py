#!/usr/bin/env python3
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
UPSTREAM = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "upstream"
CM = UPSTREAM / "CMakeLists.txt"
PSP_SRC = UPSTREAM / "src" / "psp"

if not CM.exists():
    raise SystemExit(f"Missing upstream CMakeLists.txt: {CM}")

s = CM.read_text()

def replace_once(old, new, label):
    global s
    if old not in s:
        raise SystemExit(f"ERROR: upstream CMakeLists.txt changed; missing {label}")
    s = s.replace(old, new, 1)

# Make PSP enter the same refactored platform build path as CLI/Vita/Switch.
replace_once(
    'if(PLATFORM STREQUAL "cli" OR PLATFORM STREQUAL "vita" OR PLATFORM STREQUAL "switch")',
    'if(PLATFORM STREQUAL "cli" OR PLATFORM STREQUAL "vita" OR PLATFORM STREQUAL "switch" OR PLATFORM STREQUAL "psp")',
    'platform source block'
)

# Add a PSP-specific branch without changing the existing desktop/Vita/Switch behavior.
replace_once(
'''    elseif(PLATFORM STREQUAL "switch")
        add_compile_definitions(PLATFORM_SWITCH)

        set(VM_GML_PROFILER_DEFAULT OFF)
        set(VM_TRACING_DEFAULT OFF)
        set(VM_OPCODE_PROFILER_DEFAULT OFF)
        set(VM_STUB_LOGS_DEFAULT OFF)
    else()
''',
'''    elseif(PLATFORM STREQUAL "switch")
        add_compile_definitions(PLATFORM_SWITCH)

        set(VM_GML_PROFILER_DEFAULT OFF)
        set(VM_TRACING_DEFAULT OFF)
        set(VM_OPCODE_PROFILER_DEFAULT OFF)
        set(VM_STUB_LOGS_DEFAULT OFF)
    elseif(PLATFORM STREQUAL "psp")
        add_compile_definitions(PLATFORM_PSP USE_FLOAT_REALS NO_RVALUE_INT64)\n        set(PLATFORM_LIBRARIES pspuser pspctrl)

        set(VM_GML_PROFILER_DEFAULT OFF)
        set(VM_TRACING_DEFAULT OFF)
        set(VM_OPCODE_PROFILER_DEFAULT OFF)
        set(VM_STUB_LOGS_DEFAULT OFF)
    else()
''',
    'PSP platform branch'
)

# The PSP port uses its own native/no-op platform backend.
replace_once(
'''set(BACKEND "" CACHE STRING "Desktop platform backend")
set(AUDIO_BACKEND "" CACHE STRING "Audio backend")
''',
'''set(BACKEND "" CACHE STRING "Desktop platform backend")
if(PLATFORM STREQUAL "psp")
    set(BACKEND "noop")
endif()
set(AUDIO_BACKEND "" CACHE STRING "Audio backend")
''',
'PSP backend default'
)

# The no-op PSP renderer must not compile host OpenGL/GLAD code.
replace_once(
'file(GLOB GL_SOURCES src/image/*.c src/gl_common/*.c)',
'''if(PLATFORM STREQUAL "psp")
        file(GLOB GL_SOURCES src/image/*.c)
    else()
        file(GLOB GL_SOURCES src/image/*.c src/gl_common/*.c)
    endif()''',
'GL source selection'
)

replace_once(
'if(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch")\n        # GLAD',
'if(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch" AND NOT PLATFORM STREQUAL "psp")\n        # GLAD',
'GLAD guard'
)

# Avoid the host dynamic-loader dependency on PSP.
replace_once(
'elseif(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch")',
'elseif(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch" AND NOT PLATFORM STREQUAL "psp")',
'DL library guard'
)

# Link the actual PSP system libraries used by psp_main.c.
replace_once(
'elseif(PLATFORM STREQUAL "ps2")',
'''elseif(PLATFORM STREQUAL "psp")
    target_link_libraries(butterscotch PRIVATE pspuser pspctrl)
elseif(PLATFORM STREQUAL "ps2")''',
'PSP system libraries'
)

# PSP libc does not provide POSIX sigaction. Disable only the optional
# crash-signal handler in the shared loop; the VM loop itself remains intact.
loop = UPSTREAM / "src" / "loop.c"
loop_s = loop.read_text()
replace_loop = "#if !defined(_WIN32) && !defined(PLATFORM_VITA) && !defined(__SWITCH__) && !defined(__wasi__)"
if replace_loop not in loop_s:
    raise SystemExit("ERROR: loop.c crash-handler guard changed upstream")
loop_s = loop_s.replace(
    replace_loop,
    "#if !defined(_WIN32) && !defined(PLATFORM_VITA) && !defined(PLATFORM_PSP) && !defined(__SWITCH__) && !defined(__wasi__)",
    1
)
loop.write_text(loop_s)

# Ensure PSP libraries are attached to the final executable target.
final_link = '        target_link_libraries(butterscotch PRIVATE ${BACKEND_LIBRARIES} ${AUDIO_LIBRARIES} ${PLATFORM_LIBRARIES})'
final_link_psp = final_link + '''
        if(PLATFORM STREQUAL "psp")
            target_link_libraries(butterscotch PRIVATE pspuser pspctrl)
        endif()'''
if final_link not in s:
    raise SystemExit("ERROR: final target link line changed upstream")
s = s.replace(final_link, final_link_psp, 1)

CM.write_text(s)

PSP_SRC.mkdir(parents=True, exist_ok=True)
required = ["psp_main.c", "psp_file_system.c", "psp_file_system.h", "psp_input.c", "psp_input.h", "stb_impl.c"]
for name in required:
    src = ROOT / name
    if not src.exists():
        raise SystemExit(f"Missing PSP source: {src}")
    shutil.copy2(src, PSP_SRC / name)

print("PSP overlay applied successfully.")