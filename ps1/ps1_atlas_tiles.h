#ifndef BS_PS1_ATLAS_TILES_H
#define BS_PS1_ATLAS_TILES_H

#include <stdbool.h>
#include <stdint.h>

#include "../upstream/data_win.h"

typedef struct {
    int16_t bgDef;
    uint16_t srcX;
    uint16_t srcY;
    uint16_t srcW;
    uint16_t srcH;
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
} Ps1AtlasTileEntry;

/* ATLAS.BIN contains 15 little-endian uint16 fields per tile entry. */
bool Ps1AtlasTiles_init(void);
void Ps1AtlasTiles_shutdown(void);
const Ps1AtlasTileEntry* Ps1AtlasTiles_find(int32_t bgDef,
                                             int32_t srcX, int32_t srcY,
                                             int32_t srcW, int32_t srcH);

#endif
