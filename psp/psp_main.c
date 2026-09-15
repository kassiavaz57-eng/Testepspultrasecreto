#include "psp_main.h"
#include "psp_input.h"
#include "psp_renderer.h"
#include "psp_file_system.h"
#include "data_win.h"
#include "vm.h"
#include "runner.h"
#include "runner_keyboard.h"
#include "noop_audio_system.h"
#include "log.h"
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <stdint.h>
#include <stdbool.h>
int main(void){psp_setup_callbacks();logInfo("Butterscotch PSP: boot\n");const char*root="ms0:/PSP/GAME/BUTTERSCOTCH";const char*path="ms0:/PSP/GAME/BUTTERSCOTCH/data.win";DataWin*d=NULL;if(!load_data_win(path,&d)){logError("Could not load %s\n",path);sceKernelSleepThread();return 1;}logInfo("WAD %u, %ux%u\n",d->gen8.wadVersion,d->gen8.defaultWindowWidth,d->gen8.defaultWindowHeight);VMContext*vm=VM_create(d);Renderer*r=PSPRenderer_create();AudioSystem*a=(AudioSystem*)NoopAudioSystem_create();FileSystem*fs=PspFileSystem_create(root);if(!vm||!r||!a||!fs){logError("Subsystem initialization failed\n");sceKernelSleepThread();return 1;}Runner*runner=Runner_create(d,vm,r,fs,a,0);if(!runner){logError("Runner_create failed\n");sceKernelSleepThread();return 1;}runner->osType=OS_PSP;Runner_initFirstRoom(runner);
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
    sceGuFinish(); sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}sceKernelExitGame();return 0;}