#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "platformdefs.h"
#include "runner.h"
#include "psp_input.h"
#include "gettime.h"

extern Runner *g_runner;

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless) {
    (void)reqW; (void)reqH; (void)title; (void)headless;
    sceDisplaySetMode(0, 480, 272);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    return true;
}

void platformExit(void) {
}

void platformInitFunctions(Runner *runner) {
    (void)runner;
}

bool platformGetWindowSize(int32_t *outW, int32_t *outH) {
    if (!outW || !outH) return false;
    *outW = 480;
    *outH = 272;
    return true;
}

bool platformGetScaledWindowSize(int32_t *outW, int32_t *outH) {
    return platformGetWindowSize(outW, outH);
}

void platformSetWindowSize(int32_t width, int32_t height) {
    (void)width; (void)height;
}

void platformSetWindowTitle(const char *title) {
    (void)title;
}

void platformGetMousePos(double *xPos, double *yPos) {
    if (xPos) *xPos = 0.0;
    if (yPos) *yPos = 0.0;
}

bool platformHandleEvents(void) {
    if (!g_runner) return false;
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
    if (targetTime <= now) return;
    uint64_t remaining = targetTime - now;
    if (remaining > 2000000ULL) {
        uint32_t us = (uint32_t)((remaining - 1000000ULL) / 1000ULL);
        if (us) sceKernelDelayThread(us);
    }
    while (nowNanos() < targetTime)
        sceKernelDelayThread(100);
}
