#include <stdint.h>
#include <psxetc.h>
#include <psxgpu.h>

#define W 320
#define H 240

static DISPENV disp;
static DRAWENV draw;
static TILE tile;

static void rect(int x,int y,int w,int h,int r,int g,int b){
    setTile(&tile);
    setXY0(&tile,x,y);
    setWH(&tile,w,h);
    setRGB0(&tile,r,g,b);
    DrawPrim(&tile);
}

static void video_init(void){
    ResetGraph(0);
    SetDefDispEnv(&disp,0,0,W,H);
    SetDefDrawEnv(&draw,0,H,W,H);
    draw.isbg=1;
    setRGB0(&draw,0,0,0);
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    SetDispMask(1);
}

static void pattern(void){
    rect(0,0,W,H,0,0,0);
    rect(4,4,28,28,0,0,255);
    rect(W-32,4,28,28,0,255,0);
    rect(4,H-32,28,28,255,0,0);
    rect(W-32,H-32,28,28,255,255,0);
    rect(156,116,8,8,255,255,255);
    rect(0,119,W,2,255,255,255);
    rect(159,0,2,H,255,255,255);
    DrawSync(0);
}

int main(void){
    video_init();
    for(;;){
        pattern();
        VSync(0);
    }
    return 0;
}
