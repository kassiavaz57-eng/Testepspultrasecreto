#ifndef _BS_PS1_TEXTURE_PAGES_H_
#define _BS_PS1_TEXTURE_PAGES_H_

#include "common.h"
#include "stdio_compat.h"
#include <psxgpu.h>
#include <stdbool.h>
#include <stdint.h>

#define PS1_TEX_PAGE_WORDS 64
#define PS1_TEX_PAGE_HEIGHT 256
#define PS1_TEX_PAGE_BYTES 32768
#define PS1_TEX_PAGE_SLOTS 11
#define PS1_TEX_PAGE_VRAM_X 320
#define PS1_TEX_PAGE_VRAM_Y 0
#define PS1_TEX_MAX_TPAG_PIECES 32

typedef struct { uint16_t atlasId, atlasX, atlasY, width, height; uint16_t cropX, cropY, cropW, cropH, clutIndex; } Ps1PageTPAG;
typedef struct { uint32_t dataOffset; uint16_t width, height; uint8_t bpp; uint32_t dataSize; uint8_t compression; } Ps1PageAtlas;
typedef struct { bool valid; uint16_t atlasId, pageX, pageY; uint8_t bpp; uint16_t vramX, vramY; uint32_t lastUsed; } Ps1PageSlot;
typedef struct { bool valid; uint16_t index, x, y; uint32_t lastUsed; } Ps1PageClut;

/*
 * One PS1 texture-page primitive worth of a TPAG.
 *
 * x/y/width/height identify the physical post-resize region inside the TPAG
 * atlas rectangle. u/v identify the corresponding local coordinates in the
 * PS1 VRAM texture page. crop* preserves the original pre-resize GameMaker
 * sprite mapping: cropX/Y are offsets inside the original bounding box and
 * cropW/H are the pre-resize dimensions of the content. atlasWidth/Height
 * are the post-crop/post-resize physical dimensions from ATLAS.BIN.
 *
 * Keeping both coordinate spaces here is intentional. The renderer must not
 * assume that one source pixel always equals one atlas texel: the official
 * preprocessor can resize the cropped content while packing the atlas.
 */
typedef struct {
    int16_t x, y;
    uint16_t width, height;
    uint16_t u, v, tpage, clut;
    uint16_t cropX, cropY, cropW, cropH;
    uint16_t atlasWidth, atlasHeight;
} Ps1TexturePagePiece;

typedef struct {
    Ps1PageTPAG* tpag;
    uint16_t tpagCount;
    Ps1PageAtlas* atlases;
    uint16_t atlasCount;
    Ps1PageSlot pages[PS1_TEX_PAGE_SLOTS];
    Ps1PageClut clut4[64];
    Ps1PageClut clut8[64];
    FILE* texturesFile;
    uint32_t frameCounter;
    uint8_t pageData[PS1_TEX_PAGE_BYTES];
} Ps1TexturePages;

bool Ps1TexturePages_init(Ps1TexturePages* cache);
void Ps1TexturePages_shutdown(Ps1TexturePages* cache);
void Ps1TexturePages_beginFrame(Ps1TexturePages* cache);
uint16_t Ps1TexturePages_resolveTPAG(Ps1TexturePages* cache, int32_t tpagIndex, Ps1TexturePagePiece outPieces[PS1_TEX_MAX_TPAG_PIECES]);

#endif
