#ifndef _BS_PS1_TEXTURE_PAGES_H_
#define _BS_PS1_TEXTURE_PAGES_H_

#include "common.h"
#include "stdio_compat.h"
#include <psxgpu.h>
#include <stdbool.h>
#include <stdint.h>

/* One PS1 texture page occupies exactly 64 VRAM words by 256 lines.
 * In 4bpp that represents 256x256 logical pixels; in 8bpp it represents
 * 128x256 logical pixels. */
#define PS1_TEX_PAGE_WORDS 64
#define PS1_TEX_PAGE_HEIGHT 256
#define PS1_TEX_PAGE_BYTES 32768
#define PS1_TEX_PAGE_SLOTS 11
#define PS1_TEX_PAGE_VRAM_X 320
#define PS1_TEX_PAGE_VRAM_Y 0

#define PS1_TEX_MAX_TPAG_PIECES 4

typedef struct {
    uint16_t atlasId;
    uint16_t atlasX;
    uint16_t atlasY;
    uint16_t width;
    uint16_t height;
    uint16_t cropX;
    uint16_t cropY;
    uint16_t cropW;
    uint16_t cropH;
    uint16_t clutIndex;
} Ps1PageTPAG;

typedef struct {
    uint32_t dataOffset;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint32_t dataSize;
    uint8_t compression;
} Ps1PageAtlas;

typedef struct {
    bool valid;
    uint16_t atlasId;
    uint16_t pageX;
    uint16_t pageY;
    uint8_t bpp;
    uint16_t vramX;
    uint16_t vramY;
    uint32_t lastUsed;
} Ps1PageSlot;

typedef struct {
    bool valid;
    uint16_t index;
    uint16_t x;
    uint16_t y;
    uint32_t lastUsed;
} Ps1PageClut;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t u;
    uint16_t v;
    uint16_t tpage;
    uint16_t clut;
} Ps1TexturePagePiece;

typedef struct {
    Ps1PageTPAG* tpag;
    uint16_t tpagCount;
    Ps1PageAtlas* atlases;
    uint16_t atlasCount;
    Ps1PageSlot pages[PS1_TEX_PAGE_SLOTS];
    Ps1PageClut clut4[32];
    Ps1PageClut clut8[64];
    FILE* texturesFile;
    uint32_t frameCounter;
} Ps1TexturePages;

bool Ps1TexturePages_init(Ps1TexturePages* cache);
void Ps1TexturePages_shutdown(Ps1TexturePages* cache);
void Ps1TexturePages_beginFrame(Ps1TexturePages* cache);

/* Resolves one TPAG rectangle into up to four page-local pieces. A piece never
 * crosses a PS1 texture-page boundary, so its UV coordinates are directly
 * usable by POLY_FT4/SPRT. */
uint16_t Ps1TexturePages_resolveTPAG(
    Ps1TexturePages* cache,
    int32_t tpagIndex,
    Ps1TexturePagePiece outPieces[PS1_TEX_MAX_TPAG_PIECES]);

#endif
