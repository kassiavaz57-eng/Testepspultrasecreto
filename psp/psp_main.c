#include <pspkernel.h>
#include <stdio.h>
#include "loop.h"
#include "platformdefs.h"

PSP_MODULE_INFO("Butterscotch", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

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
