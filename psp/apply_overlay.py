if old not in s: raise SystemExit('ERROR: CMake loop.c block changed upstream')
s=s.replace(old,'if(PLATFORM STREQUAL "cli" OR PLATFORM STREQUAL "vita" OR PLATFORM STREQUAL "switch" OR PLATFORM STREQUAL "psp")',1)
old='if(ENABLE_NOOP_RENDERER AND NOT BACKEND STREQUAL "noop")'
if old not in s: raise SystemExit('ERROR: CMake noop-renderer guard changed upstream')
s=s.replace(old,'if(ENABLE_NOOP_RENDERER AND NOT BACKEND STREQUAL "noop" AND NOT PLATFORM STREQUAL "psp")',1)
# PSP has no host dynamic-loader/OpenGL GLAD dependency; the noop backend must not build glad.c.
old_glad='if(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch")\n        # GLAD'
if old_glad not in s: raise SystemExit('ERROR: GLAD CMake block changed upstream')
s=s.replace(old_glad,'if(NOT PLATFORM STREQUAL "vita" AND NOT PLATFORM STREQUAL "switch" AND NOT PLATFORM STREQUAL "psp")\n        # GLAD',1)
# PSP uses the native/no-op renderer path; host GL common sources require glad/OpenGL headers.
old_gl_sources='file(GLOB GL_SOURCES src/image/*.c src/gl_common/*.c)'
if old_gl_sources not in s: raise SystemExit('ERROR: GL source glob changed upstream')
s=s.replace(old_gl_sources,'if(PLATFORM STREQUAL "psp")\n        file(GLOB GL_SOURCES src/image/*.c)\n    else()\n        file(GLOB GL_SOURCES src/image/*.c src/gl_common/*.c)\n    endif()',1)
# Upstream declares BACKEND with an empty cache value after the platform preamble.
# Override that declaration for PSP after it occurs, so target_sources() sees "noop".
anchor='set(BACKEND "" CACHE STRING "Desktop platform backend")'
if anchor not in s: raise SystemExit('ERROR: upstream BACKEND declaration changed')
s=s.replace(anchor,anchor+'\nif(PLATFORM STREQUAL "psp")\n    set(BACKEND "noop")\nendif()',1)
block='''elseif(PLATFORM STREQUAL "psp")
    set(BACKEND "noop")\n    add_compile_definitions(PLATFORM_PSP USE_FLOAT_REALS NO_RVALUE_INT64)
    set(BACKEND "noop" CACHE STRING "Desktop platform backend" FORCE)
    set(BACKEND_LIBRARIES "")
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