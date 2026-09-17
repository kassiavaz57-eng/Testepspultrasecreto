#ifndef _BS_PS1_RENDERER_H_
#define _BS_PS1_RENDERER_H_
#include "common.h"
#include "renderer.h"
#include <psxgpu.h>

/* The PSn00bSDK setters normally take VRAM coordinates. The PS1 texture cache
 * already returns resolved primitive IDs, so these backend-local forms accept
 * those IDs directly. */
#undef setClut
#define setClut(p, x, ...) ((p)->clut = (x))

#undef setTPage
#define setTPage(p, x, ...) ((p)->tpage = (x))

/* The SDK's POLY_FT4 uses x0/y0 ... x3/y3. Keep macro parameters distinct
 * from those member names so the preprocessor cannot rewrite p->x0 into p->sx. */
#define BS_PS1_SETXY4_7(p,_x0,_y0,_x1,_y1,_x2,_y2) \
    ((p)->x0=(_x0),(p)->y0=(_y0),(p)->x1=(_x1),(p)->y1=(_y1), \
     (p)->x2=(_x2),(p)->y2=(_y2),(p)->x3=((_x2)+(_x0)-(_x1)), \
     (p)->y3=((_y2)+(_y0)-(_y1)))
#define BS_PS1_SETXY4_9(p,_x0,_y0,_x1,_y1,_x2,_y2,_x3,_y3) \
    ((p)->x0=(_x0),(p)->y0=(_y0),(p)->x1=(_x1),(p)->y1=(_y1), \
     (p)->x2=(_x2),(p)->y2=(_y2),(p)->x3=(_x3),(p)->y3=(_y3))
#define BS_PS1_SETXY4_SELECT(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME
#define BS_PS1_SETXY4(...) \
    BS_PS1_SETXY4_SELECT(__VA_ARGS__, BS_PS1_SETXY4_9, BS_PS1_SETXY4_9, BS_PS1_SETXY4_7)(__VA_ARGS__)
#undef setXY4
#define setXY4(...) BS_PS1_SETXY4(__VA_ARGS__)

/* The renderer API calls the type RendererVtable (lowercase t). */
#define RendererVTable RendererVtable

Renderer* Ps1Renderer_create(void);
void Ps1Renderer_present(void);
#endif
