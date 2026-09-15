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
int main(void){setup_callbacks();logInfo("Butterscotch PSP: boot\n");const char*root="ms0:/PSP/GAME/BUTTERSCOTCH";const char*path="ms0:/PSP/GAME/BUTTERSCOTCH/data.win";DataWin*d=NULL;if(!load_data_win(path,&d)){logError("Could not load %s\n",path);sceKernelSleepThread();return 1;}logInfo("WAD %u, %ux%u\n",d->gen8.wadVersion,d->gen8.defaultWindowWidth,d->gen8.defaultWindowHeight);VMContext*vm=VM_create(d);Renderer*r=PSPRenderer_create();AudioSystem*a=(AudioSystem*)NoopAudioSystem_create();FileSystem*fs=PspFileSystem_create(root);if(!vm||!r||!a||!fs){logError("Subsystem initialization failed\n");sceKernelSleepThread();return 1;}Runner*runner=Runner_create(d,vm,r,fs,a,0);if(!runner){logError("Runner_create failed\n");sceKernelSleepThread();return 1;}runner->osType=OS_PSP;Runner_initFirstRoom(runner);
sceDisplayWaitVblankStart();
for(;;){
    SceCtrlData pad; sceCtrlReadBufferPositive(&pad,1); PspInput_poll(runner,&pad);
    if(pad.Buttons&PSP_CTRL_START)break;
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
    RunnerKeyboard_beginFrame(runner->keyboard);

    /* Isolated framebuffer/GU diagnostic: draw AFTER the complete Runner frame.
       If this is visible, the PSP display/swap path is healthy and the remaining
       problem is strictly in the Butterscotch rendering path. */
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuViewport(2048,2048,480,272);
    sceGuScissor(0,0,480,272);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumOrtho(0,480,272,0,-1,1);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
    PSPVertex *dv=(PSPVertex*)sceGuGetMemory(4*sizeof(PSPVertex));
    unsigned int dc=GU_RGBA(255,0,255,255);
    dv[0]=(PSPVertex){0,0,dc,8,8,0}; dv[1]=(PSPVertex){0,0,dc,48,8,0};
    dv[2]=(PSPVertex){0,0,dc,48,48,0}; dv[3]=(PSPVertex){0,0,dc,8,48,0};
    sceGuDrawArray(GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,NULL,dv);
    sceGuEnable(GU_BLEND);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuFinish(); sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
sceKernelExitGame();
return 0;
}