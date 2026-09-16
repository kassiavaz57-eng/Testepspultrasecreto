#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "loop.h"
#include "platformdefs.h"
#include "runner.h"
#include "psp_input.h"
#include "gettime.h"

PSP_MODULE_INFO("Butterscotch", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

static Runner *g_runner = NULL;
static int g_width = 480;
static int g_height = 272;
static bool g_initialized = false;

static int exit_cb(int a, int b, void *c) {
    (void)a; (void)b; (void)c;
    sceKernelExitGame();
    return 0;
}

static int cb_thread(SceSize a, void *b) {
    (void)a; (void)b;
    int cb = sceKernelCreateCallback("Exit Callback", exit_cb, NULL);
    if (cb >= 0)
        sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int t = sceKernelCreateThread("update_thread", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0)
        sceKernelStartThread(t, 0, NULL);
}

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless) {
    (void)reqW;
    (void)reqH;
    (void)title;
    (void)headless;

    if (g_initialized)
        return true;

    sceDisplaySetMode(0, 480, 272);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    g_initialized = true;

    FILE *f = fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_boot.txt", "a");
    if (f) {
        fprintf(f, "PLATFORM: init %dx%d\\n", g_width, g_height);
        fflush(f);
        fclose(f);
    }

    return true;
}

void platformExit(void) {
    g_initialized = false;
    g_runner = NULL;
}

void platformInitFunctions(Runner *runner) {
    g_runner = runner;
    runner->setCursor = NULL;
    runner->currentCursor = GML_CR_DEFAULT;
}

bool platformGetWindowSize(int32_t *outW, int32_t *outH) {
    if (!outW || !outH || !g_initialized)
        return false;
    *outW = g_width;
    *outH = g_height;
    return true;
}

bool platformGetScaledWindowSize(int32_t *outW, int32_t *outH) {
    return platformGetWindowSize(outW, outH);
}

void platformSetWindowSize(int32_t width, int32_t height) {
    (void)width;
    (void)height;
}

void platformSetWindowTitle(const char *title) {
    (void)title;
}

void platformGetMousePos(double *xPos, double *yPos) {
    if (xPos) *xPos = 0.0;
    if (yPos) *yPos = 0.0;
}

bool platformHandleEvents(void) {
    if (!g_runner)
        return false;

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(&pad, 1);
    PspInput_poll(g_runner, &pad);
    return false;
}

void platformSwapBuffers(void) {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void *platformGetProcAddress(const char *name) {
    (void)name;
    return NULL;
}

void platformSleepUntil(uint64_t targetTime) {
    uint64_t now = nowNanos();
    if (targetTime <= now)
        return;

    uint64_t remaining = targetTime - now;
    if (remaining > 2000000ULL) {
        uint32_t us = (uint32_t)((remaining - 1000000ULL) / 1000ULL);
        if (us > 0)
            sceKernelDelayThread(us);
    }

    while (nowNanos() < targetTime)
        sceKernelDelayThread(100);
}

int main(int argc, char **argv) {
    setup_callbacks();
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    CommandLineArgs args = {0};
    args.exitAtFrame = -1;
    args.speedMultiplier = 1.0;
    args.fastForwardSpeed = 0.0;
    args.osType = OS_WINDOWS;
    args.profilerFramesBetween = 0;
    args.loadType = DATAWINLOADTYPE_LOAD_PER_CHUNK;
    args.lazyRooms = true;
    args.lazyTextures = true;
    args.lazyAudio = true;
    args.renderer = NOOP;
    args.dataWinPath = "ms0:/PSP/GAME/BUTTERSCOTCH/data.win";
    args.saveFolder = "ms0:/PSP/GAME/BUTTERSCOTCH";

    FILE *f = fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_boot.txt", "a");
    if (f) {
        fprintf(f, "BOOT: main\\n");
        fprintf(f, "BOOT: entering Butterscotch loop\\n");
        fflush(f);
        fclose(f);
    }

    int ret = loop(args, (argc > 0 && argv != NULL) ? argv[0] : "EBOOT.PBP");
    freeCommandLineArgs(&args);
    return ret;
}
