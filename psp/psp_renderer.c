#include "psp_renderer.h"
#include "noop_renderer.h"
#include "image/image_decoder.h"
#include "data_win.h"
#include "common.h"
#include "text_utils.h"
#include "log.h"
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psputils.h>
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
#define PSP_TEX_PSM GU_PSM_4444
typedef struct { float u,v; unsigned int color; float x,y,z; } PSPVertex;
/* Large enough for Undertale rooms with many sprites/text glyphs; avoids display-list exhaustion/corruption. */
static unsigned int __attribute__((aligned(16))) g_list[262144/sizeof(unsigned int)];
#define PSP_TEX_CACHE_ENTRIES 128
#define PSP_TEX_CACHE_BYTES (8u*1024u*1024u)
typedef struct {
    int valid;
    int pageId, sx, sy, sw, sh, tw, th;
    uint8_t *pixels;
    size_t bytes;
    unsigned long lastUse;
    unsigned long lastFrame;
    int initialized;
} PSPTextureCacheEntry;
static PSPTextureCacheEntry g_texCache[PSP_TEX_CACHE_ENTRIES];
static size_t g_texCacheBytes=0;
static unsigned long g_texCacheClock=0;
static unsigned long g_texCacheFrame=0;
static PSPTextureCacheEntry *g_boundTexture=NULL;
static int g_boundTw=0,g_boundTh=0;
static RendererVtable *g_baseVtable=NULL;
static RendererVtable *g_pspVtable=NULL;
static int g_guReady=0;
static uint8_t *g_cachedPixels=NULL;
static size_t g_cachedPixelsSize=0;
static int g_cachedPage=-1,g_cachedW=0,g_cachedH=0;
static unsigned long g_drawCalls=0,g_uploadFails=0,g_uploadFailSize=0,g_uploadFailLoad=0,g_uploadFailBounds=0,g_uploadFailPow2=0,g_lastReportMs=0;
static unsigned long g_texHits=0,g_texMisses=0,g_texEvictions=0,g_texBinds=0,g_pageDecodes=0;
static unsigned long long g_pageDecodeUs=0,g_texCopyUs=0;

/* One-second CPU timing breakdown. This is diagnostic only: it does not
   alter rendering or scheduling. */
static unsigned long long g_frameLogicUs=0,g_frameDrawUs=0,g_frameSyncUs=0;
static unsigned long g_frameCount=0;
static uint64_t g_frameStartUs=0,g_drawStartUs=0;
static unsigned long g_lastDiagFrame=0;
static void pspDiagFileReport(void);
static void pspPerfFileReport(void);
static void pspDiagReport(void){ unsigned long now=(unsigned long)(sceKernelGetSystemTimeWide()/1000ULL); if(now-g_lastReportMs>=1000){ if(g_drawStartUs){ g_frameDrawUs += sceKernelGetSystemTimeWide()-g_drawStartUs; g_drawStartUs=0; } logInfo("PSP_DIAG draws=%lu fails=%lu size=%lu load=%lu bounds=%lu pow2=%lu\\n",g_drawCalls,g_uploadFails,g_uploadFailSize,g_uploadFailLoad,g_uploadFailBounds,g_uploadFailPow2); pspDiagFileReport(); pspPerfFileReport(); g_drawCalls=g_uploadFails=g_uploadFailSize=g_uploadFailLoad=g_uploadFailBounds=g_uploadFailPow2=0; g_lastReportMs=now; } }
static FILE *g_diagFile=NULL;
static void pspDiagFileWrite(const char *fmt,...){ if(!g_diagFile) g_diagFile=fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_diag.txt","a"); if(!g_diagFile)return; va_list ap; va_start(ap,fmt); vfprintf(g_diagFile,fmt,ap); va_end(ap); fflush(g_diagFile); }
static void pspDiagFileReport(void){ pspDiagFileWrite("PSP_DIAG draws=%lu fails=%lu size=%lu load=%lu bounds=%lu pow2=%lu\\n",g_drawCalls,g_uploadFails,g_uploadFailSize,g_uploadFailLoad,g_uploadFailBounds,g_uploadFailPow2); }
static void pspPerfFileReport(void){
    pspDiagFileWrite("PSP_TEXPERF hits=%lu misses=%lu binds=%lu evictions=%lu decodes=%lu decodeUs=%llu copyUs=%llu cacheBytes=%lu\\n",
        g_texHits,g_texMisses,g_texBinds,g_texEvictions,g_pageDecodes,g_pageDecodeUs,g_texCopyUs,(unsigned long)g_texCacheBytes);
    pspDiagFileWrite("PSP_FRAME frames=%lu logicUs=%llu drawUs=%llu syncUs=%llu\\n",
        g_frameCount,g_frameLogicUs,g_frameDrawUs,g_frameSyncUs);
    g_texHits=g_texMisses=g_texBinds=g_texEvictions=g_pageDecodes=0;
    g_pageDecodeUs=g_texCopyUs=0;
    g_frameCount=0;
    g_frameLogicUs=g_frameDrawUs=g_frameSyncUs=0;
}
static int nextPow2(int v){int n=1;while(n<v&&n<PSP_TEX_MAX)n<<=1;return n;}

/*
 * The GE reads texture memory asynchronously. The old code used
 * sceKernelDcacheWritebackInvalidateAll() for every newly-created cached
 * texture. That invalidates the PSP's entire 32 KiB D-cache and is wildly
 * expensive when a room introduces many small sprite/glyph textures.
 *
 * Only the texture buffer we just filled needs to be made visible to the GE.
 * PSP cache lines are 64 bytes, so align the range before issuing the
 * writeback. We deliberately do not invalidate the CPU cache here: the CPU
 * does not need to reread this buffer immediately and the GE only needs the
 * dirty data written to RAM.
 */
static void pspTextureWriteback(const void *ptr, size_t size){
    if(!ptr||size==0)return;
    uintptr_t start=(uintptr_t)ptr;
    uintptr_t end=start+size;
    start&=~(uintptr_t)63;
    end=(end+63)&~(uintptr_t)63;
    if(end>start){
        sceKernelDcacheWritebackRange((const void*)start,(unsigned int)(end-start));
    }
}

static void textureCacheDestroy(void){
    for(int i=0;i<PSP_TEX_CACHE_ENTRIES;i++){free(g_texCache[i].pixels);memset(&g_texCache[i],0,sizeof(g_texCache[i]));}
    g_texCacheBytes=0;
}
static void cacheClear(void){
    // Drop only the decoded source page. Cached GU texture copies are independent
    // buffers and must survive page switches while display lists may still reference them.
    free(g_cachedPixels);g_cachedPixels=NULL;g_cachedPixelsSize=0;g_cachedPage=-1;g_cachedW=g_cachedH=0;
}
static void pspTextureCacheFrameStart(void){
    g_frameStartUs=sceKernelGetSystemTimeWide();
    g_frameCount++;
    g_boundTexture=NULL; g_boundTw=g_boundTh=0;
    /* Do not expire texture entries by age. Re-decoding/re-uploading an
       otherwise valid sprite is extremely expensive on the PSP and can cause
       visible stalls/flicker. Entries are evicted only under actual capacity
       pressure by pspAllocTexture(). */ 
    g_texCacheFrame++;
}
static PSPTextureCacheEntry* pspFindTexture(int pageId,int sx,int sy,int sw,int sh){
    for(int i=0;i<PSP_TEX_CACHE_ENTRIES;i++){
        PSPTextureCacheEntry *e=&g_texCache[i];
        if(e->valid&&e->pageId==pageId&&e->sx==sx&&e->sy==sy&&e->sw==sw&&e->sh==sh){
            g_texHits++;
            e->lastUse=++g_texCacheClock;
            e->lastFrame=g_texCacheFrame;
            return e;
        }
    }
    return NULL;
}
static PSPTextureCacheEntry* pspAllocTexture(int pageId,int sx,int sy,int sw,int sh,int tw,int th){
    size_t bytes=(size_t)tw*th*2;
    if(bytes>PSP_TEX_CACHE_BYTES)return NULL;
    while(g_texCacheBytes+bytes>PSP_TEX_CACHE_BYTES){
        int victim=-1; unsigned long oldest=~0UL;
        for(int i=0;i<PSP_TEX_CACHE_ENTRIES;i++){
            PSPTextureCacheEntry *e=&g_texCache[i];
            if(e->valid && e->lastFrame<g_texCacheFrame && e->lastUse<oldest){ victim=i; oldest=e->lastUse; }
        }
        if(victim<0)return NULL;
        g_texCacheBytes-=g_texCache[victim].bytes;
        g_texEvictions++;
        free(g_texCache[victim].pixels);
        memset(&g_texCache[victim],0,sizeof(g_texCache[victim]));
    }
    for(int i=0;i<PSP_TEX_CACHE_ENTRIES;i++){
        if(!g_texCache[i].valid){
            PSPTextureCacheEntry *e=&g_texCache[i];
            e->pixels=(uint8_t*)calloc(1,bytes);
            if(!e->pixels)return NULL;
            e->valid=1;e->initialized=0;e->pageId=pageId;e->sx=sx;e->sy=sy;e->sw=sw;e->sh=sh;e->tw=tw;e->th=th;e->bytes=bytes;e->lastUse=++g_texCacheClock;e->lastFrame=g_texCacheFrame;
            g_texCacheBytes+=bytes;
            return e;
        }
    }
    return NULL;
}
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
 uint64_t decodeStart=sceKernelGetSystemTimeWide();
 uint8_t *p=ImageDecoder_decodeToRgba(tex->blobData,tex->blobSize,modern,&w,&h);
 g_pageDecodeUs += sceKernelGetSystemTimeWide()-decodeStart;
 g_pageDecodes++;
 if(!p||w<=0||h<=0){free(p);return false;} g_cachedPixels=p;g_cachedPixelsSize=(size_t)w*h*4;g_cachedPage=pageId;g_cachedW=w;g_cachedH=h;return true;
}
static bool uploadRect(DataWin *dw,int pageId,int sx,int sy,int sw,int sh,int *twOut,int *thOut,const void **pixelsOut){
 if(sw<=0||sh<=0||sw>PSP_TEX_MAX||sh>PSP_TEX_MAX){g_uploadFails++;g_uploadFailSize++;logWarn("PSP_DIAG SIZE page=%d sw=%d sh=%d\\n",pageId,sw,sh);return false;}
 if(!loadPage(dw,pageId)){g_uploadFails++;g_uploadFailLoad++;logWarn("PSP_DIAG LOAD page=%d\\n",pageId);return false;}
 if(sx<0||sy<0||sx+sw>g_cachedW||sy+sh>g_cachedH){g_uploadFails++;g_uploadFailBounds++;logWarn("PSP_DIAG BOUNDS page=%d sx=%d sy=%d sw=%d sh=%d cached=%dx%d\\n",pageId,sx,sy,sw,sh,g_cachedW,g_cachedH);return false;}
 int tw=nextPow2(sw),th=nextPow2(sh);if(tw>PSP_TEX_MAX||th>PSP_TEX_MAX){g_uploadFails++;g_uploadFailPow2++;logWarn("PSP_DIAG POW2 page=%d tw=%d th=%d\\n",pageId,tw,th);return false;}
 PSPTextureCacheEntry *e=pspFindTexture(pageId,sx,sy,sw,sh);
 if(!e){ g_texMisses++; e=pspAllocTexture(pageId,sx,sy,sw,sh,tw,th); }
 if(!e){g_uploadFails++;g_uploadFailLoad++;logWarn("PSP_DIAG CACHE_FULL page=%d sw=%d sh=%d\\n",pageId,sw,sh);return false;}
 if(!e->initialized){
     uint64_t copyStart=sceKernelGetSystemTimeWide();
     for(int y=0;y<sh;y++){
         uint16_t *dst=(uint16_t*)(e->pixels+(size_t)y*tw*2);
         const uint8_t *src=g_cachedPixels+((size_t)(sy+y)*g_cachedW+sx)*4;
         for(int x=0;x<sw;x++){
             uint8_t r=src[x*4+0], g=src[x*4+1], b=src[x*4+2], a=src[x*4+3];
             dst[x]=(uint16_t)(((r>>4)<<12)|((g>>4)<<8)|((b>>4)<<4)|(a>>4));
         }
     }
     g_texCopyUs += sceKernelGetSystemTimeWide()-copyStart;
     e->initialized=1;
     pspTextureWriteback(e->pixels,e->bytes);
 }
 if(g_boundTexture!=e || g_boundTw!=tw || g_boundTh!=th){
     g_texBinds++;
     /*
      * sceGuTexImage() already flushes the PSP texture page-cache. The
      * explicit sceGuTexFlush() that used to follow it was redundant.
      * Texture mode/function/filter are constant for this renderer and are
      * configured once when the GU starts.
      */
     sceGuTexImage(0,tw,th,tw,e->pixels);
     g_boundTexture=e; g_boundTw=tw; g_boundTh=th;
 }
 *twOut=tw;*thOut=th;*pixelsOut=e->pixels;return true;
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
    pspTextureCacheFrameStart();
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
    /*
     * All PSP texture uploads in this backend are RGBA8888, modulated by
     * the vertex colour, and point-filtered. Keep these states outside the
     * per-texture bind path; sceGuTexImage() is the only state that changes
     * from one cached texture to another.
     */
    sceGuTexMode(PSP_TEX_PSM,0,0,GU_FALSE);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST,GU_NEAREST);
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
    textureCacheDestroy();
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
    pspTextureCacheFrameStart();
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(GU_RGBA(0,0,0,255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    float sx=(gameW>0)?((float)PSP_W/(float)gameW):1.0f;
    float sy=(gameH>0)?((float)PSP_H/(float)gameH):1.0f;
    float scale=(sx<sy)?sx:sy;
    int fitW=(int)floorf((float)gameW*scale+0.5f);
    int fitH=(int)floorf((float)gameH*scale+0.5f);
    int fitX=(PSP_W-fitW)/2, fitY=(PSP_H-fitH)/2;
    setViewTransform(0,0,(float)gameW,(float)gameH,fitX,fitY,fitW,fitH);
    renderer->CPortX=0; renderer->CPortY=0; renderer->CPortW=PSP_W; renderer->CPortH=PSP_H;
}
static void pspEndFrameInit(Renderer *renderer){(void)renderer;}
static void pspEndFrameEnd(Renderer *renderer){(void)renderer;pspDiagReport();}
static void pspBeginView(Renderer *renderer,int32_t viewX,int32_t viewY,int32_t viewW,int32_t viewH,int32_t portX,int32_t portY,int32_t portW,int32_t portH,float viewAngle){
    (void)viewAngle;
    renderer->CPortX=portX;renderer->CPortY=portY;renderer->CPortW=portW;renderer->CPortH=portH;
    // portW/portH are GameMaker logical viewport dimensions (Undertale uses
    // 640x480 for a 320x240 view). They are not the physical PSP framebuffer.
    // Mapping them directly to fitX/fitY shifts the entire view to (160,120)
    // on a 480x272 screen, leaving only the lower-right portion visible.
    // Fit the logical camera against the actual PSP framebuffer instead.
    (void)portX; (void)portY; (void)portW; (void)portH;
    float sx=(viewW>0)?((float)PSP_W/(float)viewW):1.0f;
    float sy=(viewH>0)?((float)PSP_H/(float)viewH):1.0f;
    float scale=(sx<sy)?sx:sy;
    int fitW=(int)floorf((float)viewW*scale+0.5f);
    int fitH=(int)floorf((float)viewH*scale+0.5f);
    int fitX=(PSP_W-fitW)/2;
    int fitY=(PSP_H-fitH)/2;
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


static void pspDrawTextColor(Renderer *renderer,const char *text,float x,float y,float xscale,float yscale,float angleDeg,int32_t c1,int32_t c2,int32_t c3,int32_t c4,float alpha,float lineSeparation){
    if(!renderer||!renderer->dataWin||!text||renderer->drawFont<0||(uint32_t)renderer->drawFont>=renderer->dataWin->font.count)return;
    DataWin *dw=renderer->dataWin;
    Font *font=&dw->font.fonts[renderer->drawFont];
    if(font->tpagIndex<0||(uint32_t)font->tpagIndex>=dw->tpag.count)return;
    int len=(int)strlen(text); if(len<=0)return;
    int lines=TextUtils_countLines(text,len);
    float stride=(lineSeparation<0.0f)?TextUtils_lineStride(font):(lineSeparation/(font->scaleY!=0.0f?font->scaleY:1.0f));
    float valign=0.0f,total=(float)lines*stride;
    if(renderer->drawValign==1)valign=-total/2.0f;
    else if(renderer->drawValign==2)valign=-total;
    float angle=-angleDeg*(float)M_PI/180.0f,ca=cosf(angle),sa=sinf(angle);
    int lineStart=0;
    while(lineStart<=len){
        int lineEnd=lineStart;
        while(lineEnd<len&&!TextUtils_isNewlineChar(text[lineEnd]))lineEnd++;
        int lineLen=lineEnd-lineStart;
        float lineWidth=TextUtils_measureLineWidth(font,text+lineStart,lineLen);
        float halign=0.0f;
        if(renderer->drawHalign==1)halign=-lineWidth/2.0f;
        else if(renderer->drawHalign==2)halign=-lineWidth;
        float cursorX=halign;
        float cursorY=valign-(float)font->ascenderOffset+(float)(lineStart==0?0:0);
        int previousLine=0;
        for(int ls=0; ls<lineStart; ls++){ if(TextUtils_isNewlineChar(text[ls])) previousLine++; }
        cursorY+=(float)previousLine*stride;
        int32_t pos=0; uint16_t ch=0; bool hasCh=false;
        if(lineLen>0){ch=TextUtils_decodeUtf8(text+lineStart,lineLen,&pos);hasCh=true;}
        while(hasCh){
            FontGlyph *glyph=TextUtils_findGlyph(font,ch);
            uint16_t next=0; bool hasNext=lineLen>pos;
            if(hasNext)next=TextUtils_decodeUtf8(text+lineStart,lineLen,&pos);
            if(glyph){
                if(glyph->sourceWidth>0&&glyph->sourceHeight>0){
                    TexturePageItem *tp=&dw->tpag.items[font->tpagIndex];
                    int sx=(int)tp->sourceX+(int)glyph->sourceX;
                    int sy=(int)tp->sourceY+(int)glyph->sourceY;
                    int tw=0,th=0; const void *pixels=NULL;
                    if(uploadRect(dw,tp->texturePageId,sx,sy,glyph->sourceWidth,glyph->sourceHeight,&tw,&th,&pixels)){
                        (void)pixels;
                        float lx=cursorX+(float)glyph->offset;
                        float ly=cursorY;
                        float w=(float)glyph->sourceWidth*xscale*font->scaleX;
                        float h=(float)glyph->sourceHeight*yscale*font->scaleY;
                        float px=x+lx*xscale*font->scaleX, py=y+ly*yscale*font->scaleY;
                        float qx0=px,qy0=py,qx1=px+w,qy1=py,qx2=px+w,qy2=py+h,qx3=px,qy3=py+h;
                        if(angleDeg!=0.0f){
                            float ax=x,ay=y;
                            float dx[4]={qx0-ax,qx1-ax,qx2-ax,qx3-ax},dy[4]={qy0-ay,qy1-ay,qy2-ay,qy3-ay};
                            qx0=ax+ca*dx[0]-sa*dy[0]; qy0=ay+sa*dx[0]+ca*dy[0];
                            qx1=ax+ca*dx[1]-sa*dy[1]; qy1=ay+sa*dx[1]+ca*dy[1];
                            qx2=ax+ca*dx[2]-sa*dy[2]; qy2=ay+sa*dx[2]+ca*dy[2];
                            qx3=ax+ca*dx[3]-sa*dy[3]; qy3=ay+sa*dx[3]+ca*dy[3];
                        }
                        uint32_t cc0=bgrToGu((uint32_t)c1,alpha),cc1=bgrToGu((uint32_t)c2,alpha),cc2=bgrToGu((uint32_t)c3,alpha),cc3=bgrToGu((uint32_t)c4,alpha);
                        drawQuad(qx0,qy0,qx1,qy1,qx2,qy2,qx3,qy3,0,0,(float)glyph->sourceWidth,(float)glyph->sourceHeight,cc0,cc1,cc2,cc3);
                    }
                }
                cursorX+=(float)glyph->shift;
                if(hasNext)cursorX+=TextUtils_getKerningOffset(glyph,next);
            }
            ch=next;hasCh=hasNext;
        }
        if(lineEnd>=len)break;
        lineStart=TextUtils_skipNewline(text,lineEnd,len);
    }
}
static void pspDrawText(Renderer *renderer,const char *text,float x,float y,float xscale,float yscale,float angleDeg,float lineSeparation){
    uint32_t c=renderer?renderer->drawColor:0xFFFFFF;
    float a=renderer?renderer->drawAlpha:1.0f;
    pspDrawTextColor(renderer,text,x,y,xscale,yscale,angleDeg,(int32_t)c,(int32_t)c,(int32_t)c,(int32_t)c,a,lineSeparation);
}
static void pspDrawSpritePartColor(Renderer *renderer,int32_t tpagIndex,int32_t srcOffX,int32_t srcOffY,int32_t srcW,int32_t srcH,float x,float y,float xscale,float yscale,float angleDeg,float pivotX,float pivotY,uint32_t color1,uint32_t color2,uint32_t color3,uint32_t color4,float alpha){
    // The PSP GU texture dimensions are capped at 512x512. Split larger GameMaker
    // source rectangles into native-size pieces instead of dropping them entirely.
    if(srcW>PSP_TEX_MAX || srcH>PSP_TEX_MAX){
        for(int cy=0;cy<srcH;cy+=PSP_TEX_MAX){
            int ch=(srcH-cy>PSP_TEX_MAX)?PSP_TEX_MAX:srcH-cy;
            for(int cx=0;cx<srcW;cx+=PSP_TEX_MAX){
                int cw=(srcW-cx>PSP_TEX_MAX)?PSP_TEX_MAX:srcW-cx;
                pspDrawSpritePartColor(renderer,tpagIndex,srcOffX+cx,srcOffY+cy,cw,ch,
                    x+cx*xscale,y+cy*yscale,xscale,yscale,angleDeg,pivotX,pivotY,
                    color1,color2,color3,color4,alpha);
            }
        }
        return;
    }
    g_drawCalls++;
    if(g_drawStartUs==0) g_drawStartUs=sceKernelGetSystemTimeWide();
    DataWin *dw=renderer->dataWin;
    if(!dw||tpagIndex<0||(uint32_t)tpagIndex>=dw->tpag.count)return;
    TexturePageItem *tpag=&dw->tpag.items[tpagIndex];
    int sx=(int)tpag->sourceX+srcOffX, sy=(int)tpag->sourceY+srcOffY, tw,th;
    const void *texPixels=NULL; if(!uploadRect(dw,tpag->texturePageId,sx,sy,srcW,srcH,&tw,&th,&texPixels))return;
    (void)texPixels; float qx[4]={x,x+srcW*xscale,x+srcW*xscale,x}, qy[4]={y,y,y+srcH*yscale,y+srcH*yscale};
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
    g_pspVtable->drawSprite=pspDrawSprite;g_pspVtable->drawSpritePart=pspDrawSpritePart;g_pspVtable->drawText=pspDrawText;g_pspVtable->drawTextColor=pspDrawTextColor;g_pspVtable->drawTextUI=pspDrawTextColor;
    g_pspVtable->drawSpritePartColor=pspDrawSpritePartColor;g_pspVtable->drawSpriteTiled=pspDrawSpriteTiled;g_pspVtable->drawTiledPart=pspDrawTiledPart;g_pspVtable->drawRectangle=pspDrawRectangle;
    g_pspVtable->clearScreen=pspClearScreen;
    renderer->vtable=g_pspVtable; return renderer;
}