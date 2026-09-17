#include "ps1_fast_renderer.h"
#include <math.h>

/* Fast room path for real GameMaker tiled sprites. The source rectangle is
   repeated from the sprite origin; each partial edge tile is clipped in
   source-space before entering the real PS1 renderer. */
static void ps1FastDrawSpriteTiled(Renderer* renderer, int32_t tpagIndex,
    float originX, float originY, float x, float y, float xscale, float yscale,
    bool tileX, bool tileY, float roomW, float roomH, uint32_t color, float alpha) {
    if (!renderer || !renderer->dataWin || !renderer->vtable || !renderer->vtable->drawSpritePart) return;
    if (tpagIndex < 0 || (uint32_t)tpagIndex >= renderer->dataWin->tpag.count) return;
    TexturePageItem* tpag = &renderer->dataWin->tpag.items[tpagIndex];
    int32_t sw = (int32_t)tpag->sourceWidth, sh = (int32_t)tpag->sourceHeight;
    if (sw <= 0 || sh <= 0 || xscale == 0.0f || yscale == 0.0f) return;

    float stepX = fabsf(sw * xscale), stepY = fabsf(sh * yscale);
    float maxW = tileX && roomW > 0 ? roomW : stepX;
    float maxH = tileY && roomH > 0 ? roomH : stepY;
    int nx = tileX ? (int)ceilf(maxW / stepX) : 1;
    int ny = tileY ? (int)ceilf(maxH / stepY) : 1;
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;
    if (nx > 128) nx = 128;
    if (ny > 128) ny = 128;

    /* GameMaker's x/y are the origin position, not the top-left of every
       repeated tile. Do the origin subtraction once, then use local source
       coordinates for every repeated piece. */
    const float startX = x - originX * xscale;
    const float startY = y - originY * yscale;

    for (int ty = 0; ty < ny; ++ty) {
        for (int tx = 0; tx < nx; ++tx) {
            float dx = startX + tx * stepX;
            float dy = startY + ty * stepY;
            int32_t cw = sw, ch = sh;
            float rw = maxW - tx * stepX, rh = maxH - ty * stepY;
            if (tileX && rw < stepX) cw = (int32_t)floorf(rw / fabsf(xscale));
            if (tileY && rh < stepY) ch = (int32_t)floorf(rh / fabsf(yscale));
            if (cw <= 0 || ch <= 0) continue;
            renderer->vtable->drawSpritePart(renderer, tpagIndex, 0, 0, cw, ch,
                dx, dy, xscale, yscale, 0.0f, 0.0f, 0.0f, color, alpha);
        }
    }
}

void Ps1FastRenderer_install(Renderer* renderer) {
    if (!renderer || !renderer->vtable) return;
    /* RoomTile stays in the core renderer: it has the dedicated ATLAS.BIN
       lookup and the real TPAG fallback. */
    renderer->vtable->drawSpriteTiled = ps1FastDrawSpriteTiled;
}
