#include <stdint.h>
#include <psxgpu.h>

#define W 320
#define H 240
#define OT_LEN 16
#define PACKET_LEN 4096

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint8_t packet[PACKET_LEN];
} RenderBuffer;

typedef struct {
    RenderBuffer b[2];
    uint8_t *next;
    int active;
} RenderContext;

static RenderContext ctx;

static void init_video(void) {
    ResetGraph(0);

    SetDefDrawEnv(&ctx.b[0].draw, 0, 0, W, H);
    SetDefDispEnv(&ctx.b[0].disp, 0, 0, W, H);
    SetDefDrawEnv(&ctx.b[1].draw, 0, H, W, H);
    SetDefDispEnv(&ctx.b[1].disp, 0, H, W, H);

    setRGB0(&ctx.b[0].draw, 0, 0, 0);
    setRGB0(&ctx.b[1].draw, 0, 0, 0);
    ctx.b[0].draw.isbg = 1;
    ctx.b[1].draw.isbg = 1;

    ctx.active = 0;
    ctx.next = ctx.b[0].packet;
    ClearOTagR(ctx.b[0].ot, OT_LEN);

    SetDispMask(1);
}

static void *alloc_prim(int z, int size) {
    RenderBuffer *rb = &ctx.b[ctx.active];
    uint8_t *p = ctx.next;
    addPrim(&rb->ot[z], p);
    ctx.next += size;
    return p;
}

static void render_frame(void) {
    RenderBuffer *draw = &ctx.b[ctx.active];
    RenderBuffer *display = &ctx.b[ctx.active ^ 1];

    TILE *t = (TILE *)alloc_prim(1, sizeof(TILE));
    setTile(t);
    setXY0(t, 0, 0);
    setWH(t, W, H);
    setRGB0(t, 12, 12, 12);

    t = (TILE *)alloc_prim(2, sizeof(TILE));
    setTile(t);
    setXY0(t, 4, 4);
    setWH(t, 28, 28);
    setRGB0(t, 0, 0, 255);

    t = (TILE *)alloc_prim(2, sizeof(TILE));
    setTile(t);
    setXY0(t, W - 32, 4);
    setWH(t, 28, 28);
    setRGB0(t, 0, 255, 0);

    t = (TILE *)alloc_prim(2, sizeof(TILE));
    setTile(t);
    setXY0(t, 4, H - 32);
    setWH(t, 28, 28);
    setRGB0(t, 255, 0, 0);

    t = (TILE *)alloc_prim(2, sizeof(TILE));
    setTile(t);
    setXY0(t, W - 32, H - 32);
    setWH(t, 28, 28);
    setRGB0(t, 255, 255, 0);

    t = (TILE *)alloc_prim(2, sizeof(TILE));
    setTile(t);
    setXY0(t, 156, 116);
    setWH(t, 8, 8);
    setRGB0(t, 255, 255, 255);

    DrawSync(0);
    VSync(0);

    PutDispEnv(&display->disp);
    DrawOTagEnv(&draw->ot[OT_LEN - 1], &draw->draw);

    ctx.active ^= 1;
    ctx.next = display->packet;
    ClearOTagR(display->ot, OT_LEN);
}

int main(void) {
    init_video();

    for (;;) {
        render_frame();
    }

    return 0;
}
