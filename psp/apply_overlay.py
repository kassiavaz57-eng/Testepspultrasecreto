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
        add_compile_definitions(PLATFORM_PSP USE_FLOAT_REALS NO_RVALUE_INT64)\n        set(PLATFORM_LIBRARIES pspuser pspctrl pspgu pspgum pspdisplay)

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
    target_link_libraries(butterscotch PRIVATE pspuser pspctrl pspgu pspgum pspdisplay)
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
            # Keep the PSP SDK libraries on the final link line explicitly.
            # The upstream platform variable can be overwritten later.
            target_link_libraries(butterscotch PRIVATE
                pspuser
                pspctrl
                pspgu
                pspge
                pspgum
                pspdisplay
            )
        endif()'''
if final_link not in s:
    raise SystemExit("ERROR: final target link line changed upstream")
s = s.replace(final_link, final_link_psp, 1)

CM.write_text(s)

# Temporary upstream draw-pipeline diagnostics for PSP. Instrumentation only.
runner = UPSTREAM / "src" / "runner.c"
runner_s = runner.read_text()

def runner_replace_once(old, new, label):
    global runner_s
    if old not in runner_s:
        raise SystemExit(f"ERROR: runner.c changed upstream; missing {label}")
    runner_s = runner_s.replace(old, new, 1)

runner_replace_once(
    '#include "stb_ds.h"',
    r'''#include "stb_ds.h"

#ifdef PLATFORM_PSP
static unsigned long pspDiagRunnerFrames = 0;
static unsigned long pspDiagViewsEnabled = 0;
static unsigned long pspDiagViewsSeen = 0;
static unsigned long pspDiagCameraNull = 0;
static unsigned long pspDiagBeginViews = 0;
static unsigned long pspDiagFallback = 0;
static unsigned long pspDiagRunnerDraw = 0;
static unsigned long pspDiagMaxDrawables = 0;
static unsigned long pspDiagInstancesVisited = 0;
static unsigned long pspDiagInstancesVisible = 0;
static unsigned long pspDiagDrawEvents = 0;
static unsigned long pspDiagDirectSelf = 0;

static void pspRunnerDiagReport(void) {
    if ((++pspDiagRunnerFrames % 60) != 0) return;
    FILE* f = fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_diag_runner.txt", "a");
    if (f == NULL) return;
    fprintf(f,
        "RUNNER_DIAG frames=%lu viewsEnabled=%lu viewsSeen=%lu cameraNull=%lu beginViews=%lu fallback=%lu runnerDraw=%lu maxDrawables=%lu instancesVisited=%lu instancesVisible=%lu drawEvents=%lu directSelf=%lu\\n",
        pspDiagRunnerFrames, pspDiagViewsEnabled, pspDiagViewsSeen, pspDiagCameraNull,
        pspDiagBeginViews, pspDiagFallback, pspDiagRunnerDraw, pspDiagMaxDrawables,
        pspDiagInstancesVisited, pspDiagInstancesVisible, pspDiagDrawEvents, pspDiagDirectSelf);
    fclose(f);
    pspDiagViewsEnabled = pspDiagViewsSeen = pspDiagCameraNull = 0;
    pspDiagBeginViews = pspDiagFallback = pspDiagRunnerDraw = pspDiagMaxDrawables = 0;
    pspDiagInstancesVisited = pspDiagInstancesVisible = pspDiagDrawEvents = pspDiagDirectSelf = 0;
}
#endif''',
    "runner diagnostic declarations"
)

runner_replace_once(
'''void Runner_draw(Runner* runner) {
    Room* room = runner->currentRoom;

    rebuildDrawableCacheIfDirty(runner);
    int32_t drawableCount = (int32_t) arrlen(runner->cachedDrawables);''',
'''void Runner_draw(Runner* runner) {
    Room* room = runner->currentRoom;
#ifdef PLATFORM_PSP
    pspDiagRunnerDraw++;
#endif

    rebuildDrawableCacheIfDirty(runner);
    int32_t drawableCount = (int32_t) arrlen(runner->cachedDrawables);
#ifdef PLATFORM_PSP
    if ((unsigned long) drawableCount > pspDiagMaxDrawables) pspDiagMaxDrawables = (unsigned long) drawableCount;
#endif''',
    "Runner_draw entry"
)

runner_replace_once(
'''        } else if (d->type == DRAWABLE_INSTANCE) {
            Instance* inst = d->instance;
            // Filter inactive/invisible instances at draw time so the cache doesn't need invalidation when those flags toggle.
            if (!inst->active || !inst->visible) continue;''',
'''        } else if (d->type == DRAWABLE_INSTANCE) {
            Instance* inst = d->instance;
#ifdef PLATFORM_PSP
            pspDiagInstancesVisited++;
#endif
            // Filter inactive/invisible instances at draw time so the cache doesn't need invalidation when those flags toggle.
            if (!inst->active || !inst->visible) continue;
#ifdef PLATFORM_PSP
            pspDiagInstancesVisible++;
#endif''',
    "instance counters"
)

runner_replace_once(
'''            if (codeId >= 0) {
                Runner_executeResolvedEvent(runner, inst, EVENT_DRAW, DRAW_NORMAL, codeId, ownerObjectIndex);
            } else if (runner->renderer != nullptr) {
                Renderer_drawSelf(runner->renderer, inst);''',
'''            if (codeId >= 0) {
#ifdef PLATFORM_PSP
                pspDiagDrawEvents++;
#endif
                Runner_executeResolvedEvent(runner, inst, EVENT_DRAW, DRAW_NORMAL, codeId, ownerObjectIndex);
            } else if (runner->renderer != nullptr) {
#ifdef PLATFORM_PSP
                pspDiagDirectSelf++;
#endif
                Renderer_drawSelf(runner->renderer, inst);''',
    "draw event counters"
)

runner_replace_once(
'''    bool viewsEnabled = runner->viewsEnabled;

    int32_t widescreenBaseW''',
'''    bool viewsEnabled = runner->viewsEnabled;
#ifdef PLATFORM_PSP
    if (viewsEnabled) pspDiagViewsEnabled++;
#endif

    int32_t widescreenBaseW''',
    "viewsEnabled counter"
)

runner_replace_once(
'''            RuntimeView* view = &runner->views[vi];
            if (!view->enabled) continue;
            // Geometry comes from the assigned camera (source of truth); the viewport (port) stays on the view.
            GMLCamera* camera = Runner_getCameraForView(runner, (int32_t) vi);
            if (camera == nullptr) continue;''',
'''            RuntimeView* view = &runner->views[vi];
            if (!view->enabled) continue;
#ifdef PLATFORM_PSP
            pspDiagViewsSeen++;
#endif
            // Geometry comes from the assigned camera (source of truth); the viewport (port) stays on the view.
            GMLCamera* camera = Runner_getCameraForView(runner, (int32_t) vi);
            if (camera == nullptr) {
#ifdef PLATFORM_PSP
                pspDiagCameraNull++;
#endif
                continue;
            }''',
    "camera-null counter"
)

runner_replace_once(
'''            renderer->vtable->beginView(renderer, viewX, viewY, viewW, viewH, portX, portY, portW, portH, viewAngle);

            Runner_draw(runner);''',
'''            renderer->vtable->beginView(renderer, viewX, viewY, viewW, viewH, portX, portY, portW, portH, viewAngle);
#ifdef PLATFORM_PSP
            pspDiagBeginViews++;
#endif

            Runner_draw(runner);''',
    "beginView counter"
)

runner_replace_once(
'''    if (!anyViewRendered) {
        runner->viewCurrent = 0;''',
'''    if (!anyViewRendered) {
#ifdef PLATFORM_PSP
        pspDiagFallback++;
#endif
        runner->viewCurrent = 0;''',
    "fallback counter"
)

runner_replace_once(
'''        renderer->vtable->endView(renderer);

    }

    // Reset view_current''',
'''        renderer->vtable->endView(renderer);

    }
#ifdef PLATFORM_PSP
    pspRunnerDiagReport();
#endif

    // Reset view_current''',
    "diagnostic report call"
)

runner.write_text(runner_s)

PSP_SRC.mkdir(parents=True, exist_ok=True)
required = ["psp_main.c", "psp_file_system.c", "psp_file_system.h", "psp_input.c", "psp_input.h", "psp_renderer.c", "psp_renderer.h", "stb_impl.c"]
for name in required:
    src = ROOT / name
    if not src.exists():
        raise SystemExit(f"Missing PSP source: {src}")
    shutil.copy2(src, PSP_SRC / name)

print("PSP overlay applied successfully.")