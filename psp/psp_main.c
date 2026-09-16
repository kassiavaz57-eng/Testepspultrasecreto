#include <pspkernel.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <psprtc.h>
#include <psppower.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "data_win.h"
#include "file_system.h"
#include "gettime.h"
#include "log.h"
#include "noop_audio_system.h"
#include "psp_audio_system.h"
#include "psp_renderer.h"
#include "runner.h"
#include "vm.h"
#include "psp_file_system.h"
#include "psp_input.h"
PSP_MODULE_INFO("Butterscotch", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
static int exit_cb(int a,int b,void*c){(void)a;(void)b;(void)c;sceKernelExitGame();return 0;}
static int cb_thread(SceSize a,void*b){(void)a;(void)b;int cb=sceKernelCreateCallback("Exit Callback",exit_cb,NULL);if(cb>=0)sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
static void setup_callbacks(void){int t=sceKernelCreateThread("update_thread",cb_thread,0x11,0xFA0,0,NULL);if(t>=0)sceKernelStartThread(t,0,NULL);}
static unsigned long long pspPerfStepUs=0,pspPerfDrawUs=0,pspPerfSyncUs=0,pspPerfWaitUs=0;
static unsigned long long pspPerfFrames=0,pspPerfFrameMinUs=~0ULL,pspPerfFrameMaxUs=0;
void platformLog(const logType type,const char*fmt,va_list va){if(type==LOG_TYPE_WARNING)fputs("Warning: ",stdout);else if(type==LOG_TYPE_ERROR)fputs("Error: ",stdout);else if(type==LOG_TYPE_DEBUG)fputs("Debug: ",stdout);vprintf(fmt,va);}
static bool load_data_win(const char*path,DataWin**out){DataWinParserOptions o={0};o.parseGen8=true;o.parseOptn=true;o.parseLang=true;o.parseExtn=true;o.parseSond=true;o.parseAgrp=true;o.parseSprt=true;o.parseBgnd=true;o.parsePath=true;o.parseScpt=true;o.parseGlob=true;o.parseShdr=true;o.parseFont=true;o.parseTmln=true;o.parseObjt=true;o.parseRoom=true;o.parseTpag=true;o.parseCode=true;o.parseVari=true;o.parseFunc=true;o.parseStrg=true;o.parseTxtr=true;o.parseAudo=true;o.skipLoadingPreciseMasksForNonPreciseSprites=true;o.lazyLoadRooms=true;o.lazyLoadTextures=true;o.lazyLoadAudio=true;o.loadType=DATAWINLOADTYPE_LOAD_PER_CHUNK;*out=DataWin_parse(path,o);return *out!=NULL;}
int main(void){
    setup_callbacks();
    // Use the PSP's full supported 333 MHz CPU / 166 MHz bus clock. The current
    // renderer is CPU-bound (PNG decode + texture uploads), so leaving the PSP
    // at its lower default clock needlessly throttles the port.
    scePowerSetClockFrequency(333, 333, 166);
    Renderer *renderer = PSPRenderer_create();
    FileSystem *fs = PspFileSystem_create(".");
    AudioSystem *audio = (AudioSystem*)PspAudioSystem_create();
    DataWin *d=NULL;
    if(!load_data_win("data.win",&d)) sceKernelExitGame();
    VMContext *vm = VM_create(d);
    Runner *runner=Runner_create(d, vm, renderer, fs, audio, 0);
    if(!runner) sceKernelExitGame();
    Runner_initFirstRoom(runner);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    uint64_t lastFrameUs = sceKernelGetSystemTimeWide();
    uint64_t framePaceUs = lastFrameUs;
    uint64_t lastDiagUs = lastFrameUs;
    uint64_t framesSinceDiag = 0;
    for(;;){
    uint64_t frameStartUs = sceKernelGetSystemTimeWide();
    if(frameStartUs>lastFrameUs){ uint64_t rawFrameUs=frameStartUs-lastFrameUs; if(rawFrameUs<pspPerfFrameMinUs)pspPerfFrameMinUs=rawFrameUs; if(rawFrameUs>pspPerfFrameMaxUs)pspPerfFrameMaxUs=rawFrameUs; }
    uint64_t elapsedUs = frameStartUs - lastFrameUs;
    lastFrameUs = frameStartUs;
    // Runner deltaTime is expressed in microseconds. Cap large stalls so a slow PSP/PPSSPP frame
    // cannot inject an enormous simulation step.
    if (elapsedUs > 100000ULL) elapsedUs = 100000ULL;
    runner->deltaTime = (double)elapsedUs;
    RunnerKeyboard_beginFrame(runner->keyboard);
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    PspInput_poll(runner, &pad);
    uint64_t stepStartUs=sceKernelGetSystemTimeWide();
    Runner_step(runner);
    pspPerfStepUs += sceKernelGetSystemTimeWide()-stepStartUs;
    int32_t gameW=(int32_t)d->gen8.defaultWindowWidth, gameH=(int32_t)d->gen8.defaultWindowHeight;
    // The PSP renderer owns a live GU display list. Start the frame before any
    // draw call; unlike the PS2 queue renderer, drawPre cannot run before GU start.
    uint64_t drawStartUs=sceKernelGetSystemTimeWide();
    Runner_beginFrame(runner,gameW,gameH,480,272,480,272);
    Runner_drawPre(runner,480,272);
    Runner_drawViews(runner,gameW,gameH,false);
    runner->renderer->vtable->endFrameInit(runner->renderer);
    Runner_drawPost(runner,480,272);
    runner->renderer->vtable->endFrameEnd(runner->renderer);
    Runner_drawGUI(runner,480,272,gameW,gameH);
    pspPerfDrawUs += sceKernelGetSystemTimeWide()-drawStartUs;
    uint64_t syncStartUs=sceKernelGetSystemTimeWide();
    sceGuFinish(); sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    pspPerfSyncUs += sceKernelGetSystemTimeWide()-syncStartUs;
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    // Target 30 Hz without the old double-vblank stall: a slow frame is not
    // forced to wait for a second vblank and fall straight to ~15 FPS.
    uint64_t paceNow = sceKernelGetSystemTimeWide();
    uint64_t frameUs = paceNow - framePaceUs;
    if (frameUs < 33333ULL) sceKernelDelayThread((SceUInt)(33333ULL - frameUs));
    pspPerfWaitUs += sceKernelGetSystemTimeWide()-paceNow;
    pspPerfFrames++;
    framePaceUs = sceKernelGetSystemTimeWide();
    // Match the shared runner loop: consume a queued room change after the frame.
    Runner_handlePendingRoomChange(runner);
    framesSinceDiag++;
    uint64_t nowDiagUs = sceKernelGetSystemTimeWide();
    if (nowDiagUs - lastDiagUs >= 1000000ULL) {
        FILE *diag = fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_diag.txt", "a");
        if (diag) {
            GMLCamera *cam = Runner_getCameraForView(runner, 0);
            fprintf(diag, "PSP_FRAME frames=%llu room=%d views=%d roomSize=%dx%d camera=%dx%d fps=%0.2f cpu=%d\\n",
                    (unsigned long long)framesSinceDiag,
                    runner->currentRoomIndex,
                    runner->viewsEnabled ? 1 : 0,
                    runner->currentRoom ? runner->currentRoom->width : 0,
                    runner->currentRoom ? runner->currentRoom->height : 0,
                    cam ? cam->viewWidth : 0,
                    cam ? cam->viewHeight : 0,
                    (double)framesSinceDiag / ((double)(nowDiagUs - lastDiagUs) / 1000000.0),
                    scePowerGetCpuClockFrequency());
            fprintf(diag, "PSP_PERF frames=%llu stepAvgUs=%llu drawAvgUs=%llu syncAvgUs=%llu waitAvgUs=%llu minFrameUs=%llu maxFrameUs=%llu\\n",
                    (unsigned long long)pspPerfFrames,
                    pspPerfFrames ? pspPerfStepUs/pspPerfFrames : 0,
                    pspPerfFrames ? pspPerfDrawUs/pspPerfFrames : 0,
                    pspPerfFrames ? pspPerfSyncUs/pspPerfFrames : 0,
                    pspPerfFrames ? pspPerfWaitUs/pspPerfFrames : 0,
                    pspPerfFrameMinUs==~0ULL ? 0 : pspPerfFrameMinUs,
                    pspPerfFrameMaxUs);
            fclose(diag);
        }
        pspPerfStepUs=pspPerfDrawUs=pspPerfSyncUs=pspPerfWaitUs=0;
        pspPerfFrames=0; pspPerfFrameMinUs=~0ULL; pspPerfFrameMaxUs=0;
        framesSinceDiag = 0;
        lastDiagUs = nowDiagUs;
    }
}
sceKernelExitGame();
return 0;
}