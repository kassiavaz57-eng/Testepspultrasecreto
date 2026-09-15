#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "data_win.h"
#include "file_system.h"
#include "gettime.h"
#include "log.h"
#include "noop_audio_system.h"
#include "noop_renderer.h"
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
static bool load_data_win(const char*path,DataWin**out){DataWinParserOptions o={0};o.parseGen8=true;o.parseOptn=true;o.parseLang=true;o.parseExtn=true;o.parseSond=true;o.parseAgrp=true;o.parseSprt=true;o.parseBgnd=true;o.parsePath=true;o.parseScpt=true;o.parseGlob=true;o.parseShdr=true;o.parseFont=true;o.parseTmln=true;o.parseObjt=true;o.parseRoom=true;o.parseTpag=true;o.parseCode=true;o.parseVari=true;o.parseFunc=true;o.parseStrg=true;o.parseTxtr=false;o.parseAudo=false;o.skipLoadingPreciseMasksForNonPreciseSprites=true;o.lazyLoadRooms=true;o.lazyLoadTextures=true;o.lazyLoadAudio=true;o.loadType=DATAWINLOADTYPE_LOAD_PER_CHUNK;*out=DataWin_parse(path,o);return *out!=NULL;}
int main(void){pspDebugScreenInit();setup_callbacks();logInfo("Butterscotch PSP: boot\n");const char*root="ms0:/PSP/GAME/BUTTERSCOTCH";const char*path="ms0:/PSP/GAME/BUTTERSCOTCH/data.win";DataWin*d=NULL;if(!load_data_win(path,&d)){logError("Could not load %s\n",path);sceKernelSleepThread();return 1;}logInfo("WAD %u, %ux%u\n",d->gen8.wadVersion,d->gen8.defaultWindowWidth,d->gen8.defaultWindowHeight);VMContext*vm=VM_create(d);Renderer*r=NoopRenderer_create();AudioSystem*a=(AudioSystem*)NoopAudioSystem_create();FileSystem*fs=PspFileSystem_create(root);if(!vm||!r||!a||!fs){logError("Subsystem initialization failed\n");sceKernelSleepThread();return 1;}Runner*runner=Runner_create(d,vm,r,fs,a,0);if(!runner){logError("Runner_create failed\n");sceKernelSleepThread();return 1;}runner->osType=OS_PSP;Runner_initFirstRoom(runner);const uint64_t step=33333333ULL;uint64_t next=nowNanos();for(;;){SceCtrlData pad; sceCtrlReadBufferPositive(&pad,1);PspInput_poll(runner,&pad);if(pad.Buttons&PSP_CTRL_START)break;Runner_step(runner);next+=step;uint64_t now=nowNanos();if(next>now)sceKernelDelayThread((unsigned int)((next-now)/1000ULL));else next=now;}sceKernelExitGame();return 0;}