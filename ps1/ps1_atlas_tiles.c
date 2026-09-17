#include "ps1_atlas_tiles.h"

#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "utils.h"

#include <stdlib.h>

static Ps1AtlasTileEntry* gEntries;
static uint16_t gCount;

static uint16_t rd16(FILE* f, bool* ok) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) {
        *ok = false;
        return 0;
    }
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

bool Ps1AtlasTiles_init(void) {
    Ps1AtlasTiles_shutdown();

    char* path = PS1Utils_createDevicePath("ATLAS.BIN");
    FILE* f = fopen(path, "rb");
    free(path);
    if (!f) return false;

    uint8_t version = 0;
    bool ok = fread(&version, 1, 1, f) == 1;
    if (!ok || version != 0) {
        fclose(f);
        return false;
    }

    uint16_t tpagCount = rd16(f, &ok);
    uint16_t tileCount = rd16(f, &ok);
    uint16_t atlasCount = rd16(f, &ok);
    (void)tpagCount;
    (void)atlasCount;
    if (!ok) {
        fclose(f);
        return false;
    }

    /* Skip atlas table. Each entry is dataOffset:u32, width:u16,
       height:u16, bpp:u8, dataSize:u32, compression:u8. */
    if (fseek(f, (long)atlasCount * 16L, SEEK_CUR) != 0) {
        fclose(f);
        return false;
    }

    /* Skip TPAG table. Each entry is 10 uint16 fields. */
    if (fseek(f, (long)tpagCount * 20L, SEEK_CUR) != 0) {
        fclose(f);
        return false;
    }

    if (tileCount == 0) {
        fclose(f);
        return true;
    }

    gEntries = (Ps1AtlasTileEntry*)safeMalloc((size_t)tileCount * sizeof(*gEntries));
    if (!gEntries) {
        fclose(f);
        return false;
    }

    for (uint16_t i = 0; i < tileCount; ++i) {
        Ps1AtlasTileEntry* e = &gEntries[i];
        e->bgDef = (int16_t)rd16(f, &ok);
        e->srcX = rd16(f, &ok);
        e->srcY = rd16(f, &ok);
        e->srcW = rd16(f, &ok);
        e->srcH = rd16(f, &ok);
        e->atlasId = rd16(f, &ok);
        e->atlasX = rd16(f, &ok);
        e->atlasY = rd16(f, &ok);
        e->width = rd16(f, &ok);
        e->height = rd16(f, &ok);
        e->cropX = rd16(f, &ok);
        e->cropY = rd16(f, &ok);
        e->cropW = rd16(f, &ok);
        e->cropH = rd16(f, &ok);
        e->clutIndex = rd16(f, &ok);
        if (!ok) {
            free(gEntries);
            gEntries = NULL;
            gCount = 0;
            fclose(f);
            return false;
        }
    }

    gCount = tileCount;
    fclose(f);
    return true;
}

void Ps1AtlasTiles_shutdown(void) {
    free(gEntries);
    gEntries = NULL;
    gCount = 0;
}

const Ps1AtlasTileEntry* Ps1AtlasTiles_find(int32_t bgDef,
                                             int32_t srcX, int32_t srcY,
                                             int32_t srcW, int32_t srcH) {
    if (!gEntries) return NULL;
    for (uint16_t i = 0; i < gCount; ++i) {
        const Ps1AtlasTileEntry* e = &gEntries[i];
        if ((int32_t)e->bgDef == bgDef &&
            (int32_t)e->srcX == srcX &&
            (int32_t)e->srcY == srcY &&
            (int32_t)e->srcW == srcW &&
            (int32_t)e->srcH == srcH) {
            return e;
        }
    }
    return NULL;
}
