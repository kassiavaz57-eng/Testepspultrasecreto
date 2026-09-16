#include <stdint.h>
#include <string.h>
#include <psxetc.h>
#include <psxgpu.h>
#include <psxcd.h>

#define W 320
#define H 240
#define OVL 256

static DISPENV disp;
static DRAWENV draw;

static void video_init(void) {
    ResetGraph(0);
    SetDefDispEnv(&disp, 0, 0, W, H);
    SetDefDrawEnv(&draw, 0, H, W, H);
    draw.isbg = 1;
    setRGB0(&draw, 0, 0, 0);
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    SetDispMask(1);
    FntLoad(960, 0);
    FntOpen(8, 8, 304, 224, 0, 512);
}

static void show_status(const char *a, const char *b, const char *c) {
    FntPrint(-1, "BUTTERSCOTCH PS1\n----------------\n%s\n%s\n%s\n", a, b, c);
    DrawSync(0);
    FntFlush(-1);
    DrawSync(0);
    VSync(0);
}

static int probe_data(void) {
    CdlFILE file;
    uint32_t sector[512];

    if (!CdSearchFile(&file, "\\DATA.WIN;1"))
        return -1;
    if (CdRead(1, sector, CdlModeSize) < 0)
        return -2;
    if (CdReadSync(0, 0) < 0)
        return -3;
    if (memcmp(sector, "GEN8", 4) != 0)
        return -4;
    return 0;
}

int main(void) {
    video_init();
    CdInit();

    int r = probe_data();
    if (r == 0)
        show_status("DATA.WIN found.", "GEN8 header OK.", "CD streaming ready.");
    else if (r == -1)
        show_status("DATA.WIN not found.", "CD directory lookup failed.", "Check the disc image.");
    else if (r == -4)
        show_status("DATA.WIN found.", "Header is not GEN8.", "Not a normal GameMaker WAD.");
    else
        show_status("DATA.WIN found.", "CD read failed.", "Stopped safely.");

    for (;;) VSync(0);
    return 0;
}
