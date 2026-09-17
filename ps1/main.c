#include <psxetc.h>
#include <psxcd.h>
#include "loop.h"

#define DATA_WIN_PATH "\\DATA.WIN;1"

int main(void) {
    ResetGraph(0);
    CdInit();

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

    return loop(args, "BUTTER.EXE");
}
