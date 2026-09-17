#include "loop.h"
#include "platformdefs.h"

#define DATA_WIN_PATH "\\DATA.WIN;1"

int main(void) {
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
    args.renderer = PS1;
    args.dataWinPath = DATA_WIN_PATH;

    return loop(args, "BUTTER.EXE");
}
