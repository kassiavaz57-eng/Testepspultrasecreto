#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <psxetc.h>
#include "platformdefs.h"
#include "log.h"
#include "ps1_renderer.h"
#include <stdarg.h>
#include <stdio.h>

bool platformInit(int32_t reqW, int32_t reqH, const char *title, bool headless) {
    (void)reqW; (void)reqH; (void)title; (void)headless;
    return true;
}

void platformInitFunctions(Runner *runner) { (void)runner; }
void platformExit(void) {}
void platformSwapBuffers(void) { /* Main owns the PS1 frame presentation. */ }
void *platformGetProcAddress(const char *name) { (void)name; return NULL; }
bool platformHandleEvents(void) { return false; }
void platformGetMousePos(double *xPos, double *yPos) {
    if (xPos) *xPos = 0.0;
    if (yPos) *yPos = 0.0;
}
bool platformGetWindowSize(int32_t *outW, int32_t *outH) {
    if (outW) *outW = 320;
    if (outH) *outH = 240;
    return true;
}
bool platformGetScaledWindowSize(int32_t *outW, int32_t *outH) {
    return platformGetWindowSize(outW, outH);
}
void platformSetWindowSize(int32_t width, int32_t height) { (void)width; (void)height; }
void platformSetWindowTitle(const char *title) { (void)title; }
void platformSleepUntil(uint64_t time) { (void)time; VSync(0); }

void platformLog(const logType type, const char *format, va_list va) {
    (void)type;
    vfprintf(stderr, format, va);
}
