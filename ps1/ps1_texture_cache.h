#ifndef _BS_PS1_TEXTURE_CACHE_H_
#define _BS_PS1_TEXTURE_CACHE_H_

#include "common.h"
#include "stdio_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <psxgpu.h>

#define PS1_TEXTURE_MAX_SLOTS 16
#define PS1_CLUT4_CACHE_SLOTS 32
/* VRAM area x=320..1023, y=480..511 fits two 256-word CLUT8s per row. */
#define PS1_CLUT8_CACHE_SLOTS 64

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
} Ps1AtlasTPAGEntry;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint32_t dataSize;
    uint32_t dataOffset;
    uint8_t compression;
} Ps1AtlasInfo;

typedef struct {
    bool valid;
    uint16_t atlasId;
    int16_t x;
    int16_t y;
    uint16_t widthWords;
    uint16_t height;
    uint8_t bpp;
    uint32_t lastUsed;
} Ps1TextureSlot;

typedef struct {
    bool valid;
    uint16_t clutIndex;
    uint16_t x;
    uint16_t y;
    uint32_t lastUsed;
} Ps1ClutSlot;

typedef struct {
    Ps1AtlasTPAGEntry* tpag;
    uint16_t tpagCount;
    Ps1AtlasInfo* atlases;
    uint16_t atlasCount;
    FILE* texturesFile;
    Ps1TextureSlot textures[PS1_TEXTURE_MAX_SLOTS];
    Ps1ClutSlot clut4[PS1_CLUT4_CACHE_SLOTS];
    Ps1ClutSlot clut8[PS1_CLUT8_CACHE_SLOTS];
    uint32_t frameCounter;
} Ps1TextureCache;

bool Ps1TextureCache_init(Ps1TextureCache* cache);
void Ps1TextureCache_shutdown(Ps1TextureCache* cache);
uint16_t Ps1TextureCache_getTPage(Ps1TextureCache* cache, uint16_t atlasId);
uint16_t Ps1TextureCache_getClut(Ps1TextureCache* cache, uint8_t bpp, uint16_t clutIndex);
bool Ps1TextureCache_resolveTPAG(Ps1TextureCache* cache, int32_t tpagIndex, uint16_t* outTPage, uint16_t* outClut, int16_t* outU, int16_t* outV, uint16_t* outW, uint16_t* outH);

#endif /* _BS_PS1_TEXTURE_CACHE_H_ */
