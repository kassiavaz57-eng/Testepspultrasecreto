#include "ps1_texture_pages.h"
#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "utils.h"
#include <stdlib.h>

#define PS1_CLUT_Y4 496
#define PS1_CLUT_Y8 480
#define PS1_CLUT4_SLOTS 64
#define PS1_CLUT8_SLOTS 64
#define CLUT4_BYTES 64
#define CLUT8_BYTES 1024

static uint16_t readU16(FILE* f) {
    uint8_t b[2] = {0, 0};
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

static uint32_t readU32(FILE* f) {
    uint8_t b[4] = {0, 0, 0, 0};
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static bool parseAtlas(Ps1TexturePages* cache, FILE* f) {
    uint8_t version;
    uint16_t tileCount;

    if (fread(&version, 1, 1, f) != 1 || version != 0) return false;

    cache->tpagCount = readU16(f);
    tileCount = readU16(f);
    cache->atlasCount = readU16(f);

    cache->tpag = (Ps1PageTPAG*)safeMalloc(
        (size_t)cache->tpagCount * sizeof(Ps1PageTPAG));
    cache->atlases = (Ps1PageAtlas*)safeMalloc(
        (size_t)cache->atlasCount * sizeof(Ps1PageAtlas));

    for (uint16_t i = 0; i < cache->atlasCount; ++i) {
        Ps1PageAtlas* a = &cache->atlases[i];
        a->dataOffset = readU32(f);
        a->width = readU16(f);
        a->height = readU16(f);
        if (fread(&a->bpp, 1, 1, f) != 1) return false;
        a->dataSize = readU32(f);
        if (fread(&a->compression, 1, 1, f) != 1) return false;

        if ((a->bpp != 4 && a->bpp != 8) ||
            a->width == 0 || a->height == 0 ||
            a->width > 1024 || a->height > 1024) {
            return false;
        }
    }

    for (uint16_t i = 0; i < cache->tpagCount; ++i) {
        Ps1PageTPAG* e = &cache->tpag[i];
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

    /* AtlasTileEntry is 15 uint16 fields in the official format. We do not
     * need the records to resolve sprite TPAGs yet, but must advance exactly
     * over them so the parser remains synchronized. */
    if (tileCount != 0 && fseek(f, (long)tileCount * 30L, SEEK_CUR) != 0)
        return false;

    return true;
}

static uint16_t rgbaToPs1(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint16_t pr, pg, pb, stp;

    if (a == 0 && r == 0 && g == 0 && b == 0)
        return 0;

    pr = (uint16_t)(r >> 3);
    pg = (uint16_t)(g >> 3);
    pb = (uint16_t)(b >> 3);
    stp = (a < 255) ? 0x8000u : 0;

    /* Keep opaque black distinguishable from the transparent zero entry. */
    if (pr == 0 && pg == 0 && pb == 0 && stp == 0)
        pr = 1;

    return (uint16_t)(pr | (pg << 5) | (pb << 10) | stp);
}

static bool loadClutFile(Ps1TexturePages* cache, const char* path, uint8_t bpp) {
    char* devicePath = PS1Utils_createDevicePath(path);
    FILE* f = fopen(devicePath, "rb");
    uint32_t entryBytes = (bpp == 4) ? CLUT4_BYTES : CLUT8_BYTES;
    uint32_t capacity = (bpp == 4) ? PS1_CLUT4_SLOTS : PS1_CLUT8_SLOTS;
    Ps1PageClut* slots = (bpp == 4) ? cache->clut4 : cache->clut8;
    long fileSize;

    free(devicePath);
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    fileSize = ftell(f);
    if (fileSize < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    uint32_t count = (uint32_t)fileSize / entryBytes;
    if (count > capacity) {
        /* A CLUT that cannot remain resident for a complete frame is not safe
         * to silently truncate: later TPAG indices would select wrong colors. */
        fclose(f);
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t raw[CLUT8_BYTES];
        uint16_t colors[256];
        uint32_t colorCount = (bpp == 4) ? 16u : 256u;

        if (fread(raw, 1, entryBytes, f) != entryBytes) {
            fclose(f);
            return false;
        }

        for (uint32_t c = 0; c < colorCount; ++c) {
            uint32_t o = c * 4u;
            colors[c] = rgbaToPs1(raw[o], raw[o + 1], raw[o + 2], raw[o + 3]);
        }

        RECT rect;
        rect.x = (bpp == 4) ? (int16_t)((i % 64u) * 16u)
                            : (int16_t)((i % 4u) * 256u);
        rect.y = (bpp == 4) ? PS1_CLUT_Y4 + (int16_t)(i / 64u)
                            : PS1_CLUT_Y8 + (int16_t)(i / 4u);
        rect.w = (bpp == 4) ? 16 : 256;
        rect.h = 1;

        LoadImage(&rect, (uint32_t*)colors);
        DrawSync(0);

        slots[i].valid = true;
        slots[i].index = (uint16_t)i;
        slots[i].x = (uint16_t)rect.x;
        slots[i].y = (uint16_t)rect.y;
        slots[i].lastUsed = cache->frameCounter;
    }

    fclose(f);
    return true;
}

static int findPage(Ps1TexturePages* cache, uint16_t atlasId,
                    uint16_t pageX, uint16_t pageY, uint8_t bpp) {
    for (int i = 0; i < PS1_TEX_PAGE_SLOTS; ++i) {
        Ps1PageSlot* s = &cache->pages[i];
        if (s->valid && s->atlasId == atlasId &&
            s->pageX == pageX && s->pageY == pageY && s->bpp == bpp)
            return i;
    }
    return -1;
}

static int choosePageSlot(Ps1TexturePages* cache) {
    int victim = -1;
    uint32_t oldest = 0xffffffffu;

    for (int i = 0; i < PS1_TEX_PAGE_SLOTS; ++i) {
        Ps1PageSlot* s = &cache->pages[i];
        if (!s->valid) return i;
        /* Pages touched during this frame are pinned. The renderer must batch
         * or submit work before requesting more than the available slots. */
        if (s->lastUsed == cache->frameCounter) continue;
        if (s->lastUsed < oldest) {
            oldest = s->lastUsed;
            victim = i;
        }
    }
    return victim;
}

static uint32_t atlasRowBytes(const Ps1PageAtlas* a) {
    if (a->bpp == 4)
        return ((uint32_t)a->width + 1u) / 2u;
    return (uint32_t)a->width;
}

static uint32_t pageByteWidth(const Ps1PageAtlas* a) {
    /* Both PS1 indexed modes occupy exactly 64 VRAM words per page row. */
    (void)a;
    return 128u;
}

static uint32_t atlasUncompressedSize(const Ps1PageAtlas* a) {
    return atlasRowBytes(a) * (uint32_t)a->height;
}

static bool decodePageRle(FILE* f, const Ps1PageAtlas* a,
                          uint16_t pageX, uint16_t pageY,
                          uint8_t* out) {
    uint32_t rowBytes = atlasRowBytes(a);
    uint32_t totalBytes = atlasUncompressedSize(a);
    uint32_t targetByteWidth = pageByteWidth(a);
    uint32_t pagePixelWidth = (a->bpp == 4) ? 256u : 128u;
    uint32_t pageStartRow = (uint32_t)pageY * 256u;
    uint32_t pageStartX = (uint32_t)pageX * pagePixelWidth;
    uint32_t consumed = 0;
    uint32_t produced = 0;

    (void)memset(out, 0, PS1_TEX_PAGE_BYTES);

    while (consumed + 1u < a->dataSize && produced < totalBytes) {
        uint8_t runLength;
        uint8_t value;
        uint32_t runStart;
        uint32_t runEnd;

        if (fread(&runLength, 1, 1, f) != 1 ||
            fread(&value, 1, 1, f) != 1)
            return false;
        consumed += 2u;

        if (runLength == 0) continue;

        runStart = produced;
        runEnd = produced + (uint32_t)runLength;
        if (runEnd > totalBytes) runEnd = totalBytes;

        while (runStart < runEnd) {
            uint32_t row = runStart / rowBytes;
            uint32_t rowEnd = (row + 1u) * rowBytes;
            uint32_t targetStart;
            uint32_t targetEnd;
            uint32_t copyStart;
            uint32_t copyEnd;

            if (row >= (uint32_t)a->height) break;
            if (rowEnd > totalBytes) rowEnd = totalBytes;

            targetStart = row * rowBytes + pageStartX / ((a->bpp == 4) ? 2u : 1u);
            targetEnd = targetStart + targetByteWidth;
            if (targetEnd > rowEnd) targetEnd = rowEnd;

            if (runStart < targetStart) {
                runStart = (runEnd < targetStart) ? runEnd : targetStart;
                continue;
            }
            if (runStart >= targetEnd) {
                runStart = rowEnd;
                continue;
            }

            copyStart = runStart;
            copyEnd = runEnd < targetEnd ? runEnd : targetEnd;

            if (row >= pageStartRow && row < pageStartRow + 256u) {
                uint32_t dstRow = row - pageStartRow;
                uint32_t dstCol = copyStart - targetStart;
                uint32_t count = copyEnd - copyStart;
                uint8_t* dst = out + dstRow * targetByteWidth + dstCol;
                for (uint32_t n = 0; n < count; ++n)
                    dst[n] = value;
            }
            runStart = copyEnd;
        }

        produced += (uint32_t)runLength;
        if (produced > totalBytes) produced = totalBytes;
    }

    return produced == totalBytes;
}

static bool decodePageRaw(FILE* f, const Ps1PageAtlas* a,
                          uint16_t pageX, uint16_t pageY,
                          uint8_t* out) {
    uint32_t rowBytes = atlasRowBytes(a);
    uint32_t pageWidth = pageByteWidth(a);
    uint32_t sourceXBytes = (a->bpp == 4) ? (uint32_t)pageX * 128u
                                          : (uint32_t)pageX * 128u;
    uint32_t firstRow = (uint32_t)pageY * 256u;

    (void)memset(out, 0, PS1_TEX_PAGE_BYTES);

    for (uint32_t y = 0; y < 256u; ++y) {
        uint32_t srcRow = firstRow + y;
        uint8_t* dst = out + y * pageWidth;
        uint32_t copyBytes;

        if (srcRow >= a->height || sourceXBytes >= rowBytes)
            continue;

        copyBytes = rowBytes - sourceXBytes;
        if (copyBytes > pageWidth) copyBytes = pageWidth;

        if (fseek(f, (long)(a->dataOffset + srcRow * rowBytes + sourceXBytes), SEEK_SET) != 0)
            return false;
        if (fread(dst, 1, copyBytes, f) != copyBytes)
            return false;
    }

    return true;
}

static bool uploadPage(Ps1TexturePages* cache, uint16_t atlasId,
                       uint16_t pageX, uint16_t pageY) {
    const Ps1PageAtlas* a;
    int slot;
    uint8_t pageData[PS1_TEX_PAGE_BYTES];
    uint16_t vramX;
    char* devicePath;

    if (atlasId >= cache->atlasCount) return false;
    a = &cache->atlases[atlasId];
    slot = choosePageSlot(cache);
    if (slot < 0) return false;

    devicePath = PS1Utils_createDevicePath("TEXTURES.BIN");
    if (cache->texturesFile == NULL)
        cache->texturesFile = fopen(devicePath, "rb");
    free(devicePath);
    if (cache->texturesFile == NULL) return false;

    if (a->compression == 1) {
        if (fseek(cache->texturesFile, (long)a->dataOffset, SEEK_SET) != 0)
            return false;
        if (!decodePageRle(cache->texturesFile, a, pageX, pageY, pageData))
            return false;
    } else {
        if (!decodePageRaw(cache->texturesFile, a, pageX, pageY, pageData))
            return false;
    }

    vramX = (uint16_t)(PS1_TEX_PAGE_VRAM_X + slot * PS1_TEX_PAGE_WORDS);
    RECT rect;
    rect.x = (int16_t)vramX;
    rect.y = PS1_TEX_PAGE_VRAM_Y;
    rect.w = PS1_TEX_PAGE_WORDS;
    rect.h = PS1_TEX_PAGE_HEIGHT;

    LoadImage(&rect, (uint32_t*)pageData);
    DrawSync(0);

    cache->pages[slot].valid = true;
    cache->pages[slot].atlasId = atlasId;
    cache->pages[slot].pageX = pageX;
    cache->pages[slot].pageY = pageY;
    cache->pages[slot].bpp = a->bpp;
    cache->pages[slot].vramX = vramX;
    cache->pages[slot].vramY = PS1_TEX_PAGE_VRAM_Y;
    cache->pages[slot].lastUsed = cache->frameCounter;
    return true;
}

static int ensurePage(Ps1TexturePages* cache, uint16_t atlasId,
                      uint16_t pageX, uint16_t pageY) {
    int found = findPage(cache, atlasId, pageX, pageY, cache->atlases[atlasId].bpp);
    if (found >= 0) {
        cache->pages[found].lastUsed = cache->frameCounter;
        return found;
    }
    if (!uploadPage(cache, atlasId, pageX, pageY)) return -1;
    return findPage(cache, atlasId, pageX, pageY, cache->atlases[atlasId].bpp);
}

static uint16_t getClut(Ps1TexturePages* cache, uint8_t bpp, uint16_t index) {
    Ps1PageClut* slots = (bpp == 4) ? cache->clut4 : cache->clut8;
    uint32_t count = (bpp == 4) ? PS1_CLUT4_SLOTS : PS1_CLUT8_SLOTS;
    for (uint32_t i = 0; i < count; ++i) {
        if (slots[i].valid && slots[i].index == index) {
            slots[i].lastUsed = cache->frameCounter;
            return getClut(slots[i].x, slots[i].y);
        }
    }
    return 0;
}

bool Ps1TexturePages_init(Ps1TexturePages* cache) {
    char* path;
    FILE* f;

    if (!cache) return false;
    (void)memset(cache, 0, sizeof(*cache));
    cache->frameCounter = 1;

    path = PS1Utils_createDevicePath("ATLAS.BIN");
    f = fopen(path, "rb");
    free(path);
    if (!f) return false;

    if (!parseAtlas(cache, f)) {
        fclose(f);
        Ps1TexturePages_shutdown(cache);
        return false;
    }
    fclose(f);

    if (!loadClutFile(cache, "CLUT4.BIN", 4) ||
        !loadClutFile(cache, "CLUT8.BIN", 8)) {
        Ps1TexturePages_shutdown(cache);
        return false;
    }

    return true;
}

void Ps1TexturePages_shutdown(Ps1TexturePages* cache) {
    if (!cache) return;
    if (cache->texturesFile) fclose(cache->texturesFile);
    free(cache->tpag);
    free(cache->atlases);
    (void)memset(cache, 0, sizeof(*cache));
}

void Ps1TexturePages_beginFrame(Ps1TexturePages* cache) {
    if (!cache) return;
    ++cache->frameCounter;
    if (cache->frameCounter == 0) cache->frameCounter = 1;
}

uint16_t Ps1TexturePages_resolveTPAG(
    Ps1TexturePages* cache,
    int32_t tpagIndex,
    Ps1TexturePagePiece outPieces[PS1_TEX_MAX_TPAG_PIECES]) {
    const Ps1PageTPAG* e;
    const Ps1PageAtlas* a;
    uint32_t pagePixelWidth;
    uint32_t startX, startY, endX, endY;
    uint16_t pieceCount = 0;

    if (!cache || !outPieces || tpagIndex < 0 ||
        (uint32_t)tpagIndex >= cache->tpagCount)
        return 0;

    e = &cache->tpag[tpagIndex];
    if (e->atlasId == 0xffffu || e->atlasId >= cache->atlasCount)
        return 0;

    a = &cache->atlases[e->atlasId];
    pagePixelWidth = (a->bpp == 4) ? 256u : 128u;

    startX = e->atlasX;
    startY = e->atlasY;
    endX = startX + e->width;
    endY = startY + e->height;

    for (uint32_t py = startY / 256u; py <= (endY - 1u) / 256u; ++py) {
        for (uint32_t px = startX / pagePixelWidth;
             px <= (endX - 1u) / pagePixelWidth; ++px) {
            uint32_t pageLeft = px * pagePixelWidth;
            uint32_t pageTop = py * 256u;
            uint32_t left = startX > pageLeft ? startX : pageLeft;
            uint32_t top = startY > pageTop ? startY : pageTop;
            uint32_t right = endX < pageLeft + pagePixelWidth ? endX : pageLeft + pagePixelWidth;
            uint32_t bottom = endY < pageTop + 256u ? endY : pageTop + 256u;
            int slot;

            if (right <= left || bottom <= top) continue;
            if (pieceCount >= PS1_TEX_MAX_TPAG_PIECES) return 0;

            slot = ensurePage(cache, e->atlasId, (uint16_t)px, (uint16_t)py);
            if (slot < 0) return 0;

            outPieces[pieceCount].x = (int16_t)(left - startX);
            outPieces[pieceCount].y = (int16_t)(top - startY);
            outPieces[pieceCount].width = (uint16_t)(right - left);
            outPieces[pieceCount].height = (uint16_t)(bottom - top);
            outPieces[pieceCount].u = (uint16_t)(left - pageLeft);
            outPieces[pieceCount].v = (uint16_t)(top - pageTop);
            outPieces[pieceCount].tpage = getTPage(
                a->bpp == 4 ? 0 : 1, 0,
                cache->pages[slot].vramX,
                cache->pages[slot].vramY);
            outPieces[pieceCount].clut = getClut(cache, a->bpp, e->clutIndex);
            if (outPieces[pieceCount].clut == 0 && e->clutIndex != 0)
                return 0;

            ++pieceCount;
        }
    }

    return pieceCount;
}
