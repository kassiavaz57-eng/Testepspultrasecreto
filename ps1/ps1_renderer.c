#include "ps1_renderer.h"
#include "runner.h"
#include "utils.h"
#include "log.h"
#include "ps1_texture_cache.h"
#include <psxgpu.h>
#include <psxetc.h>
#include <string.h>
#include <math.h>

#define PS1_WIDTH 320
#define PS1_HEIGHT 240
#define PS1_OT_LENGTH 32
#define PS1_PACKET_BYTES 32768
#define PS1_Z 8

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[PS1_OT_LENGTH];
    uint8_t packet[PS1_PACKET_BYTES];
} Ps1RenderBuffer;

typedef struct {
    Renderer base;
    Ps1RenderBuffer buffers[2];
    uint8_t* nextPacket;
    int active;
    float scaleX;
    float scaleY;
    float offsetX;
    float offsetY;
    int32_t viewX;
    int32_t viewY;
    int32_t blendMode;
    BlendFactors blendFactors;
    bool blendEnable;
    bool alphaTestEnable;
    uint8_t alphaTestRef;
    bool colorWriteR, colorWriteG, colorWriteB, colorWriteA;
    Ps1TextureCache textures;
    bool texturesReady;
} Ps1Renderer;

static Ps1Renderer* gPs1Renderer = NULL;

static void* allocPrim(Ps1Renderer* ps1, size_t size) {
    Ps1RenderBuffer* b = &ps1->buffers[ps1->active];
    if ((size_t)(ps1->nextPacket - b->packet) + size > PS1_PACKET_BYTES)
        return NULL;
    void* p = ps1->nextPacket;
    ps1->nextPacket += size;
    return p;
}

static void queuePrim(Ps1Renderer* ps1, void* prim) {
    Ps1RenderBuffer* b = &ps1->buffers[ps1->active];
    addPrim(&b->ot[PS1_Z], prim);
}

static uint8_t cr(uint32_t c) { return (uint8_t)(c & 0xff); }
static uint8_t cg(uint32_t c) { return (uint8_t)((c >> 8) & 0xff); }
static uint8_t cb(uint32_t c) { return (uint8_t)((c >> 16) & 0xff); }
static uint8_t ca(float a) { if (a <= 0.0f) return 0; if (a >= 1.0f) return 255; return (uint8_t)(a * 255.0f); }

static int sx(Ps1Renderer* p, float x) { return (int)((x - p->viewX) * p->scaleX + p->offsetX); }
static int sy(Ps1Renderer* p, float y) { return (int)((y - p->viewY) * p->scaleY + p->offsetY); }

static void ps1Init(Renderer* renderer, DataWin* dataWin) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    renderer->dataWin = dataWin;
    renderer->drawColor = 0xFFFFFF;
    renderer->drawAlpha = 1.0f;
    renderer->drawFont = -1;
    renderer->drawHalign = 0;
    renderer->drawValign = 0;
    renderer->circlePrecision = 24;
    p->scaleX = 1.0f;
    p->scaleY = 1.0f;
    p->blendEnable = true;
    p->colorWriteR = p->colorWriteG = p->colorWriteB = p->colorWriteA = true;
    p->blendMode = bm_normal;
    p->texturesReady = Ps1TextureCache_init(&p->textures);
    if (!p->texturesReady)
        logWarn("Ps1Renderer: real texture cache unavailable; sprite calls will be skipped\n");
    FntLoad(960, 0);
    logInfo("Ps1Renderer: GPU backend initialized\n");
}

static void ps1Destroy(Renderer* renderer) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    if (p->texturesReady) Ps1TextureCache_shutdown(&p->textures);
    if (gPs1Renderer == p) gPs1Renderer = NULL;
    free(p);
}

static void ps1BeginFrame(Renderer* renderer, int32_t gameW, int32_t gameH, int32_t windowW, int32_t windowH) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    (void)windowW; (void)windowH;
    p->scaleX = (gameW > 0) ? (float)PS1_WIDTH / (float)gameW : 1.0f;
    p->scaleY = (gameH > 0) ? (float)PS1_HEIGHT / (float)gameH : 1.0f;
    if (p->scaleY < p->scaleX) p->scaleX = p->scaleY;
    p->scaleY = p->scaleX;
    p->offsetX = ((float)PS1_WIDTH - (float)gameW * p->scaleX) * 0.5f;
    p->offsetY = ((float)PS1_HEIGHT - (float)gameH * p->scaleY) * 0.5f;
    ClearOTagR(p->buffers[p->active].ot, PS1_OT_LENGTH);
    p->nextPacket = p->buffers[p->active].packet;
}

static void ps1EndFrameInit(Renderer* renderer) { (void)renderer; }
static void ps1EndFrameEnd(Renderer* renderer) { (void)renderer; }

static void ps1BeginView(Renderer* renderer, int32_t viewX, int32_t viewY, int32_t viewW, int32_t viewH, int32_t portX, int32_t portY, int32_t portW, int32_t portH, float angle) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    (void)portX; (void)portY; (void)portW; (void)portH; (void)angle;
    p->viewX = viewX; p->viewY = viewY;
    p->scaleX = viewW > 0 ? (float)PS1_WIDTH / (float)viewW : 1.0f;
    p->scaleY = viewH > 0 ? (float)PS1_HEIGHT / (float)viewH : 1.0f;
    if (p->scaleY < p->scaleX) p->scaleX = p->scaleY;
    p->scaleY = p->scaleX;
    p->offsetX = ((float)PS1_WIDTH - viewW * p->scaleX) * 0.5f;
    p->offsetY = ((float)PS1_HEIGHT - viewH * p->scaleY) * 0.5f;
}
static void ps1EndView(Renderer* renderer) { (void)renderer; }
static void ps1ApplyProjection(Renderer* renderer, const Matrix4f* a, const Matrix4f* b) { (void)renderer; (void)a; (void)b; }
static void ps1BeginGUI(Renderer* renderer, int32_t guiW, int32_t guiH, int32_t px, int32_t py, int32_t pw, int32_t ph, int32_t target) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    (void)px; (void)py; (void)pw; (void)ph; (void)target;
    p->viewX = 0; p->viewY = 0;
    p->scaleX = guiW > 0 ? (float)PS1_WIDTH / guiW : 1.0f;
    p->scaleY = guiH > 0 ? (float)PS1_HEIGHT / guiH : 1.0f;
    if (p->scaleY < p->scaleX) p->scaleX = p->scaleY;
    p->scaleY = p->scaleX;
    p->offsetX = ((float)PS1_WIDTH - guiW * p->scaleX) * 0.5f;
    p->offsetY = ((float)PS1_HEIGHT - guiH * p->scaleY) * 0.5f;
}
static void ps1SetGuiProjection(Renderer* renderer, int32_t guiW, int32_t guiH, int32_t pw, int32_t ph, bool surface) { (void)pw; (void)ph; (void)surface; ps1BeginGUI(renderer, guiW, guiH, 0, 0, 0, 0, APPLICATION_SURFACE_ID); }
static void ps1EndGUI(Renderer* renderer) { (void)renderer; }

static void ps1DrawRectangle(Renderer* renderer, float x1, float y1, float x2, float y2, uint32_t color, float alpha, bool outline) {
    Ps1Renderer* p = (Ps1Renderer*)renderer;
    int ax=sx(p,x1), ay=sy(p,y1), bx=sx(p,x2), by=sy(p,y2);
    if (outline) {
        TILE* t=(TILE*)allocPrim(p,sizeof(TILE)); if(t){setTile(t);setXY0(t,ax,ay);setWH(t,bx-ax,1);setRGB0(t,cr(color),cg(color),cb(color));queuePrim(p,t);}
        t=(TILE*)allocPrim(p,sizeof(TILE)); if(t){setTile(t);setXY0(t,ax,by-1);setWH(t,bx-ax,1);setRGB0(t,cr(color),cg(color),cb(color));queuePrim(p,t);}
        t=(TILE*)allocPrim(p,sizeof(TILE)); if(t){setTile(t);setXY0(t,ax,ay);setWH(t,1,by-ay);setRGB0(t,cr(color),cg(color),cb(color));queuePrim(p,t);}
        t=(TILE*)allocPrim(p,sizeof(TILE)); if(t){setTile(t);setXY0(t,bx-1,ay);setWH(t,1,by-ay);setRGB0(t,cr(color),cg(color),cb(color));queuePrim(p,t);}
    } else {
        TILE* t=(TILE*)allocPrim(p,sizeof(TILE)); if(!t)return;
        setTile(t); setXY0(t,ax,ay); setWH(t,bx-ax,by-ay); setRGB0(t,cr(color),cg(color),cb(color));
        if(alpha<1.0f)setSemiTrans(t,1); queuePrim(p,t);
    }
}
static void ps1DrawRectangleColor(Renderer*r,float x1,float y1,float x2,float y2,uint32_t c1,uint32_t c2,uint32_t c3,uint32_t c4,float a,bool o){(void)c2;(void)c3;(void)c4;ps1DrawRectangle(r,x1,y1,x2,y2,c1,a,o);}
static void ps1DrawLine(Renderer* r,float x1,float y1,float x2,float y2,float width,uint32_t color,float alpha){Ps1Renderer*p=(Ps1Renderer*)r;LINE_F2*l=(LINE_F2*)allocPrim(p,sizeof(LINE_F2));if(!l)return;setLineF2(l);setXY2(l,sx(p,x1),sy(p,y1),sx(p,x2),sy(p,y2));setRGB0(l,cr(color),cg(color),cb(color));if(alpha<1)setSemiTrans(l,1);queuePrim(p,l);(void)width;}
static void ps1DrawLineColor(Renderer*r,float x1,float y1,float x2,float y2,float w,uint32_t c1,uint32_t c2,float a){(void)c2;ps1DrawLine(r,x1,y1,x2,y2,w,c1,a);}
static void ps1DrawTriangle(Renderer*r,float x1,float y1,float x2,float y2,float x3,float y3,uint32_t c1,uint32_t c2,uint32_t c3,float a,bool o){(void)c2;(void)c3;(void)o;Ps1Renderer*p=(Ps1Renderer*)r;POLY_F3*q=(POLY_F3*)allocPrim(p,sizeof(POLY_F3));if(!q)return;setPolyF3(q);setXY3(q,sx(p,x1),sy(p,y1),sx(p,x2),sy(p,y2),sx(p,x3),sy(p,y3));setRGB0(q,cr(c1),cg(c1),cb(c1));if(a<1)setSemiTrans(q,1);queuePrim(p,q);}

static void rotatePoint(float cx,float cy,float x,float y,float rad,float* ox,float* oy){float dx=x-cx,dy=y-cy,s=sinf(rad),c=cosf(rad);*ox=cx+dx*c-dy*s;*oy=cy+dx*s+dy*c;}

static bool resolveSprite(Ps1Renderer* p,int32_t tpagIndex,uint16_t* tpage,uint16_t* clut,int16_t* u,int16_t* v,uint16_t* w,uint16_t* h){
    if(!p->texturesReady)return false;
    return Ps1TextureCache_resolveTPAG(&p->textures,tpagIndex,tpage,clut,u,v,w,h);
}

static void ps1DrawSpritePart(Renderer*r,int32_t t,int32_t sx0,int32_t sy0,int32_t sw,int32_t sh,float x,float y,float xs,float ys,float ang,float px,float py,uint32_t color,float alpha){
    Ps1Renderer*p=(Ps1Renderer*)r;uint16_t tp,cl;int16_t u,v;uint16_t tw,th;
    if(!resolveSprite(p,t,&tp,&cl,&u,&v,&tw,&th))return;
    if(sw<=0||sh<=0)return;
    int iu=u+sx0,iv=v+sy0;
    float x0=x-px*xs,y0=y-py*ys,x1=x0+sw*xs,y1=y0+sh*ys;
    float ax,ay,bx,by,cx,cy,dx,dy,rad=ang*0.017453292519943295f;
    rotatePoint(x,y,x0,y0,rad,&ax,&ay);rotatePoint(x,y,x1,y0,rad,&bx,&by);rotatePoint(x,y,x1,y1,rad,&cx,&cy);rotatePoint(x,y,x0,y1,rad,&dx,&dy);
    POLY_FT4*q=(POLY_FT4*)allocPrim(p,sizeof(POLY_FT4));if(!q)return;setPolyFT4(q);setXY4(q,sx(p,ax),sy(p,ay),sx(p,bx),sy(p,by),sx(p,cx),sy(p,cy));setUV4(q,(uint8_t)iu,(uint8_t)iv,(uint8_t)(iu+sw),(uint8_t)iv,(uint8_t)(iu+sw),(uint8_t)(iv+sh),(uint8_t)iu,(uint8_t)(iv+sh));setClut(q,cl);setTPage(q,tp);setRGB0(q,cr(color),cg(color),cb(color));if(alpha<1)setSemiTrans(q,1);queuePrim(p,q);
}
static void ps1DrawSprite(Renderer*r,int32_t t,float x,float y,float ox,float oy,float xs,float ys,float ang,uint32_t c,float a){Ps1Renderer*p=(Ps1Renderer*)r;uint16_t tp,cl;int16_t u,v;uint16_t w,h;if(!resolveSprite(p,t,&tp,&cl,&u,&v,&w,&h))return;ps1DrawSpritePart(r,t,0,0,w,h,x,y,xs,ys,ang,ox,oy,c,a);}
static void ps1DrawSpritePartColor(Renderer*r,int32_t t,int32_t x0,int32_t y0,int32_t w,int32_t h,float x,float y,float xs,float ys,float ang,float px,float py,uint32_t c1,uint32_t c2,uint32_t c3,uint32_t c4,float a){(void)c2;(void)c3;(void)c4;ps1DrawSpritePart(r,t,x0,y0,w,h,x,y,xs,ys,ang,px,py,c1,a);}
static void ps1DrawSpritePos(Renderer*r,int32_t t,float x1,float y1,float x2,float y2,float x3,float y3,float x4,float y4,float a){Ps1Renderer*p=(Ps1Renderer*)r;uint16_t tp,cl;int16_t u,v;uint16_t w,h;if(!resolveSprite(p,t,&tp,&cl,&u,&v,&w,&h))return;POLY_FT4*q=(POLY_FT4*)allocPrim(p,sizeof(POLY_FT4));if(!q)return;setPolyFT4(q);setXY4(q,sx(p,x1),sy(p,y1),sx(p,x2),sy(p,y2),sx(p,x3),sy(p,y3),sx(p,x4),sy(p,y4));setUV4(q,(uint8_t)u,(uint8_t)v,(uint8_t)(u+w),(uint8_t)v,(uint8_t)(u+w),(uint8_t)(v+h),(uint8_t)u,(uint8_t)(v+h));setClut(q,cl);setTPage(q,tp);setRGB0(q,255,255,255);if(a<1)setSemiTrans(q,1);queuePrim(p,q);}
static void ps1DrawSpriteTiled(Renderer*r,int32_t t,float ox,float oy,float x,float y,float xs,float ys,bool tx,bool ty,float rw,float rh,uint32_t c,float a){(void)tx;(void)ty;(void)rw;(void)rh;ps1DrawSprite(r,t,x,y,ox,oy,xs,ys,0,c,a);}
static void ps1DrawTile(Renderer*r,RoomTile*t,float ox,float oy){(void)r;(void)t;(void)ox;(void)oy;}
static void ps1DrawTiledPart(Renderer*r,int32_t t,int32_t x,int32_t y,int32_t w,int32_t h,float dx,float dy,float dw,float dh,uint32_t c,float a){ps1DrawSpritePart(r,t,x,y,w,h,dx,dy,dw/(float)w,dh/(float)h,0,0,0,c,a);}

static void ps1DrawText(Renderer*r,const char*text,float x,float y,float xs,float ys,float ang,float sep){(void)xs;(void)ys;(void)ang;(void)sep;Ps1Renderer*p=(Ps1Renderer*)r;Ps1RenderBuffer*b=&p->buffers[p->active];p->nextPacket=(uint8_t*)FntSort(&b->ot[0],p->nextPacket,sx(p,x),sy(p,y),text);}
static void ps1DrawTextColor(Renderer*r,const char*t,float x,float y,float xs,float ys,float ang,int32_t c1,int32_t c2,int32_t c3,int32_t c4,float a,float sep){(void)c2;(void)c3;(void)c4;(void)a;ps1DrawText(r,t,x,y,xs,ys,ang,sep);}
static void ps1DrawTextUI(Renderer*r,const char*t,float x,float y,float xs,float ys,float ang,int32_t c1,int32_t c2,int32_t c3,int32_t c4,float a,float sep){ps1DrawTextColor(r,t,x,y,xs,ys,ang,c1,c2,c3,c4,a,sep);}
static void ps1PrimitiveBegin(Renderer*r,int32_t p){(void)r;(void)p;} static void ps1PrimitiveBeginTexture(Renderer*r,int32_t p,int32_t t){(void)r;(void)p;(void)t;} static void ps1PrimitiveEnd(Renderer*r){(void)r;} static void ps1DrawVertex(Renderer*r,float x,float y,float z,uint32_t c,float a,float u,float v){(void)r;(void)x;(void)y;(void)z;(void)c;(void)a;(void)u;(void)v;} static void ps1DrawVertexBuffer(Renderer*r,VertexBuffer*b,int32_t p,int32_t t,int32_t o,int32_t c){(void)r;(void)b;(void)p;(void)t;(void)o;(void)c;}
static void ps1Flush(Renderer*renderer){Ps1Renderer*p=(Ps1Renderer*)renderer;Ps1RenderBuffer*b=&p->buffers[p->active];DrawSync(0);DrawOTagEnv(&b->ot[PS1_OT_LENGTH-1],&b->draw);ClearOTagR(b->ot,PS1_OT_LENGTH);p->nextPacket=b->packet;}
static void ps1Clear(Renderer*r,uint32_t c,float a){ps1DrawRectangle(r,0,0,PS1_WIDTH,PS1_HEIGHT,c,a,false);}
static int32_t ps1CreateSpriteFromSurface(Renderer*r,int32_t s,int32_t x,int32_t y,int32_t w,int32_t h,bool rb,bool sm,int32_t xo,int32_t yo){(void)r;(void)s;(void)x;(void)y;(void)w;(void)h;(void)rb;(void)sm;(void)xo;(void)yo;return -1;} static void ps1DeleteSprite(Renderer*r,int32_t s){(void)r;(void)s;}
static BlendFactors ps1GetBlendFactors(Renderer*r){return ((Ps1Renderer*)r)->blendFactors;} static int32_t ps1GetBlendMode(Renderer*r){return ((Ps1Renderer*)r)->blendMode;} static void ps1SetBlendMode(Renderer*r,int32_t m){((Ps1Renderer*)r)->blendMode=m;} static void ps1SetBlendModeExt(Renderer*r,int32_t s,int32_t d,int32_t sa,int32_t da){Ps1Renderer*p=(Ps1Renderer*)r;p->blendFactors=(BlendFactors){s,d,sa,da};} static void ps1SetBlendEnable(Renderer*r,bool e){((Ps1Renderer*)r)->blendEnable=e;} static bool ps1GetBlendEnable(Renderer*r){return ((Ps1Renderer*)r)->blendEnable;} static void ps1SetAlphaTest(Renderer*r,bool e){((Ps1Renderer*)r)->alphaTestEnable=e;} static bool ps1GetAlphaTest(Renderer*r){return ((Ps1Renderer*)r)->alphaTestEnable;} static void ps1SetAlphaRef(Renderer*r,uint8_t v){((Ps1Renderer*)r)->alphaTestRef=v;} static void ps1SetColorWrite(Renderer*r,bool a,bool b,bool c,bool d){Ps1Renderer*p=(Ps1Renderer*)r;p->colorWriteR=a;p->colorWriteG=b;p->colorWriteB=c;p->colorWriteA=d;} static void ps1GetColorWrite(Renderer*r,bool*a,bool*b,bool*c,bool*d){Ps1Renderer*p=(Ps1Renderer*)r;if(a)*a=p->colorWriteR;if(b)*b=p->colorWriteG;if(c)*c=p->colorWriteB;if(d)*d=p->colorWriteA;} static void ps1SetFog(Renderer*r,bool e,uint32_t c){(void)r;(void)e;(void)c;}
static int32_t ps1CreateSurface(Renderer*r,int32_t w,int32_t h){(void)r;(void)w;(void)h;return -1;} static bool ps1SurfaceExists(Renderer*r,int32_t s){(void)r;(void)s;return false;} static bool ps1SetRenderTarget(Renderer*r,int32_t s,bool i){(void)r;(void)i;return s==APPLICATION_SURFACE_ID;} static int32_t ps1EnsureSurface(Renderer*r,int32_t w,int32_t h){(void)r;(void)w;(void)h;return APPLICATION_SURFACE_ID;} static float ps1SurfaceW(Renderer*r,int32_t s){(void)r;(void)s;return PS1_WIDTH;} static float ps1SurfaceH(Renderer*r,int32_t s){(void)r;(void)s;return PS1_HEIGHT;} static void ps1DrawSurface(Renderer*r,int32_t s,int32_t x,int32_t y,int32_t w,int32_t h,float dx,float dy,float xs,float ys,float a,uint32_t c,float al){(void)s;(void)x;(void)y;ps1DrawRectangle(r,dx,dy,dx+w*xs,dy+h*ys,c,al,false);} static void ps1DrawSurfaceColor(Renderer*r,int32_t s,int32_t x,int32_t y,int32_t w,int32_t h,float dx,float dy,float xs,float ys,float a,uint32_t c1,uint32_t c2,uint32_t c3,uint32_t c4,float al){(void)c2;(void)c3;(void)c4;ps1DrawSurface(r,s,x,y,w,h,dx,dy,xs,ys,a,c1,al);} static void ps1DrawSurfaceTiled(Renderer*r,int32_t s,float x,float y,float xs,float ys,float rw,float rh,uint32_t c,float a){(void)s;(void)rw;(void)rh;ps1DrawRectangle(r,x,y,x+PS1_WIDTH*xs,y+PS1_HEIGHT*ys,c,a,false);} static void ps1SurfaceResize(Renderer*r,int32_t s,int32_t w,int32_t h){(void)r;(void)s;(void)w;(void)h;} static void ps1SurfaceFree(Renderer*r,int32_t s){(void)r;(void)s;} static void ps1SurfaceCopy(Renderer*r,int32_t d,int32_t dx,int32_t dy,int32_t s,int32_t sx0,int32_t sy0,int32_t w,int32_t h,bool p){(void)r;(void)d;(void)dx;(void)dy;(void)s;(void)sx0;(void)sy0;(void)w;(void)h;(void)p;} static bool ps1SurfacePixels(Renderer*r,int32_t s,uint8_t*out){(void)r;(void)s;(void)out;return false;}
static uint32_t ps1SpriteTexture(Renderer*r,int32_t t){(void)r;return t>=0?(uint32_t)t+1:0;} static uint32_t ps1SurfaceTexture(Renderer*r,int32_t s){(void)r;(void)s;return 0;} static float ps1TexelW(Renderer*r,uint32_t t){(void)r;(void)t;return 1.0f/PS1_WIDTH;} static float ps1TexelH(Renderer*r,uint32_t t){(void)r;(void)t;return 1.0f/PS1_HEIGHT;} static bool ps1TextureUV(Renderer*r,uint32_t t,float*u){(void)r;(void)t;if(!u)return false;u[0]=u[1]=0;u[2]=u[3]=1;return true;} static void ps1TextureStage(Renderer*r,int32_t s,uint32_t t){(void)r;(void)s;(void)t;}
static void ps1ShaderVoid(Renderer*r,int32_t s){(void)r;(void)s;} static int32_t ps1ShaderInt(Renderer*r,int32_t s,char*u){(void)r;(void)s;(void)u;return -1;} static int32_t ps1ShaderSampler(Renderer*r,int32_t s,char*u){return ps1ShaderInt(r,s,u);} static void ps1ShaderF(Renderer*r,int32_t h,int32_t c,float a,float b,float d,float e){(void)r;(void)h;(void)c;(void)a;(void)b;(void)d;(void)e;} static void ps1ShaderFA(Renderer*r,int32_t h,float*v,uint32_t c){(void)r;(void)h;(void)v;(void)c;} static void ps1ShaderI(Renderer*r,int32_t h,int32_t c,int32_t a,int32_t b,int32_t d,int32_t e){(void)r;(void)h;(void)c;(void)a;(void)b;(void)d;(void)e;} static bool ps1ShaderCompiled(Renderer*r,int32_t s){(void)r;(void)s;return false;} static bool ps1ShadersSupported(void){return false;}
static void ps1SetMatrix(Renderer*r,int32_t t,Matrix4f m){(void)r;(void)t;(void)m;}

static RendererVtable ps1Vtable;
Renderer* Ps1Renderer_create(void){ResetGraph(0);Ps1Renderer*p=(Ps1Renderer*)safeMalloc(sizeof(Ps1Renderer));memset(p,0,sizeof(*p));p->base.vtable=&ps1Vtable;p->active=0;SetDefDispEnv(&p->buffers[0].disp,0,0,PS1_WIDTH,PS1_HEIGHT);SetDefDrawEnv(&p->buffers[0].draw,0,PS1_HEIGHT,PS1_WIDTH,PS1_HEIGHT);SetDefDispEnv(&p->buffers[1].disp,0,PS1_HEIGHT,PS1_WIDTH,PS1_HEIGHT);SetDefDrawEnv(&p->buffers[1].draw,0,0,PS1_WIDTH,PS1_HEIGHT);p->buffers[0].draw.isbg=1;p->buffers[1].draw.isbg=1;setRGB0(&p->buffers[0].draw,0,0,0);setRGB0(&p->buffers[1].draw,0,0,0);p->nextPacket=p->buffers[0].packet;ClearOTagR(p->buffers[0].ot,PS1_OT_LENGTH);PutDrawEnv(&p->buffers[0].draw);PutDispEnv(&p->buffers[0].disp);SetDispMask(1);gPs1Renderer=p;return (Renderer*)p;}
void Ps1Renderer_present(void){if(!gPs1Renderer)return;Ps1Renderer*p=gPs1Renderer;Ps1RenderBuffer*draw=&p->buffers[p->active];Ps1RenderBuffer*disp=&p->buffers[p->active^1];DrawSync(0);DrawOTagEnv(&draw->ot[PS1_OT_LENGTH-1],&draw->draw);VSync(0);PutDispEnv(&disp->disp);p->active^=1;p->nextPacket=p->buffers[p->active].packet;ClearOTagR(p->buffers[p->active].ot,PS1_OT_LENGTH);PutDrawEnv(&p->buffers[p->active].draw);}
static void initVtable(void){memset(&ps1Vtable,0,sizeof(ps1Vtable));ps1Vtable.init=ps1Init;ps1Vtable.destroy=ps1Destroy;ps1Vtable.beginFrame=ps1BeginFrame;ps1Vtable.endFrameInit=ps1EndFrameInit;ps1Vtable.endFrameEnd=ps1EndFrameEnd;ps1Vtable.beginView=ps1BeginView;ps1Vtable.endView=ps1EndView;ps1Vtable.applyProjection=ps1ApplyProjection;ps1Vtable.beginGUI=ps1BeginGUI;ps1Vtable.setGuiProjection=ps1SetGuiProjection;ps1Vtable.endGUI=ps1EndGUI;ps1Vtable.drawSprite=ps1DrawSprite;ps1Vtable.drawSpritePart=ps1DrawSpritePart;ps1Vtable.drawSpritePartColor=ps1DrawSpritePartColor;ps1Vtable.drawSpritePos=ps1DrawSpritePos;ps1Vtable.drawRectangle=ps1DrawRectangle;ps1Vtable.drawRectangleColor=ps1DrawRectangleColor;ps1Vtable.drawLine=ps1DrawLine;ps1Vtable.drawLineColor=ps1DrawLineColor;ps1Vtable.drawTriangle=ps1DrawTriangle;ps1Vtable.drawText=ps1DrawText;ps1Vtable.drawTextColor=ps1DrawTextColor;ps1Vtable.drawTextUI=ps1DrawTextUI;ps1Vtable.primitiveBegin=ps1PrimitiveBegin;ps1Vtable.primitiveBeginTexture=ps1PrimitiveBeginTexture;ps1Vtable.primitiveEnd=ps1PrimitiveEnd;ps1Vtable.drawVertex=ps1DrawVertex;ps1Vtable.drawVertexBuffer=ps1DrawVertexBuffer;ps1Vtable.flush=ps1Flush;ps1Vtable.clearScreen=ps1Clear;ps1Vtable.createSpriteFromSurface=ps1CreateSpriteFromSurface;ps1Vtable.deleteSprite=ps1DeleteSprite;ps1Vtable.gpuGetBlendFactors=ps1GetBlendFactors;ps1Vtable.gpuGetBlendMode=ps1GetBlendMode;ps1Vtable.gpuSetBlendMode=ps1SetBlendMode;ps1Vtable.gpuSetBlendModeExt=ps1SetBlendModeExt;ps1Vtable.gpuSetBlendEnable=ps1SetBlendEnable;ps1Vtable.gpuGetBlendEnable=ps1GetBlendEnable;ps1Vtable.gpuSetAlphaTestEnable=ps1SetAlphaTest;ps1Vtable.gpuGetAlphaTestEnable=ps1GetAlphaTest;ps1Vtable.gpuSetAlphaTestRef=ps1SetAlphaRef;ps1Vtable.gpuSetColorWriteEnable=ps1SetColorWrite;ps1Vtable.gpuGetColorWriteEnable=ps1GetColorWrite;ps1Vtable.gpuSetFog=ps1SetFog;ps1Vtable.drawTile=ps1DrawTile;ps1Vtable.drawSpriteTiled=ps1DrawSpriteTiled;ps1Vtable.drawTiledPart=ps1DrawTiledPart;ps1Vtable.createSurface=ps1CreateSurface;ps1Vtable.surfaceExists=ps1SurfaceExists;ps1Vtable.setRenderTarget=ps1SetRenderTarget;ps1Vtable.ensureApplicationSurface=ps1EnsureSurface;ps1Vtable.getSurfaceWidth=ps1SurfaceW;ps1Vtable.getSurfaceHeight=ps1SurfaceH;ps1Vtable.drawSurface=ps1DrawSurface;ps1Vtable.drawSurfaceColor=ps1DrawSurfaceColor;ps1Vtable.drawSurfaceTiled=ps1DrawSurfaceTiled;ps1Vtable.surfaceResize=ps1SurfaceResize;ps1Vtable.surfaceFree=ps1SurfaceFree;ps1Vtable.surfaceCopy=ps1SurfaceCopy;ps1Vtable.surfaceGetPixels=ps1SurfacePixels;ps1Vtable.spriteGetTexture=ps1SpriteTexture;ps1Vtable.surfaceGetTexture=ps1SurfaceTexture;ps1Vtable.textureGetTexelWidth=ps1TexelW;ps1Vtable.textureGetTexelHeight=ps1TexelH;ps1Vtable.textureGetUVs=ps1TextureUV;ps1Vtable.textureSetStage=ps1TextureStage;ps1Vtable.gpuSetShader=ps1ShaderVoid;ps1Vtable.gpuResetShader=ps1ShaderVoid;ps1Vtable.shaderGetUniform=ps1ShaderInt;ps1Vtable.shaderGetSamplerIndex=ps1ShaderSampler;ps1Vtable.shaderSetUniformF=ps1ShaderF;ps1Vtable.shaderSetUniformFArray=ps1ShaderFA;ps1Vtable.shaderSetUniformI=ps1ShaderI;ps1Vtable.shaderIsCompiled=ps1ShaderCompiled;ps1Vtable.shadersSupported=ps1ShadersSupported;ps1Vtable.setMatrix=ps1SetMatrix;}
__attribute__((constructor)) static void ps1RendererConstruct(void){initVtable();}
