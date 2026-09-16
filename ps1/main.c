#include <stdint.h>
#include <psxetc.h>
#include <psxgpu.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define OT_LEN 16
#define PACKET_LEN 2048

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint8_t packet[PACKET_LEN];
} RenderBuffer;

static RenderBuffer buffers[2];
static uint8_t *next_packet;
static int db;

static void init_video(void) {
    ResetGraph(0);
    SetVideoMode(MODE_NTSC);

    SetDefDispEnv(&buffers[0].disp, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&buffers[0].draw, 0, SCREEN_H, SCREEN_W, SCREEN_H);

    SetDefDispEnv(&buffers[1].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&buffers[1].draw, 0, 0, SCREEN_W, SCREEN_H);

    setRGB0(&buffers[0].draw, 0, 0, 0);
    setRGB0(&buffers[1].draw, 0, 0, 0);
    buffers[0].draw.isbg = 1;
    buffers[1].draw.isbg = 1;
    buffers[0].draw.dtd = 1;
    buffers[1].draw.dtd = 1;

    db = 0;
    next_packet = buffers[0].packet;
    ClearOTagR(buffers[0].ot, OT_LEN);
    ClearOTagR(buffers[1].ot, OT_LEN);

    PutDispEnv(&buffers[0].disp);
    PutDrawEnv(&buffers[0].draw);
    SetDispMask(1);
}

static void add_tile(int x, int y, int w, int h, int r, int g, int b) {
    RenderBuffer *buf = &buffers[db];
    TILE *tile = (TILE *)next_packet;

    setTile(tile);
    setXY0(tile, x, y);
    setWH(tile, w, h);
    setRGB0(tile, r, g, b);

    addPrim(&buf->ot[1], tile);
    next_packet += sizeof(TILE);
}

static void frame(void) {
    RenderBuffer *drawbuf = &buffers[db];

    next_packet = drawbuf->packet;
    ClearOTagR(drawbuf->ot, OT_LEN);

    add_tile(0, 0, SCREEN_W, SCREEN_H, 8, 8, 8);
    add_tile(4, 4, 28, 28, 0, 0, 255);
    add_tile(SCREEN_W - 32, 4, 28, 28, 0, 255, 0);
    add_tile(4, SCREEN_H - 32, 28, 28, 255, 0, 0);
    add_tile(SCREEN_W - 32, SCREEN_H - 32, 28, 28, 255, 255, 0);
    add_tile(156, 116, 8, 8, 255, 255, 255);
    add_tile(0, 119, SCREEN_W, 2, 255, 255, 255);
    add_tile(159, 0, 2, SCREEN_H, 255, 255, 255);

    DrawSync(0);
    VSync(0);

    PutDispEnv(&drawbuf->disp);
    PutDrawEnv(&drawbuf->draw);
    SetDispMask(1);
    DrawOTag(&drawbuf->ot[OT_LEN - 1]);

    db ^= 1;
}

int main(void) {
    init_video();

    for (;;) {
        frame();
    }

    return 0;
}
