#include "ps1_sprite_mapping.h"

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool Ps1SpriteMap_piece(const Ps1TexturePagePiece* piece,
                        int32_t requestX, int32_t requestY,
                        int32_t requestW, int32_t requestH,
                        Ps1SpriteMappedRect* out) {
    if (!piece || !out || requestW <= 0 || requestH <= 0 ||
        piece->width == 0 || piece->height == 0 ||
        piece->cropW == 0 || piece->cropH == 0 ||
        piece->atlasWidth == 0 || piece->atlasHeight == 0) {
        return false;
    }

    /*
     * Ps1TexturePages_resolveTPAG() stores piece->x/y in source/crop space:
     *   piece->x = cropX + (physicalAtlasX - atlasX)
     *   piece->y = cropY + (physicalAtlasY - atlasY)
     *
     * The physical atlas rectangle can be resized relative to cropW/cropH.
     * Therefore the source interval represented by this piece is NOT simply
     * [piece->x, piece->x + piece->width]. Recover the source interval using
     * the same source->atlas ratio used by the preprocessor.
     */
    const int32_t atlasOffX = (int32_t)piece->x - (int32_t)piece->cropX;
    const int32_t atlasOffY = (int32_t)piece->y - (int32_t)piece->cropY;

    int32_t pieceSrcX0 = (int32_t)piece->cropX +
        (atlasOffX * (int32_t)piece->cropW) / (int32_t)piece->atlasWidth;
    int32_t pieceSrcY0 = (int32_t)piece->cropY +
        (atlasOffY * (int32_t)piece->cropH) / (int32_t)piece->atlasHeight;
    int32_t pieceSrcX1 = (int32_t)piece->cropX +
        ((atlasOffX + (int32_t)piece->width) * (int32_t)piece->cropW) /
            (int32_t)piece->atlasWidth;
    int32_t pieceSrcY1 = (int32_t)piece->cropY +
        ((atlasOffY + (int32_t)piece->height) * (int32_t)piece->cropH) /
            (int32_t)piece->atlasHeight;

    pieceSrcX0 = clamp32(pieceSrcX0, (int32_t)piece->cropX,
                         (int32_t)piece->cropX + piece->cropW);
    pieceSrcY0 = clamp32(pieceSrcY0, (int32_t)piece->cropY,
                         (int32_t)piece->cropY + piece->cropH);
    pieceSrcX1 = clamp32(pieceSrcX1, pieceSrcX0,
                         (int32_t)piece->cropX + piece->cropW);
    pieceSrcY1 = clamp32(pieceSrcY1, pieceSrcY0,
                         (int32_t)piece->cropY + piece->cropH);

    int32_t requestX1 = requestX + requestW;
    int32_t requestY1 = requestY + requestH;
    int32_t srcX0 = requestX > pieceSrcX0 ? requestX : pieceSrcX0;
    int32_t srcY0 = requestY > pieceSrcY0 ? requestY : pieceSrcY0;
    int32_t srcX1 = requestX1 < pieceSrcX1 ? requestX1 : pieceSrcX1;
    int32_t srcY1 = requestY1 < pieceSrcY1 ? requestY1 : pieceSrcY1;

    if (srcX1 <= srcX0 || srcY1 <= srcY0) return false;

    /* Map the clipped source rectangle back into this physical atlas piece. */
    int32_t atlasX0 = (srcX0 - pieceSrcX0) * (int32_t)piece->width /
                      (pieceSrcX1 - pieceSrcX0);
    int32_t atlasY0 = (srcY0 - pieceSrcY0) * (int32_t)piece->height /
                      (pieceSrcY1 - pieceSrcY0);
    int32_t atlasX1 = (srcX1 - pieceSrcX0) * (int32_t)piece->width /
                      (pieceSrcX1 - pieceSrcX0);
    int32_t atlasY1 = (srcY1 - pieceSrcY0) * (int32_t)piece->height /
                      (pieceSrcY1 - pieceSrcY0);

    atlasX0 = clamp32(atlasX0, 0, piece->width);
    atlasY0 = clamp32(atlasY0, 0, piece->height);
    atlasX1 = clamp32(atlasX1, atlasX0, piece->width);
    atlasY1 = clamp32(atlasY1, atlasY0, piece->height);

    out->srcX = srcX0;
    out->srcY = srcY0;
    out->srcW = srcX1 - srcX0;
    out->srcH = srcY1 - srcY0;
    out->atlasX = atlasX0;
    out->atlasY = atlasY0;
    out->atlasW = atlasX1 - atlasX0;
    out->atlasH = atlasY1 - atlasY0;
    return out->atlasW > 0 && out->atlasH > 0;
}
