#include <stdint.h>
#include <psxetc.h>
#include <psxgpu.h>

#define W 320
#define H 240

static DISPENV disp;
static DRAWENV draw;
static RECT box;

static void video_init(void) {
    ResetGraph(0);
    SetDefDispEnv(&disp, 0, 0, W, H);
    SetDefDrawEnv(&draw, 0, H, W, H);
    draw.isbg = 1;
    setRGB0(&draw, 0, 0, 0);
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    SetDispMask(1);
}

static void draw_test(void) {
    ClearImage(&box, 0, 0, 0);
    box.x = 0;
    box.y = 0;
    box.w = W;
    box.h = H;
    ClearImage(&box, 96, 0, 0);
    DrawSync(0);
    VSync(0);
}

int main(void) {
    video_init();

    box.x = 0;
    box.y = 0;
    box.w = W;
    box.h = H;
    ClearImage(&box, 96, 0, 0);

    for (;;) {
        draw_test();
    }

    return 0;
}
