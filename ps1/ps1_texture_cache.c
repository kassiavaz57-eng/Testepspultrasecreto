#include "ps1_texture_cache.h"
#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "utils.h"
#include <psxgpu.h>
#include <stdlib.h>

#define PS1_TEXTURE_BASE_X 320
#define PS1_TEXTURE_BASE_Y 0
#define PS1_CLUT_BASE_X 320
#define PS1_CLUT_Y 240
#define PS1_VRAM_WIDTH_WORDS 1024
#define CLUT4_BYTES 64
#define CLUT8_BYTES 1024

typedef struct {
    uint32_t offset;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint32_t dataSize;
    uint8_t compression;
} AtlasDiskEntry;

static uint16_t readU16(FILE* f) {
    uint8_t b[2] = {0, 0};
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}
static uint32_t readU32(FILE* f) {
    uint8_t b[4] = {0, 0, 0, 0};
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static bool readAtlasHeader(Ps1TextureCache* cache, FILE* f) {
    uint8_t version;
    if (fread(&version, 1, 1, f) != 1 || version != 0) return false;
    uint16_t tpagCount = readU16(f);
    uint16_t tileCount = readU16(f);
    uint16_t atlasCount = readU16(f);
    (void)tileCount;
    cache->tpagCount = tpagCount;
    cache->atlasCount = atlasCount;
    cache->tpag = (Ps1AtlasTPAGEntry*)safeMalloc(sizeof(Ps1AtlasTPAGEntry) * tpagCount);
    cache->atlases = (Ps1AtlasInfo*)safeMalloc(sizeof(Ps1AtlasInfo) * atlasCount);

    repeat(atlasCount, i) {
        Ps1AtlasInfo* a = &cache->atlases[i];
        a->dataOffset = readU32(f);
        a->width = readU16(f);
        a->height = readU16(f);
        if (fread(&a->bpp, 1, 1, f) != 1) return false;
        a->dataSize = readU32(f);
        if (fread(&a->compression, 1, 1, f) != 1) return false;
        if (a->bpp != 4 && a->bpp != 8) return false;
        if (a->width == 0 || a->height == 0 || a->width > 256 || a->height > 256) return false;
    }

    repeat(tpagCount, i) {
        Ps1AtlasTPAGEntry* e = &cache->tpag[i];
        e->atlasId = readU16(f);
        e->atlasX = readU16(f);
        e->atlasY = readU16(f);
        e->width = readU16(f);
        e->height = readU16(f);
        e->cropX = readU16(f);
        e->cropY = readU16(f);
        e->cropW = readU16(f);
        e->cropH = readU16(f);
        e->clutIndex = readU16(f);
    }
    return true;
}

static uint16_t rgbaToPs1(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 0 && r == 0 && g == 0 && b == 0) return 0;
    uint16_t pr = (uint16_t)(r >> 3);
    uint16_t pg = (uint16_t)(g >> 3);
    uint16_t pb = (uint16_t)(b >> 3);
    uint16_t stp = (a < 255) ? 0x8000u : 0;
    if (pr == 0 && pg == 0 && pb == 0 && stp == 0) pr = 1;
    return (uint16_t)(pr | (pg << 5) | (pb << 10) | stp);
}

static bool uploadClutFile(Ps1TextureCache* cache, const char* path, uint8_t bpp) {
    char* devicePath = PS1Utils_createDevicePath(path);
    FILE* f = fopen(devicePath, "rb");
    free(devicePath);
    if (!f) return false;
    uint32_t entryBytes = (bpp == 4) ? CLUT4_BYTES : CLUT8_BYTES;
    fseek(f, 0, SEEK_END);
    long endPos = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (endPos < 0) { fclose(f); return false; }
    uint32_t count = (uint32_t)endPos / entryBytes;
    Ps1ClutSlot* slots = (bpp == 4) ? cache->clut4 : cache->clut8;
    uint32_t capacity = (bpp == 4) ? PS1_CLUT4_CACHE_SLOTS : PS1_CLUT8_CACHE_SLOTS;
    uint32_t loadCount = count < capacity ? count : capacity;

    repeat(loadCount, i) {
        uint8_t raw[CLUT8_BYTES];
        uint16_t colors[256];
        uint32_t colorCount = (bpp == 4) ? 16 : 256;
        if (fread(raw, 1, entryBytes, f) != entryBytes) { fclose(f); return false; }
        repeat(colorCount, c) {
            uint32_t o = c * 4;
            colors[c] = rgbaToPs1(raw[o], raw[o + 1], raw[o + 2], raw[o + 3]);
        }
        uint16_t paletteWidth = (bpp == 4) ? 16 : 256;
        uint16_t x = (uint16_t)(PS1_CLUT_BASE_X + (i * paletteWidth) % (1024 - PS1_CLUT_BASE_X));
        uint16_t y = (uint16_t)(PS1_CLUT_Y + (i * paletteWidth) / (1024 - PS1_CLUT_BASE_X));
        RECT rect = {x, y, paletteWidth, 1};
        LoadImage(&rect, (uint32_t*)colors);
        DrawSync(0);
        slots[i].valid = true;
        slots[i].clutIndex = (uint16_t)i;
        slots[i].x = x;
        slots[i].y = y;
        slots[i].lastUsed = cache->frameCounter;
    }
    fclose(f);
    return true;
}

static void decompressAtlas(const uint8_t* src, uint32_t srcSize, uint8_t compression, uint32_t outSize, uint8_t* out) {
    if (compression == 1) {
        uint32_t s = 0, d = 0;
        while (s + 1 < srcSize && d < outSize) {
            uint8_t count = src[s++];
            uint8_t value = src[s++];
            repeat(count, j) {
                if (d >= outSize) break;
                out[d++] = value;
            }
        }
        while (d < outSize) out[d++] = 0;
    } else {
        uint32_t n = srcSize < outSize ? srcSize : outSize;
        memcpy(out, src, n);
        while (n < outSize) out[n++] = 0;
    }
}

static uint32_t atlasPixelBytes(uint16_t width, uint16_t height, uint8_t bpp) {
    return (bpp == 4) ? (((uint32_t)width * height) + 1) / 2 : (uint32_t)width * height;
}

static uint32_t packIndexedPixels(const uint8_t* indexed, uint16_t width, uint16_t height, uint8_t bpp, uint16_t* out) {
    uint32_t pixels = (uint32_t)width * height;
    uint32_t wordsPerRow = (bpp == 4) ? (((uint32_t)width + 3) / 4) : (((uint32_t)width + 1) / 2);
    uint32_t src = 0;
    repeat(height, y) {
        repeat(wordsPerRow, x) {
            uint16_t word = 0;
            if (bpp == 4) {
                uint8_t p0 = src < pixels ? indexed[src++] : 0;
                uint8_t p1 = src < pixels ? indexed[src++] : 0;
                uint8_t p2 = src < pixels ? indexed[src++] : 0;
                uint8_t p3 = src < pixels ? indexed[src++] : 0;
                word = (uint16_t)((p0 & 15) | ((p1 & 15) << 4) | ((p2 & 15) << 8) | ((p3 & 15) << 12));
            } else {
                uint8_t p0 = src < pixels ? indexed[src++] : 0;
                uint8_t p1 = src < pixels ? indexed[src++] : 0;
                word = (uint16_t)(p0 | ((uint16_t)p1 << 8));
            }
            out[y * wordsPerRow + x] = word;
        }
    }
    return wordsPerRow * height;
}

static int findFreeTextureSlot(Ps1TextureCache* cache) {
    repeat(PS1_TEXTURE_MAX_SLOTS, i) if (!cache->textures[i].valid) return (int)i;
    uint32_t oldest = 0xFFFFFFFFu;
    int victim = 0;
    repeat(PS1_TEXTURE_MAX_SLOTS, i) if (cache->textures[i].lastUsed < oldest) { oldest = cache->textures[i].lastUsed; victim = (int)i; }
    return victim;
}

static bool loadAtlas(Ps1TextureCache* cache, uint16_t atlasId) {
    if (atlasId >= cache->atlasCount) return false;
    Ps1AtlasInfo* info = &cache->atlases[atlasId];
    uint32_t indexedSize = atlasPixelBytes(info->width, info->height, info->bpp);
    uint16_t words = (uint16_t)((info->bpp == 4) ? (((uint32_t)info->width + 3) / 4) : (((uint32_t)info->width + 1) / 2));
    if (words > 256 || info->height > 256) return false;

    int slot = findFreeTextureSlot(cache);
    uint16_t baseX = 0;
    bool placed = false;
    repeat(11, candidateIndex) {
        uint16_t candidate = (uint16_t)(PS1_TEXTURE_BASE_X + candidateIndex * 64);
        if (candidate + words > PS1_VRAM_WIDTH_WORDS) continue;
        bool overlap = false;
        repeat(PS1_TEXTURE_MAX_SLOTS, j) {
            if (!cache->textures[j].valid || j == (uint32_t)slot) continue;
            uint16_t otherW = cache->textures[j].widthWords;
            if (!(candidate + words <= (uint16_t)cache->textures[j].x || (uint16_t)cache->textures[j].x + otherW <= candidate)) { overlap = true; break; }
        }
        if (!overlap) { baseX = candidate; placed = true; break; }
    }
    if (!placed) return false;

    char* devicePath = PS1Utils_createDevicePath("TEXTURES.BIN");
    FILE* f = fopen(devicePath, "rb");
    free(devicePath);
    if (!f) return false;
    if (fseek(f, (long)info->dataOffset, SEEK_SET) != 0) { fclose(f); return false; }

    uint8_t* compressed = (uint8_t*)safeMalloc(info->dataSize);
    uint8_t* indexed = (uint8_t*)safeMalloc(indexedSize);
    uint16_t* packed = (uint16_t*)safeMalloc((size_t)words * info->height * sizeof(uint16_t));
    size_t got = fread(compressed, 1, info->dataSize, f);
    fclose(f);
    if (got != info->dataSize) { free(compressed); free(indexed); free(packed); return false; }

    decompressAtlas(compressed, info->dataSize, info->compression, indexedSize, indexed);
    packIndexedPixels(indexed, info->width, info->height, info->bpp, packed);
    RECT rect = {baseX, PS1_TEXTURE_BASE_Y, words, (int16_t)info->height};
    LoadImage(&rect, (uint32_t*)packed);
    DrawSync(0);

    cache->textures[slot].valid = true;
    cache->textures[slot].atlasId = atlasId;
    cache->textures[slot].x = (int16_t)baseX;
    cache->textures[slot].y = PS1_TEXTURE_BASE_Y;
    cache->textures[slot].widthWords = words;
    cache->textures[slot].height = info->height;
    cache->textures[slot].bpp = info->bpp;
    cache->textures[slot].lastUsed = ++cache->frameCounter;

    free(compressed); free(indexed); free(packed);
    return true;
}

bool Ps1TextureCache_init(Ps1TextureCache* cache) {
    memset(cache, 0, sizeof(*cache));
    cache->frameCounter = 1;
    char* path = PS1Utils_createDevicePath("ATLAS.BIN");
    FILE* f = fopen(path, "rb");
    free(path);
    if (!f) return false;
    bool ok = readAtlasHeader(cache, f);
    fclose(f);
    if (!ok) return false;
    if (!uploadClutFile(cache, "CLUT4.BIN", 4)) return false;
    if (!uploadClutFile(cache, "CLUT8.BIN", 8)) return false;
    return true;
}

void Ps1TextureCache_shutdown(Ps1TextureCache* cache) {
    if (!cache) return;
    free(cache->tpag);
    free(cache->atlases);
    memset(cache, 0, sizeof(*cache));
}

uint16_t Ps1TextureCache_getTPage(Ps1TextureCache* cache, uint16_t atlasId) {
    if (!cache || atlasId >= cache->atlasCount) return 0;
    repeat(PS1_TEXTURE_MAX_SLOTS, i) {
        if (cache->textures[i].valid && cache->textures[i].atlasId == atlasId) {
            cache->textures[i].lastUsed = ++cache->frameCounter;
            return getTPage(cache->textures[i].bpp == 4 ? 0 : 1, 0, cache->textures[i].x, cache->textures[i].y);
        }
    }
    if (!loadAtlas(cache, atlasId)) return 0;
    repeat(PS1_TEXTURE_MAX_SLOTS, i) {
        if (cache->textures[i].valid && cache->textures[i].atlasId == atlasId)
            return getTPage(cache->textures[i].bpp == 4 ? 0 : 1, 0, cache->textures[i].x, cache->textures[i].y);
    }
    return 0;
}

uint16_t Ps1TextureCache_getClut(Ps1TextureCache* cache, uint8_t bpp, uint16_t clutIndex) {
    Ps1ClutSlot* slots = (bpp == 4) ? cache->clut4 : cache->clut8;
    uint32_t count = (bpp == 4) ? PS1_CLUT4_CACHE_SLOTS : PS1_CLUT8_CACHE_SLOTS;
    repeat(count, i) if (slots[i].valid && slots[i].clutIndex == clutIndex) {
        slots[i].lastUsed = ++cache->frameCounter;
        return getClut(slots[i].x, slots[i].y);
    }
    return 0;
}

bool Ps1TextureCache_resolveTPAG(Ps1TextureCache* cache, int32_t tpagIndex, uint16_t* outTPage, uint16_t* outClut, int16_t* outU, int16_t* outV, uint16_t* outW, uint16_t* outH) {
    if (!cache || tpagIndex < 0 || (uint32_t)tpagIndex >= cache->tpagCount) return false;
    Ps1AtlasTPAGEntry* e = &cache->tpag[tpagIndex];
    if (e->atlasId == 0xFFFFu || e->atlasId >= cache->atlasCount) return false;
    Ps1AtlasInfo* info = &cache->atlases[e->atlasId];
    uint16_t tpageBase = Ps1TextureCache_getTPage(cache, e->atlasId);
    uint16_t clut = Ps1TextureCache_getClut(cache, info->bpp, e->clutIndex);
    if (tpageBase == 0 || clut == 0) return false;

    uint16_t pagePixels = (info->bpp == 4) ? 256 : 128;
    uint16_t pageX = (uint16_t)((e->atlasX / pagePixels) * pagePixels);
    uint16_t baseX = 0;
    repeat(PS1_TEXTURE_MAX_SLOTS, i) if (cache->textures[i].valid && cache->textures[i].atlasId == e->atlasId) { baseX = (uint16_t)cache->textures[i].x; break; }
    uint16_t pageWordX = (info->bpp == 4) ? (uint16_t)(baseX + pageX / 4) : (uint16_t)(baseX + pageX / 2);
    uint16_t pageY = (uint16_t)(PS1_TEXTURE_BASE_Y + e->atlasY);
    *outTPage = getTPage(info->bpp == 4 ? 0 : 1, 0, pageWordX, pageY);
    *outClut = clut;
    *outU = (int16_t)(e->atlasX % pagePixels);
    *outV = (int16_t)e->atlasY;
    *outW = e->width;
    *outH = e->height;
    return true;
}
