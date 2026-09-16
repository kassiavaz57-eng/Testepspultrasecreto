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
static unsigned long long pspBootLogFrames=0;
void platformLog(const logType type,const char*fmt,va_list va){if(type==LOG_TYPE_WARNING)fputs("Warning: ",stdout);else if(type==LOG_TYPE_ERROR)fputs("Error: ",stdout);else if(type==LOG_TYPE_DEBUG)fputs("Debug: ",stdout);vprintf(fmt,va);}
static bool load_data_win(const char*path,DataWin**out){DataWinParserOptions o={0};o.parseGen8=true;o.parseOptn=true;o.parseLang=true;o.parseExtn=true;o.parseSond=true;o.parseAgrp=true;o.parseSprt=true;o.parseBgnd=true;o.parsePath=true;o.parseScpt=true;o.parseGlob=true;o.parseShdr=true;o.parseFont=true;o.parseTmln=true;o.parseObjt=true;o.parseRoom=true;o.parseTpag=true;o.parseCode=true;o.parseVari=true;o.parseFunc=true;o.parseStrg=true;o.parseTxtr=true;o.parseAudo=true;o.skipLoadingPreciseMasksForNonPreciseSprites=true;o.lazyLoadRooms=true;o.lazyLoadTextures=true;o.lazyLoadAudio=true;o.loadType=DATAWINLOADTYPE_LOAD_PER_CHUNK;*out=DataWin_parse(path,o);return *out!=NULL;}
int main(void){
    setup_callbacks();

    /* Minimal PSP video probe: no Butterscotch, no data.win, no renderer.
       This deliberately owns the framebuffer/GU setup so we can isolate the
       PSP display path from every game/runtime subsystem. */
    sceGuInit();

    static unsigned int __attribute__((aligned(16))) list[262144];

    void *fb0 = (void*)0x00000000;
    void *fb1 = (void*)0x00110000;
    void *zbuf = (void*)0x00198000;

    sceDisplaySetMode(0, 480, 272);
    sceDisplaySetFrameBuf(fb0, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, 512);
    sceGuDispBuffer(480, 272, fb1, 512);
    sceGuDepthBuffer(zbuf, 512);
    sceGuOffset(2048 - (480/2), 2048 - (272/2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuScissor(0, 0, 480, 272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuClearColor(GU_RGBA(0, 180, 255, 255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();

    FILE *bootlog=fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_boot.txt","a");
    if(bootlog){
        fprintf(bootlog,"VIDEO_PROBE: GU initialized\\n");
        fprintf(bootlog,"VIDEO_PROBE: framebuffer=0x00000000 display=480x272\\n");
        fprintf(bootlog,"VIDEO_PROBE: clear+swap completed\\n");
        fflush(bootlog);
    }

    for(;;){
        sceDisplayWaitVblankStart();
    }

    return 0;
}
