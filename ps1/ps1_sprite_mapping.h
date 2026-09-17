#ifndef _BS_PS1_SPRITE_MAPPING_H_
#define _BS_PS1_SPRITE_MAPPING_H_

#include "ps1_texture_pages.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t srcX;
    int32_t srcY;
    int32_t srcW;
    int32_t srcH;
    int32_t atlasX;
    int32_t atlasY;
    int32_t atlasW;
    int32_t atlasH;
} Ps1SpriteMappedRect;

/*
 * Maps a requested rectangle in GameMaker/source sprite space onto the
 * physical post-crop/post-resize atlas rectangle represented by one PS1
 * texture-page piece.
 *
 * The TPAG crop rectangle is the source/game-space visible rectangle. The
 * atlas rectangle is allowed to have different dimensions because the
 * official preprocessor may resize content while packing it.
 */
bool Ps1SpriteMap_piece(const Ps1TexturePagePiece* piece,
                        int32_t requestX, int32_t requestY,
                        int32_t requestW, int32_t requestH,
                        Ps1SpriteMappedRect* out);

#endif
