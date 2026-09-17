#include "ps1_fast_renderer.h"
#include <math.h>

/* Fast room path for the real GameMaker tiled-sprite operation.
   The tile origin may lie outside the room, so the first visible tile can
   have a negative index. We iterate the source-space repetition range that
   actually intersects the room instead of assuming the origin is positive. */
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

    /* GameMaker's x/y is the sprite origin. Convert it once into the
       top-left of repetition #0 in room coordinates. */
    const float startX = x - originX * xscale;
    const float startY = y - originY * yscale;

    int firstX = 0, firstY = 0, lastX = 0, lastY = 0;
    if (tileX && roomW > 0.0f) {
        firstX = (int)floorf(-startX / stepX);
        lastX = (int)ceilf((roomW - startX) / stepX) - 1;
    }
    if (tileY && roomH > 0.0f) {
        firstY = (int)floorf(-startY / stepY);
        lastY = (int)ceilf((roomH - startY) / stepY) - 1;
    }

    /* Avoid pathological malformed DATA.WIN values consuming the whole
       frame, while keeping a generous range for normal rooms. */
    if (lastX - firstX > 128) lastX = firstX + 128;
    if (lastY - firstY > 128) lastY = firstY + 128;
    if (firstX - lastX > 128) firstX = lastX - 128;
    if (firstY - lastY > 128) firstY = lastY - 128;

    for (int ty = firstY; ty <= lastY; ++ty) {
        for (int tx = firstX; tx <= lastX; ++tx) {
            float dx = startX + tx * stepX;
            float dy = startY + ty * stepY;
            int32_t cw = sw;
            int32_t ch = sh;

            if (tileX && roomW > 0.0f) {
                float left = dx;
                float right = dx + stepX;
                float clipLeft = left < 0.0f ? 0.0f : left;
                float clipRight = right > roomW ? roomW : right;
                if (clipRight <= clipLeft) continue;
                if (clipLeft > left || clipRight < right) {
                    float sourceLeft = (clipLeft - left) / fabsf(xscale);
                    float sourceRight = (clipRight - left) / fabsf(xscale);
                    int32_t sx = (int32_t)floorf(sourceLeft);
                    int32_t ex = (int32_t)ceilf(sourceRight);
                    if (sx < 0) sx = 0;
                    if (ex > sw) ex = sw;
                    if (ex <= sx) continue;
                    renderer->vtable->drawSpritePart(renderer, tpagIndex,
                        sx, 0, ex - sx, sh,
                        dx + (sx * xscale), dy,
                        xscale, yscale, 0.0f, 0.0f, 0.0f,
                        color, alpha);
                    if (tileY && roomH > 0.0f) {
                        /* The 2-D edge case is handled below by the normal
                           path when Y is also clipped; don't duplicate it. */
                    }
                    continue;
                }
            }

            if (tileY && roomH > 0.0f) {
                float top = dy;
                float bottom = dy + stepY;
                float clipTop = top < 0.0f ? 0.0f : top;
                float clipBottom = bottom > roomH ? roomH : bottom;
                if (clipBottom <= clipTop) continue;
                if (clipTop > top || clipBottom < bottom) {
                    float sourceTop = (clipTop - top) / fabsf(yscale);
                    float sourceBottom = (clipBottom - top) / fabsf(yscale);
                    int32_t sy = (int32_t)floorf(sourceTop);
                    int32_t ey = (int32_t)ceilf(sourceBottom);
                    if (sy < 0) sy = 0;
                    if (ey > sh) ey = sh;
                    if (ey <= sy) continue;
                    renderer->vtable->drawSpritePart(renderer, tpagIndex,
                        0, sy, sw, ey - sy,
                        dx, dy + (sy * yscale),
                        xscale, yscale, 0.0f, 0.0f, 0.0f,
                        color, alpha);
                    continue;
                }
            }

            renderer->vtable->drawSpritePart(renderer, tpagIndex,
                0, 0, cw, ch, dx, dy,
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
