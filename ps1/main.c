#include <psxetc.h>
#include <psxcd.h>
#include <psxgpu.h>
#include <psxgte.h>
#include "loop.h"

#define DATA_WIN_PATH "cdrom:\\DATA.WIN;1"

static void diag_screen(const char *stage) {
    DISPENV disp;
    DRAWENV draw;
    ResetGraph(0);
    SetDefDispEnv(&disp, 0, 0, 320, 240);
    SetDefDrawEnv(&draw, 0, 240, 320, 240);
    draw.isbg = 1;
    draw.dfe = 0;
    SetDispMask(1);
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    FntLoad(960, 0);
    int id = FntOpen(16, 16, 288, 208, 0, 1024);
    FntPrint(id, "BUTTERSCOTCH PS1 DEBUG\n\n");
    FntPrint(id, "MAIN       OK\n");
    FntPrint(id, "GRAPH      OK\n");
    FntPrint(id, "CD         OK\n");
    FntPrint(id, "LOOP       %s\n", stage);
    FntFlush(-1);
}

int main(void) {
    diag_screen("START");
    CdInit();
    diag_screen("ENTERING");

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

    diag_screen("BUTTERSCOTCH LOOP");
    return loop(args, "BUTTER.EXE");
}
