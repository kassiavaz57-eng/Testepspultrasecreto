#include "ps1_fast_renderer.h"
#include <math.h>

/* Real GameMaker tiled-sprite path. Repetition is evaluated in room space;
   the origin may be outside the room, and only the visible source rectangle
   of each edge tile is submitted to the real renderer. */
static void ps1FastDrawSpriteTiled(Renderer* renderer, int32_t tpagIndex,
    float originX, float originY, float x, float y, float xscale, float yscale,
    bool tileX, bool tileY, float roomW, float roomH, uint32_t color, float alpha) {
    if (!renderer || !renderer->dataWin || !renderer->vtable || !renderer->vtable->drawSpritePart) return;
    if (tpagIndex < 0 || (uint32_t)tpagIndex >= renderer->dataWin->tpag.count) return;

    TexturePageItem* tpag = &renderer->dataWin->tpag.items[tpagIndex];
    int32_t sw = (int32_t)tpag->sourceWidth;
    int32_t sh = (int32_t)tpag->sourceHeight;
    if (sw <= 0 || sh <= 0 || xscale == 0.0f || yscale == 0.0f) return;

    const float stepX = fabsf((float)sw * xscale);
    const float stepY = fabsf((float)sh * yscale);
    if (stepX <= 0.0f || stepY <= 0.0f) return;

    const float startX = x - originX * xscale;
    const float startY = y - originY * yscale;

    int firstX = tileX && roomW > 0.0f ? (int)floorf(-startX / stepX) : 0;
    int lastX  = tileX && roomW > 0.0f ? (int)ceilf((roomW - startX) / stepX) - 1 : 0;
    int firstY = tileY && roomH > 0.0f ? (int)floorf(-startY / stepY) : 0;
    int lastY  = tileY && roomH > 0.0f ? (int)ceilf((roomH - startY) / stepY) - 1 : 0;

    if (lastX - firstX > 128) lastX = firstX + 128;
    if (lastY - firstY > 128) lastY = firstY + 128;

    for (int ty = firstY; ty <= lastY; ++ty) {
        for (int tx = firstX; tx <= lastX; ++tx) {
            float dx = startX + tx * stepX;
            float dy = startY + ty * stepY;
            int32_t sx = 0, sy = 0, cw = sw, ch = sh;

            if (tileX && roomW > 0.0f) {
                float left = dx, right = dx + stepX;
                float clipLeft = left < 0.0f ? 0.0f : left;
                float clipRight = right > roomW ? roomW : right;
                if (clipRight <= clipLeft) continue;
                if (clipLeft > left || clipRight < right) {
                    sx = (int32_t)floorf((clipLeft - left) / fabsf(xscale));
                    int32_t ex = (int32_t)ceilf((clipRight - left) / fabsf(xscale));
                    if (sx < 0) sx = 0;
                    if (ex > sw) ex = sw;
                    cw = ex - sx;
                    if (cw <= 0) continue;
                    dx += sx * xscale;
                }
            }

            if (tileY && roomH > 0.0f) {
                float top = dy, bottom = dy + stepY;
                float clipTop = top < 0.0f ? 0.0f : top;
                float clipBottom = bottom > roomH ? roomH : bottom;
                if (clipBottom <= clipTop) continue;
                if (clipTop > top || clipBottom < bottom) {
                    sy = (int32_t)floorf((clipTop - top) / fabsf(yscale));
                    int32_t ey = (int32_t)ceilf((clipBottom - top) / fabsf(yscale));
                    if (sy < 0) sy = 0;
                    if (ey > sh) ey = sh;
                    ch = ey - sy;
                    if (ch <= 0) continue;
                    dy += sy * yscale;
                }
            }

            renderer->vtable->drawSpritePart(renderer, tpagIndex,
                sx, sy, cw, ch, dx, dy,
                xscale, yscale, 0.0f, 0.0f, 0.0f,
                color, alpha);
        }
    }
}

void Ps1FastRenderer_install(Renderer* renderer) {
    if (!renderer || !renderer->vtable) return;
    /* RoomTile stays in the core renderer: it has the dedicated ATLAS.BIN
       lookup and the real TPAG fallback. */
    renderer->vtable->drawSpriteTiled = ps1FastDrawSpriteTiled;
}
