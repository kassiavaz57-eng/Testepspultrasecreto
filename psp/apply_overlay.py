from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]).resolve(); ov=Path(__file__).resolve().parent
cm=root/'CMakeLists.txt'
for p in (cm,root/'src/runner.h',root/'src/file_system.h',root/'src/data_win.h'):
    if not p.is_file(): raise SystemExit(f'ERROR: expected Butterscotch file missing: {p}')
s=cm.read_text()
if 'PLATFORM STREQUAL "psp"' in s: raise SystemExit('ERROR: PSP overlay already applied')
old='if(PLATFORM STREQUAL "ps2" OR PLATFORM STREQUAL "ps3" OR PLATFORM STREQUAL "web" OR PLATFORM STREQUAL "android")'
if old not in s: raise SystemExit('ERROR: CMake loop.c block changed upstream')
s=s.replace(old,'if(PLATFORM STREQUAL "ps2" OR PLATFORM STREQUAL "ps3" OR PLATFORM STREQUAL "psp" OR PLATFORM STREQUAL "web" OR PLATFORM STREQUAL "android")',1)
old='if(ENABLE_NOOP_RENDERER AND NOT BACKEND STREQUAL "noop")'
if old not in s: raise SystemExit('ERROR: CMake noop-renderer guard changed upstream')
s=s.replace(old,'if(ENABLE_NOOP_RENDERER AND NOT BACKEND STREQUAL "noop" AND NOT PLATFORM STREQUAL "psp")',1)
block='''elseif(PLATFORM STREQUAL "psp")
    add_compile_definitions(PLATFORM_PSP USE_FLOAT_REALS NO_RVALUE_INT64)
    set(BACKEND "noop" CACHE STRING "Desktop platform backend" FORCE)
    set(AUDIO_BACKEND "none" CACHE STRING "Audio backend" FORCE)
    set(ENABLE_NOOP_RENDERER ON CACHE BOOL "Enable the no-op renderer" FORCE)
    set(ENABLE_LEGACY_GL OFF CACHE BOOL "Enable the legacy OpenGL renderer" FORCE)
    set(ENABLE_MODERN_GL OFF CACHE BOOL "Enable the modern OpenGL renderer" FORCE)
    target_compile_options(butterscotch PRIVATE -O2 -G0 -fno-strict-aliasing)
    target_link_libraries(butterscotch PRIVATE bzip2 stb_ds sha1 stb_vorbis pspdebug pspkernel pspctrl pspdisplay pspgu pspgum pspaudio m)
'''
needle='elseif(PLATFORM STREQUAL "ps2")'
if needle not in s: raise SystemExit('ERROR: PS2 CMake block missing')
s=s.replace(needle,block+needle,1)
psp=root/'src/psp';psp.mkdir(exist_ok=True)
for n in ('psp_main.c','psp_file_system.c','psp_file_system.h','psp_input.c','psp_input.h'): shutil.copy2(ov/n,psp/n)
cm.write_text(s)
print('PSP overlay applied.')
