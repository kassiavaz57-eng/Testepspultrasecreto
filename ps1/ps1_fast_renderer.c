#include "ps1_fast_renderer.h"
#include <math.h>

/* Fast room paths: keep the real DataWin/Runner pipeline and reuse the PS1
   TPAG renderer. Unsupported special cases simply fall back instead of
   corrupting the frame. */
static void ps1FastDrawTile(Renderer* renderer, RoomTile* tile, float offsetX, float offsetY) {
    if (!renderer || !tile || !renderer->dataWin || !renderer->vtable || !renderer->vtable->drawSpritePart) return;
    int32_t tpagIndex = Renderer_resolveObjectTPAGIndex(renderer->dataWin, tile);
    if (tpagIndex < 0 || (uint32_t)tpagIndex >= renderer->dataWin->tpag.count) return;
    TexturePageItem* tpag = &renderer->dataWin->tpag.items[tpagIndex];
    int32_t sx = tile->sourceX, sy = tile->sourceY;
    int32_t sw = (int32_t)tile->width, sh = (int32_t)tile->height;
    float dx = (float)tile->x + offsetX, dy = (float)tile->y + offsetY;
    if (tpag->targetX > sx) { int32_t n = tpag->targetX - sx; dx += n * tile->scaleX; sx += n; sw -= n; }
    if (tpag->targetY > sy) { int32_t n = tpag->targetY - sy; dy += n * tile->scaleY; sy += n; sh -= n; }
    int32_t right = tpag->targetX + tpag->sourceWidth;
    int32_t bottom = tpag->targetY + tpag->sourceHeight;
    if (sx + sw > right) sw = right - sx;
    if (sy + sh > bottom) sh = bottom - sy;
    if (sw <= 0 || sh <= 0) return;
    sx -= tpag->targetX; sy -= tpag->targetY;
    renderer->vtable->drawSpritePart(renderer, tpagIndex, sx, sy, sw, sh,
        dx, dy, tile->scaleX, tile->scaleY, 0.0f, 0.0f, 0.0f,
        tile->color & 0x00FFFFFFu, tile->alpha);
}

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
    if (nx < 1) nx = 1; if (ny < 1) ny = 1;
    if (nx > 128) nx = 128; if (ny > 128) ny = 128;
    for (int ty = 0; ty < ny; ++ty) for (int tx = 0; tx < nx; ++tx) {
        float dx = x + tx * stepX, dy = y + ty * stepY;
        int32_t cw = sw, ch = sh;
        float rw = maxW - tx * stepX, rh = maxH - ty * stepY;
        if (tileX && rw < stepX) cw = (int32_t)floorf(rw / fabsf(xscale));
        if (tileY && rh < stepY) ch = (int32_t)floorf(rh / fabsf(yscale));
        if (cw <= 0 || ch <= 0) continue;
        renderer->vtable->drawSpritePart(renderer, tpagIndex, 0, 0, cw, ch,
            dx, dy, xscale, yscale, 0.0f, originX, originY, color, alpha);
    }
}

void Ps1FastRenderer_install(Renderer* renderer) {
    if (!renderer || !renderer->vtable) return;
    renderer->vtable->drawTile = ps1FastDrawTile;
    renderer->vtable->drawSpriteTiled = ps1FastDrawSpriteTiled;
}
