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
#include <stdio.h>
#include <stdarg.h>
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
static unsigned long g_drawCalls=0,g_uploadFails=0,g_uploadFailSize=0,g_uploadFailLoad=0,g_uploadFailBounds=0,g_uploadFailPow2=0,g_lastReportMs=0;
static void pspDiagReport(void){ unsigned long now=(unsigned long)(sceKernelGetSystemTimeWide()/1000ULL); if(now-g_lastReportMs>=1000){ logInfo("PSP_DIAG draws=%lu fails=%lu size=%lu load=%lu bounds=%lu pow2=%lu\\n",g_drawCalls,g_uploadFails,g_uploadFailSize,g_uploadFailLoad,g_uploadFailBounds,g_uploadFailPow2); g_drawCalls=g_uploadFails=g_uploadFailSize=g_uploadFailLoad=g_uploadFailBounds=g_uploadFailPow2=0; g_lastReportMs=now; } }
static FILE *g_diagFile=NULL;
static void pspDiagFileWrite(const char *fmt,...){ if(!g_diagFile) g_diagFile=fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_diag.txt","a"); if(!g_diagFile)return; va_list ap; va_start(ap,fmt); vfprintf(g_diagFile,fmt,ap); va_end(ap); fflush(g_diagFile); }
static void pspDiagFileReport(void){ pspDiagFileWrite("PSP_DIAG draws=%lu fails=%lu size=%lu load=%lu bounds=%lu pow2=%lu\\n",g_drawCalls,g_uploadFails,g_uploadFailSize,g_uploadFailLoad,g_uploadFailBounds,g_uploadFailPow2); }
static int nextPow2(int v){int n=1;while(n<v&&n<PSP_TEX_MAX)n<<=1;return n;}
static void cacheClear(void){free(g_cachedPixels);g_cachedPixels=NULL;g_cachedPixelsSize=0;g_cachedPage=-1;g_cachedW=g_cachedH=0;}
static uint32_t bgrToGu(uint32_t c,float alpha){unsigned int a=(unsigned int)(alpha*255.0f);if(a>255)a=255;return GU_RGBA(BGR_R(c),BGR_G(c),BGR_B(c),a);}
static float g_viewX=0.0f,g_viewY=0.0f,g_scaleX=1.0f,g_scaleY=1.0f,g_offX=0.0f,g_offY=0.0f;
static void setViewTransform(float viewX,float viewY,float viewW,float viewH,int px,int py,int pw,int ph){
    sceGuViewport(2048,2048,pw,ph);
    sceGuScissor(px,py,px+pw,py+ph);
    g_viewX=viewX; g_viewY=viewY;
    g_scaleX=(viewW!=0.0f)?((float)pw/viewW):1.0f;
    g_scaleY=(viewH!=0.0f)?((float)ph/viewH):1.0f;
    g_offX=(float)px; g_offY=(float)py;
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
 if(sw<=0||sh<=0||sw>PSP_TEX_MAX||sh>PSP_TEX_MAX){g_uploadFails++;g_uploadFailSize++;logWarn("PSP_DIAG SIZE page=%d sw=%d sh=%d\\n",pageId,sw,sh);return false;}
 if(!loadPage(dw,pageId)){g_uploadFails++;g_uploadFailLoad++;logWarn("PSP_DIAG LOAD page=%d\\n",pageId);return false;}
 if(sx<0||sy<0||sx+sw>g_cachedW||sy+sh>g_cachedH){g_uploadFails++;g_uploadFailBounds++;logWarn("PSP_DIAG BOUNDS page=%d sx=%d sy=%d sw=%d sh=%d cached=%dx%d\\n",pageId,sx,sy,sw,sh,g_cachedW,g_cachedH);return false;}
 int tw=nextPow2(sw),th=nextPow2(sh);if(tw>PSP_TEX_MAX||th>PSP_TEX_MAX){g_uploadFails++;g_uploadFailPow2++;logWarn("PSP_DIAG POW2 page=%d tw=%d th=%d\\n",pageId,tw,th);return false;}
 memset(g_textureScratch,0,sizeof(g_textureScratch));
 for(int y=0;y<sh;y++) memcpy(g_textureScratch+(size_t)y*PSP_TEX_MAX*4,g_cachedPixels+((size_t)(sy+y)*g_cachedW+sx)*4,(size_t)sw*4);
 sceKernelDcacheWritebackInvalidateAll(); sceGuTexMode(GU_PSM_8888,0,0,GU_FALSE); sceGuTexImage(0,tw,th,PSP_TEX_MAX,g_textureScratch);
 sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA); sceGuTexFilter(GU_NEAREST,GU_NEAREST); sceGuTexFlush(); sceGuTexSync();
 *twOut=tw;*thOut=th;return true;
}
static void drawQuad(float x0,float y0,float x1,float y1,float x2,float y2,float x3,float y3,float u0,float v0,float u1,float v1,uint32_t c0,uint32_t c1,uint32_t c2,uint32_t c3){
    float xs[4]={x0,x1,x2,x3},ys[4]={y0,y1,y2,y3};
    PSPVertex *v=(PSPVertex*)sceGuGetMemory(4*sizeof(PSPVertex));
    for(int i=0;i<4;i++){
        float tx=(xs[i]-g_viewX)*g_scaleX+g_offX;
        float ty=(ys[i]-g_viewY)*g_scaleY+g_offY;
        v[i]=(PSPVertex){i==1||i==2?u1:u0,i>=2?v1:v0,i==0?c0:i==1?c1:i==2?c2:c3,tx,ty,0};
    }
    sceGuDrawArray(GU_TRIANGLE_FAN,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,NULL,v);
}
static void pspInit(Renderer *renderer, DataWin *dataWin) {
    renderer->dataWin = dataWin;
    Matrix4f world;
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
    if (g_baseVtable && g_baseVtable->destroy) g_baseVtable->destroy(renderer);
    free(vt);
    g_pspVtable = NULL;
    g_baseVtable = NULL;
}

static void pspBeginFrame(Renderer *renderer, int32_t gameW, int32_t gameH, int32_t windowW, int32_t windowH) {
    (void)windowW; (void)windowH;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(GU_RGBA(0,0,0,255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    setViewTransform(0,0,(float)gameW,(float)gameH,0,0,PSP_W,PSP_H);
    renderer->CPortX=0; renderer->CPortY=0; renderer->CPortW=PSP_W; renderer->CPortH=PSP_H;
}
static void pspEndFrameInit(Renderer *renderer){(void)renderer;}
static void pspEndFrameEnd(Renderer *renderer){(void)renderer;pspDiagReport();pspDiagFileReport();}
static void pspBeginView(Renderer *renderer,int32_t viewX,int32_t viewY,int32_t viewW,int32_t viewH,int32_t portX,int32_t portY,int32_t portW,int32_t portH,float viewAngle){
    (void)viewAngle;
    renderer->CPortX=portX;renderer->CPortY=portY;renderer->CPortW=portW;renderer->CPortH=portH;
    // Preserve the GameMaker camera aspect ratio instead of stretching a 4:3 room
    // directly into the PSP 16:9 framebuffer. This is also the native-feeling
    // framing used by Undertale: the full room view remains visible with pillarbox bars.
    float sx=(viewW>0)?((float)portW/(float)viewW):1.0f;
    float sy=(viewH>0)?((float)portH/(float)viewH):1.0f;
    float scale=(sx<sy)?sx:sy;
    int fitW=(int)floorf((float)viewW*scale+0.5f);
    int fitH=(int)floorf((float)viewH*scale+0.5f);
    int fitX=portX+(portW-fitW)/2;
    int fitY=portY+(portH-fitH)/2;
    setViewTransform((float)viewX,(float)viewY,(float)viewW,(float)viewH,fitX,fitY,fitW,fitH);
}
static void pspEndView(Renderer *renderer){(void)renderer;}
static void pspBeginGUI(Renderer *renderer,int32_t guiW,int32_t guiH,int32_t portX,int32_t portY,int32_t portW,int32_t portH,int32_t targetSurfaceId){
    (void)targetSurfaceId; renderer->CPortX=portX;renderer->CPortY=portY;renderer->CPortW=portW;renderer->CPortH=portH;
    setViewTransform(0,0,(float)guiW,(float)guiH,portX,portY,portW,portH);
}
static void pspSetGuiProjection(Renderer *renderer,int32_t guiW,int32_t guiH,int32_t portW,int32_t portH,bool renderingToUserSurface){
    (void)renderer;(void)renderingToUserSurface;setViewTransform(0,0,(float)guiW,(float)guiH,0,0,portW,portH);
}
static void pspEndGUI(Renderer *renderer){(void)renderer;}

static void pspDrawSpritePartColor(Renderer *renderer,int32_t tpagIndex,int32_t srcOffX,int32_t srcOffY,int32_t srcW,int32_t srcH,float x,float y,float xscale,float yscale,float angleDeg,float pivotX,float pivotY,uint32_t color1,uint32_t color2,uint32_t color3,uint32_t color4,float alpha){
    g_drawCalls++;
    DataWin *dw=renderer->dataWin;
    if(!dw||tpagIndex<0||(uint32_t)tpagIndex>=dw->tpag.count)return;
    TexturePageItem *tpag=&dw->tpag.items[tpagIndex];
    int sx=(int)tpag->sourceX+srcOffX, sy=(int)tpag->sourceY+srcOffY, tw,th;
    if(!uploadRect(dw,tpag->texturePageId,sx,sy,srcW,srcH,&tw,&th))return;
    float qx[4]={x,x+srcW*xscale,x+srcW*xscale,x}, qy[4]={y,y,y+srcH*yscale,y+srcH*yscale};
    if(angleDeg!=0.0f){float a=-angleDeg*((float)M_PI/180.0f),ca=cosf(a),sa=sinf(a);for(int i=0;i<4;i++){float dx=qx[i]-pivotX,dy=qy[i]-pivotY;qx[i]=ca*dx-sa*dy+pivotX;qy[i]=sa*dx+ca*dy+pivotY;}}
    drawQuad(qx[0],qy[0],qx[1],qy[1],qx[2],qy[2],qx[3],qy[3],0,0,(float)srcW,(float)srcH,
        bgrToGu(color1,alpha),bgrToGu(color2,alpha),bgrToGu(color3,alpha),bgrToGu(color4,alpha));
}
static void pspDrawSpritePart(Renderer *renderer,int32_t tpagIndex,int32_t srcOffX,int32_t srcOffY,int32_t srcW,int32_t srcH,float x,float y,float xscale,float yscale,float angleDeg,float pivotX,float pivotY,uint32_t color,float alpha){
    pspDrawSpritePartColor(renderer,tpagIndex,srcOffX,srcOffY,srcW,srcH,x,y,xscale,yscale,angleDeg,pivotX,pivotY,color,color,color,color,alpha);
}
static void pspDrawSprite(Renderer *renderer,int32_t tpagIndex,float x,float y,float originX,float originY,float xscale,float yscale,float angleDeg,uint32_t color,float alpha){
    if(tpagIndex<0||(uint32_t)tpagIndex>=renderer->dataWin->tpag.count)return;
    TexturePageItem *t=&renderer->dataWin->tpag.items[tpagIndex];
    pspDrawSpritePart(renderer,tpagIndex,0,0,t->sourceWidth,t->sourceHeight,x-originX*xscale,y-originY*yscale,xscale,yscale,angleDeg,x,y,color,alpha);
}

static void pspDrawSpriteTiled(Renderer *renderer,int32_t tpagIndex,float originX,float originY,float x,float y,float xscale,float yscale,bool tileX,bool tileY,float roomW,float roomH,uint32_t color,float alpha){
    (void)originX; (void)originY;
    if(!renderer||!renderer->dataWin||tpagIndex<0||(uint32_t)tpagIndex>=renderer->dataWin->tpag.count)return;
    TexturePageItem *t=&renderer->dataWin->tpag.items[tpagIndex];
    if(t->sourceWidth<=0||t->sourceHeight<=0||xscale==0.0f||yscale==0.0f)return;
    float tileW=(float)t->sourceWidth*xscale, tileH=(float)t->sourceHeight*yscale;
    int maxY=tileY?((int)ceilf(roomH/fabsf(tileH))+1):1;
    int maxX=tileX?((int)ceilf(roomW/fabsf(tileW))+1):1;
    for(int iy=0;iy<maxY;iy++){
        float dy=y+(float)iy*tileH;
        float remainH=tileY?(roomH-(float)iy*fabsf(tileH)):(float)t->sourceHeight;
        if(tileY&&remainH<=0.0f)break;
        int sh=tileY&&remainH<(float)t->sourceHeight?(int)floorf(remainH/fabsf(yscale)):t->sourceHeight;
        if(sh<=0)break;
        for(int ix=0;ix<maxX;ix++){
            float dx=x+(float)ix*tileW;
            float remainW=tileX?(roomW-(float)ix*fabsf(tileW)):(float)t->sourceWidth;
            if(tileX&&remainW<=0.0f)break;
            int sw=tileX&&remainW<(float)t->sourceWidth?(int)floorf(remainW/fabsf(xscale)):t->sourceWidth;
            if(sw<=0)break;
            pspDrawSpritePart(renderer,tpagIndex,0,0,sw,sh,dx,dy,xscale,yscale,0.0f,dx,dy,color,alpha);
            if(!tileX)break;
        }
        if(!tileY)break;
    }
}
static void pspDrawTiledPart(Renderer *renderer,int32_t tpagIndex,int32_t srcX,int32_t srcY,int32_t srcW,int32_t srcH,float dstX,float dstY,float dstW,float dstH,uint32_t color,float alpha){
    if(!renderer||!renderer->dataWin||tpagIndex<0||(uint32_t)tpagIndex>=renderer->dataWin->tpag.count||srcW<=0||srcH<=0||dstW<=0.0f||dstH<=0.0f)return;
    TexturePageItem *t=&renderer->dataWin->tpag.items[tpagIndex];
    /* drawTiledPart coordinates are relative to the TPAG source rectangle. */
    int tileW=srcW,tileH=srcH;
    for(float y=dstY;y<dstY+dstH-0.001f;y+=(float)tileH){
        int h=(int)fminf((float)tileH,dstY+dstH-y);
        if(h<=0)break;
        for(float x=dstX;x<dstX+dstW-0.001f;x+=(float)tileW){
            int w=(int)fminf((float)tileW,dstX+dstW-x);
            if(w<=0)break;
            pspDrawSpritePart(renderer,tpagIndex,srcX,srcY,w,h,x,y,1.0f,1.0f,0.0f,x,y,color,alpha);
        }
    }
    (void)t;
}
static void pspDrawRectangle(Renderer *renderer,float x1,float y1,float x2,float y2,uint32_t color,float alpha,bool outline){
    (void)renderer; uint32_t c=bgrToGu(color,alpha); PSPVertex *v=(PSPVertex*)sceGuGetMemory(4*sizeof(PSPVertex));
    float tx1=(x1-g_viewX)*g_scaleX+g_offX, ty1=(y1-g_viewY)*g_scaleY+g_offY;
    float tx2=(x2-g_viewX)*g_scaleX+g_offX, ty2=(y2-g_viewY)*g_scaleY+g_offY;
    v[0]=(PSPVertex){0,0,c,tx1,ty1,0};v[1]=(PSPVertex){0,0,c,tx2,ty1,0};v[2]=(PSPVertex){0,0,c,tx2,ty2,0};v[3]=(PSPVertex){0,0,c,tx1,ty2,0};
    sceGuDisable(GU_TEXTURE_2D);sceGuDrawArray(outline?GU_LINE_STRIP:GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,NULL,v);sceGuEnable(GU_TEXTURE_2D);
}
static void pspClearScreen(Renderer *renderer,uint32_t color,float alpha){(void)renderer;sceGuClearColor(bgrToGu(color,alpha));sceGuClear(GU_COLOR_BUFFER_BIT);}

Renderer *PSPRenderer_create(void){
    Renderer *renderer=NoopRenderer_create(); if(!renderer)return NULL;
    g_baseVtable=renderer->vtable; g_pspVtable=(RendererVtable*)malloc(sizeof(RendererVtable));
    if(!g_pspVtable)return renderer;
    memcpy(g_pspVtable,g_baseVtable,sizeof(RendererVtable));
    g_pspVtable->init=pspInit; g_pspVtable->destroy=pspDestroy; g_pspVtable->beginFrame=pspBeginFrame;
    g_pspVtable->endFrameInit=pspEndFrameInit; g_pspVtable->endFrameEnd=pspEndFrameEnd;
    g_pspVtable->beginView=pspBeginView;g_pspVtable->endView=pspEndView;g_pspVtable->beginGUI=pspBeginGUI;
    g_pspVtable->setGuiProjection=pspSetGuiProjection;g_pspVtable->endGUI=pspEndGUI;
    g_pspVtable->drawSprite=pspDrawSprite;g_pspVtable->drawSpritePart=pspDrawSpritePart;
    g_pspVtable->drawSpritePartColor=pspDrawSpritePartColor;g_pspVtable->drawSpriteTiled=pspDrawSpriteTiled;g_pspVtable->drawTiledPart=pspDrawTiledPart;g_pspVtable->drawRectangle=pspDrawRectangle;
    g_pspVtable->clearScreen=pspClearScreen;
    renderer->vtable=g_pspVtable; return renderer;
}