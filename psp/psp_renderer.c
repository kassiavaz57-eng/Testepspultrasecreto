#include "psp_renderer.h"
#include "noop_renderer.h"
#include "image/image_decoder.h"
#include "data_win.h"
#include "common.h"
#include "log.h"
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PSP_W 480
#define PSP_H 272
#define PSP_BUF_W 512
#define PSP_TEX_MAX 512
typedef struct { float u,v; unsigned int color; float x,y,z; } PSPVertex;
static unsigned int __attribute__((aligned(16))) g_list[65536/sizeof(unsigned int)];
static unsigned char __attribute__((aligned(16))) g_textureScratch[PSP_TEX_MAX*PSP_TEX_MAX*4];
static RendererVtable *g_baseVtable=NULL;
static RendererVtable *g_pspVtable=NULL;
static int g_guReady=0;
static uint8_t *g_cachedPixels=NULL;
static size_t g_cachedPixelsSize=0;
static int g_cachedPage=-1,g_cachedW=0,g_cachedH=0;
static int nextPow2(int v){int n=1;while(n<v&&n<PSP_TEX_MAX)n<<=1;return n;}
static void cacheClear(void){free(g_cachedPixels);g_cachedPixels=NULL;g_cachedPixelsSize=0;g_cachedPage=-1;g_cachedW=g_cachedH=0;}
static uint32_t bgrToGu(uint32_t c,float alpha){unsigned int a=(unsigned int)(alpha*255.0f);if(a>255)a=255;return GU_RGBA(BGR_R(c),BGR_G(c),BGR_B(c),a);}
static void setOrtho(float l,float r,float t,float b,int px,int py,int pw,int ph){
 sceGuViewport(2048+px+pw/2,2048+py+ph/2,pw,ph); sceGuScissor(px,py,px+pw,py+ph);
 sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumOrtho(l,r,b,t,-1.0f,1.0f);
 sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity(); sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
}
static bool loadPage(DataWin *dw,int pageId){
 if(pageId<0||(uint32_t)pageId>=dw->txtr.count)return false;
 DataWin_loadTxtrIfNeeded(dw,(uint32_t)pageId); Texture *tex=&dw->txtr.textures[pageId];
 if(!tex->blobData||tex->blobSize==0)return false;
 if(g_cachedPage==pageId&&g_cachedPixels)return true;
 cacheClear(); int w=0,h=0; bool modern=DataWin_isVersionAtLeast(dw,2022,5,0,0);
 uint8_t *p=ImageDecoder_decodeToRgba(tex->blobData,tex->blobSize,modern,&w,&h);
 if(!p||w<=0||h<=0){free(p);return false;} g_cachedPixels=p;g_cachedPixelsSize=(size_t)w*h*4;g_cachedPage=pageId;g_cachedW=w;g_cachedH=h;return true;
}
static bool uploadRect(DataWin *dw,int pageId,int sx,int sy,int sw,int sh,int *twOut,int *thOut){
 if(sw<=0||sh<=0||sw>PSP_TEX_MAX||sh>PSP_TEX_MAX)return false;
 if(!loadPage(dw,pageId)||sx<0||sy<0||sx+sw>g_cachedW||sy+sh>g_cachedH)return false;
 int tw=nextPow2(sw),th=nextPow2(sh);if(tw>PSP_TEX_MAX||th>PSP_TEX_MAX)return false;
 memset(g_textureScratch,0,sizeof(g_textureScratch));
 for(int y=0;y<sh;y++) memcpy(g_textureScratch+(size_t)y*PSP_TEX_MAX*4,g_cachedPixels+((size_t)(sy+y)*g_cachedW+sx)*4,(size_t)sw*4);
 sceKernelDcacheWritebackAll(); sceGuTexMode(GU_PSM_8888,0,0,GU_FALSE); sceGuTexImage(0,tw,th,PSP_TEX_MAX,g_textureScratch);
 sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA); sceGuTexFilter(GU_NEAREST,GU_NEAREST); sceGuTexFlush();
 *twOut=tw;*thOut=th;return true;
}
static void releaseLargePageCache(void){if(g_cachedPixels&&g_cachedPixelsSize>(size_t)8*1024*1024)cacheClear();}
static void drawQuad(float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,float u0,float v0,float u1,float v1,uint32_t c0,uint32_t c1,uint32_t c2,uint32_t c3){
 PSPVertex *v=(PSPVertex*)sceGuGetMemory(4*sizeof(PSPVertex));
 v[0]=(PSPVertex){u0,v0,c0,x0,y0,0};v[1]=(PSPVertex){u1,v0,c1,x1,y1,0};v[2]=(PSPVertex){u1,v1,c2,x2,y2,0};v[3]=(PSPVertex){u0,v1,c3,x3,y3,0};
 sceGuDrawArray(GU_TRIANGLE_FAN,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,NULL,v);
}
    Matrix4f_identity(&world);
    renderer->gmlMatrices[MATRIX_WORLD] = world;
    renderer->drawColor = 0xFFFFFF;
    renderer->drawAlpha = 1.0f;
    renderer->drawFont = -1;
    renderer->currentShader = -1;

    void *fb0 = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_8888);
    void *fb1 = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_8888);
    void *zb = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_4444);

    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, PSP_BUF_W);
    sceGuDispBuffer(PSP_W, PSP_H, fb1, PSP_BUF_W);
    sceGuDepthBuffer(zb, PSP_BUF_W);
    sceGuOffset(2048 - PSP_W / 2, 2048 - PSP_H / 2);
    sceGuViewport(2048, 2048, PSP_W, PSP_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, PSP_W, PSP_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuDisplay(GU_DISPLAY_ON);
    g_guReady = 1;
    logInfo("PSP GU renderer initialized\n");
}

static void pspDestroy(Renderer *renderer) {
    cacheClear();
    if (g_guReady) {
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        sceGuTerm();
        g_guReady = 0;
    }
    RendererVtable *vt = renderer->vtable;
    renderer->vtable = g_baseVtable;
    if (g_baseVtable && g_baseVtable->destroy)
        g_baseVtable->destroy(renderer);
    free(vt);
    g_pspVtable = NULL;
    g_baseVtable = NULL;
}

static void pspBeginFrame(Renderer *renderer, int32_t gameW, int32_t gameH, int32_t windowW, int32_t windowH) {
    (void)windowW; (void)windowH;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(GU_RGBA(0, 0, 0, 255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    setOrtho(0, (float)gameW, 0, (float)gameH, 0, 0, PSP_W, PSP_H);
    renderer->CPortX = 0;
    renderer->CPortY = 0;
    renderer->CPortW = PSP_W;
    renderer->CPortH = PSP_H;
}

static void pspEndFrameInit(Renderer *renderer) {
    (void)renderer;
}

static void pspEndFrameEnd(Renderer *renderer) {
    (void)renderer;
    releaseLargePageCache();
}

static void pspBeginView(Renderer *renderer, int32_t viewX, int32_t viewY, int32_t viewW, int32_t viewH, int32_t portX, int32_t portY, int32_t portW, int32_t portH, float viewAngle) {
    (void)viewAngle;
    renderer->CPortX = portX; renderer->CPortY = portY; renderer->CPortW = portW; renderer->CPortH = portH;
    setOrtho((float)viewX, (float)(viewX + viewW), (float)viewY, (float)(viewY + viewH), portX, portY, portW, portH);
}

static void pspEndView(Renderer *renderer) { (void)renderer; }

static void pspBeginGUI(Renderer *renderer, int32_t guiW, int32_t guiH, int32_t portX, int32_t portY, int32_t portW, int32_t portH, int32_t targetSurfaceId) {
    (void)targetSurfaceId;
    renderer->CPortX = portX; renderer->CPortY = portY; renderer->CPortW = portW; renderer->CPortH = portH;
    setOrtho(0, (float)guiW, 0, (float)guiH, portX, portY, portW, portH);
}

static void pspSetGuiProjection(Renderer *renderer, int32_t guiW, int32_t guiH, int32_t portW, int32_t portH, bool renderingToUserSurface) {
    (void)renderer; (void)renderingToUserSurface;
    setOrtho(0, (float)guiW, 0, (float)guiH, 0, 0, portW, portH);
}

static void pspEndGUI(Renderer *renderer) { (void)renderer; }

static void drawQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                     float u0, float v0, float u1, float v1, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) {
    PSPVertex *v = (PSPVertex *)sceGuGetMemory(4 * sizeof(PSPVertex));