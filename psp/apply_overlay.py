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
        add_compile_definitions(PLATFORM_PSP USE_FLOAT_REALS NO_RVALUE_INT64)\n        set(PLATFORM_LIBRARIES pspuser pspdebug pspctrl)

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
    target_link_libraries(butterscotch PRIVATE pspuser pspdebug pspctrl)
elseif(PLATFORM STREQUAL "ps2")''',
'PSP system libraries'
)

CM.write_text(s)

PSP_SRC.mkdir(parents=True, exist_ok=True)
required = ["psp_main.c", "psp_file_system.c", "psp_file_system.h", "psp_input.c", "psp_input.h", "stb_impl.c"]
for name in required:
    src = ROOT / name
    if not src.exists():
        raise SystemExit(f"Missing PSP source: {src}")
    shutil.copy2(src, PSP_SRC / name)

print("PSP overlay applied successfully.")