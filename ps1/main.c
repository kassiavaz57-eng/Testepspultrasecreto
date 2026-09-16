#include <stdint.h>
#include <psxetc.h>
#include <psxgpu.h>

#define W 320
#define H 240
#define OT_LEN 8
#define PACKET_LEN 1024

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint8_t packet[PACKET_LEN];
} Buffer;

static Buffer buf;

static void init_video(void) {
    ResetGraph(0);

    SetDefDispEnv(&buf.disp, 0, 0, W, H);
    SetDefDrawEnv(&buf.draw, 0, H, W, H);

    buf.draw.isbg = 1;
    buf.draw.dtd = 1;
    setRGB0(&buf.draw, 0, 0, 0);

    ClearOTagR(buf.ot, OT_LEN);
    PutDispEnv(&buf.disp);
    PutDrawEnv(&buf.draw);
    SetDispMask(1);
}

static void draw_calibration(void) {
    TILE *p;
    uint8_t *next = buf.packet;

    ClearOTagR(buf.ot, OT_LEN);

    p = (TILE *)next;
    next += sizeof(TILE);
    setTile(p);
    setXY0(p, 4, 4);
    setWH(p, 28, 28);
    setRGB0(p, 0, 0, 255);
    addPrim(&buf.ot[OT_LEN - 1], p);

    p = (TILE *)next;
    next += sizeof(TILE);
    setTile(p);
    setXY0(p, W - 32, 4);
    setWH(p, 28, 28);
    setRGB0(p, 0, 255, 0);
    addPrim(&buf.ot[OT_LEN - 1], p);

    p = (TILE *)next;
    next += sizeof(TILE);
    setTile(p);
    setXY0(p, 4, H - 32);
    setWH(p, 28, 28);
    setRGB0(p, 255, 0, 0);
    addPrim(&buf.ot[OT_LEN - 1], p);

    p = (TILE *)next;
    next += sizeof(TILE);
    setTile(p);
    setXY0(p, W - 32, H - 32);
    setWH(p, 28, 28);
    setRGB0(p, 255, 255, 0);
    addPrim(&buf.ot[OT_LEN - 1], p);

    p = (TILE *)next;
    next += sizeof(TILE);
    setTile(p);
    setXY0(p, 156, 116);
    setWH(p, 8, 8);
    setRGB0(p, 255, 255, 255);
    addPrim(&buf.ot[OT_LEN - 1], p);

    DrawSync(0);
    VSync(0);
    PutDispEnv(&buf.disp);
    PutDrawEnv(&buf.draw);
    DrawOTag(&buf.ot[OT_LEN - 1]);
}

int main(void) {
    init_video();

    for (;;) {
        draw_calibration();
    }

    return 0;
}
