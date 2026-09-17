#include "ps1_fast_renderer.h"
#include <math.h>

static void ps1FastDrawTile(Renderer* renderer, RoomTile* tile, float offsetX, float offsetY) {
    if (renderer == NULL || tile == NULL || renderer->vtable == NULL || renderer->vtable->drawSpritePart == NULL) return;

    int32_t tpagIndex = Renderer_resolveObjectTPAGIndex(renderer->dataWin, tile);
    if (tpagIndex < 0 || (uint32_t)tpagIndex >= renderer->dataWin->tpag.count) return;

    TexturePageItem* tpag = &renderer->dataWin->tpag.items[tpagIndex];
    int32_t srcX = tile->sourceX;
    int32_t srcY = tile->sourceY;
    int32_t srcW = (int32_t)tile->width;
    int32_t srcH = (int32_t)tile->height;
    float drawX = (float)tile->x + offsetX;
    float drawY = (float)tile->y + offsetY;

    /* RoomTile coordinates are in the source/background bounding rectangle;
       TPAG source coordinates start at targetX/targetY after transparent
       trimming. Match the upstream renderer's clipping rules. */
    if (tpag->targetX > srcX) {
        int32_t clip = tpag->targetX - srcX;
        drawX += (float)clip * tile->scaleX;
        srcW -= clip;
        srcX = tpag->targetX;
    }
    if (tpag->targetY > srcY) {
        int32_t clip = tpag->targetY - srcY;
        drawY += (float)clip * tile->scaleY;
        srcH -= clip;
        srcY = tpag->targetY;
    }

    int32_t right = tpag->targetX + tpag->sourceWidth;
    int32_t bottom = tpag->targetY + tpag->sourceHeight;
    if (srcX + srcW > right) srcW = right - srcX;
    if (srcY + srcH > bottom) srcH = bottom - srcY;
    if (srcW <= 0 || srcH <= 0) return;

    srcX -= tpag->targetX;
    srcY -= tpag->targetY;

    renderer->vtable->drawSpritePart(
        renderer, tpagIndex, srcX, srcY, srcW, srcH,
        drawX, drawY, tile->scaleX, tile->scaleY,
        0.0f, 0.0f, 0.0f,
        tile->color & 0x00FFFFFFu, tile->alpha
    );
}

static void ps1FastDrawSpriteTiled(
    Renderer* renderer, int32_t tpagIndex,
    float originX, float originY,
    float x, float y, float xscale, float yscale,
    bool tileX, bool tileY,
    float roomW, float roomH,
    uint32_t color, float alpha
) {
    if (renderer == NULL || renderer->dataWin == NULL || renderer->vtable == NULL ||
        renderer->vtable->drawSpritePart == NULL) return;
    if (tpagIndex < 0 || (uint32_t)tpagIndex >= renderer->dataWin->tpag.count) return;

    TexturePageItem* tpag = &renderer->dataWin->tpag.items[tpagIndex];
    int32_t sourceW = (int32_t)tpag->sourceWidth;
    int32_t sourceH = (int32_t)tpag->sourceHeight;
    if (sourceW <= 0 || sourceH <= 0 || xscale == 0.0f || yscale == 0.0f) return;

    float stepX = fabsf((float)sourceW * xscale);
    float stepY = fabsf((float)sourceH * yscale);
    if (stepX <= 0.0f || stepY <= 0.0f) return;

    float maxW = tileX ? roomW : stepX;
    float maxH = tileY ? roomH : stepY;
    if (maxW <= 0.0f) maxW = stepX;
    if (maxH <= 0.0f) maxH = stepY;

    int xCount = tileX ? (int)ceilf(maxW / stepX) : 1;
    int yCount = tileY ? (int)ceilf(maxH / stepY) : 1;
    if (xCount < 1) xCount = 1;
    if (yCount < 1) yCount = 1;
    if (xCount > 128) xCount = 128;
    if (yCount > 128) yCount = 128;

    for (int ty = 0; ty < yCount; ++ty) {
        for (int tx = 0; tx < xCount; ++tx) {
            float drawX = x + (float)tx * stepX;
            float drawY = y + (float)ty * stepY;
            float remainingW = maxW - (float)tx * stepX;
            float remainingH = maxH - (float)ty * stepY;
            int32_t srcW = sourceW;
            int32_t srcH = sourceH;
            float drawScaleX = xscale;
            float drawScaleY = yscale;

            if (tileX && remainingW < stepX) {
                srcW = (int32_t)floorf(remainingW / fabsf(xscale));
                if (srcW <= 0) continue;
            }
            if (tileY && remainingH < stepY) {
                srcH = (int32_t)floorf(remainingH / fabsf(yscale));
                if (srcH <= 0) continue;
            }

            /* drawSpritePart's pivot convention places the source origin at
               x-originX*scale, matching GameMaker's tiled sprite behavior. */
            renderer->vtable->drawSpritePart(
                renderer, tpagIndex, 0, 0, srcW, srcH,
                drawX, drawY, drawScaleX, drawScaleY,
                0.0f, originX, originY, color, alpha
            );
        }
    }
}

void Ps1FastRenderer_install(Renderer* renderer) {
    if (renderer == NULL || renderer->vtable == NULL) return;
    renderer->vtable->drawTile = ps1FastDrawTile;
    renderer->vtable->drawSpriteTiled = ps1FastDrawSpriteTiled;
}
