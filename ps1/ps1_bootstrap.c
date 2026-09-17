#include <psxgpu.h>
#include <psxetc.h>
#include "ps1_bootstrap.h"

static DISPENV gDisp[2];
static DRAWENV gDraw[2];

void Ps1Bootstrap_init(void) {
    ResetGraph(0);
    SetDefDispEnv(&gDisp[0], 0, 0, 320, 240);
    SetDefDispEnv(&gDisp[1], 0, 240, 320, 240);
    SetDefDrawEnv(&gDraw[0], 0, 240, 320, 240);
    SetDefDrawEnv(&gDraw[1], 0, 0, 320, 240);
    gDraw[0].isbg = 1;
    gDraw[1].isbg = 1;
    setRGB0(&gDraw[0], 0, 0, 0);
    setRGB0(&gDraw[1], 0, 0, 0);
    PutDispEnv(&gDisp[0]);
    PutDrawEnv(&gDraw[0]);
    SetDispMask(1);
}
