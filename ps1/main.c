#include <psxetc.h>
#include <psxcd.h>
#include "loop.h"
#include <stdio.h>

#define DATA_WIN_PATH "cdrom:\\DATA.WIN;1"

int main(void) {
    printf("PS1: main start\\n");
    ResetGraph(0);
    printf("PS1: graph ok\\n");
    CdInit();
    printf("PS1: cd ok\\n");

    CommandLineArgs args = {0};
    args.exitAtFrame = -1;
    args.speedMultiplier = 1.0;
    args.fastForwardSpeed = 0.0;
    args.osType = OS_UNKNOWN;
    args.profilerFramesBetween = 0;
    args.loadType = DATAWINLOADTYPE_LOAD_PER_CHUNK;
    args.lazyRooms = true;
    args.lazyTextures = true;
    args.lazyAudio = true;
    args.renderer = NOOP;
    args.dataWinPath = DATA_WIN_PATH;

    printf("PS1: entering Butterscotch loop\\n");
    return loop(args, "BUTTER.EXE");
}
