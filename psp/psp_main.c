#include <pspkernel.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "data_win.h"
#include "file_system.h"
#include "gettime.h"
#include "log.h"
#include "noop_audio_system.h"
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
void platformLog(const logType type,const char*fmt,va_list va){if(type==LOG_TYPE_WARNING)fputs("Warning: ",stdout);else if(type==LOG_TYPE_ERROR)fputs("Error: ",stdout);else if(type==LOG_TYPE_DEBUG)fputs("Debug: ",stdout);vprintf(fmt,va);}
static bool load_data_win(const char*path,DataWin**out){DataWinParserOptions o={0};o.parseGen8=true;o.parseOptn=true;o.parseLang=true;o.parseExtn=true;o.parseSond=true;o.parseAgrp=true;o.parseSprt=true;o.parseBgnd=true;o.parsePath=true;o.parseScpt=true;o.parseGlob=true;o.parseShdr=true;o.parseFont=true;o.parseTmln=true;o.parseObjt=true;o.parseRoom=true;o.parseTpag=true;o.parseCode=true;o.parseVari=true;o.parseFunc=true;o.parseStrg=true;o.parseTxtr=true;o.parseAudo=false;o.skipLoadingPreciseMasksForNonPreciseSprites=true;o.lazyLoadRooms=true;o.lazyLoadTextures=true;o.lazyLoadAudio=true;o.loadType=DATAWINLOADTYPE_LOAD_PER_CHUNK;*out=DataWin_parse(path,o);return *out!=NULL;}
int main(void){
    setup_callbacks();
    Renderer *renderer = PSPRenderer_create();
    FileSystem *fs = PspFileSystem_create(".");
    AudioSystem *audio = (AudioSystem*)NoopAudioSystem_create();
    DataWin *d=NULL;
    if(!load_data_win("data.win",&d)) sceKernelExitGame();
    VMContext *vm = VM_create(d);
    Runner *runner=Runner_create(d, vm, renderer, fs, audio, 0);
    if(!runner) sceKernelExitGame();
    Runner_initFirstRoom(runner);
    for(;;){
    Runner_step(runner);
    int32_t gameW=(int32_t)d->gen8.defaultWindowWidth, gameH=(int32_t)d->gen8.defaultWindowHeight;
    // The PSP renderer owns a live GU display list. Start the frame before any
    // draw call; unlike the PS2 queue renderer, drawPre cannot run before GU start.
    Runner_beginFrame(runner,gameW,gameH,480,272,480,272);
    Runner_drawPre(runner,480,272);
    Runner_drawViews(runner,gameW,gameH,false);
    runner->renderer->vtable->endFrameInit(runner->renderer);
    Runner_drawPost(runner,480,272);
    runner->renderer->vtable->endFrameEnd(runner->renderer);
    Runner_drawGUI(runner,480,272,gameW,gameH);
    sceCtrlReadBufferPositive(NULL,0);
    RunnerKeyboard_beginFrame(runner->keyboard);
    sceGuFinish(); sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart(); sceGuSwapBuffers();
    // Match the shared runner loop: consume a queued room change after the frame.
    Runner_handlePendingRoomChange(runner);
}
sceKernelExitGame();
return 0;
}