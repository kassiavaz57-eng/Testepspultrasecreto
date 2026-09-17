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

/* Accept both the complete 4-vertex form and the older sprite-part call that
 * supplies only the first three vertices. The latter is a rotated rectangle,
 * so the fourth vertex is the parallelogram completion. */
#define BS_PS1_SETXY4_7(p,x0,y0,x1,y1,x2,y2) \
    ((p)->x0=(x0),(p)->y0=(y0),(p)->x1=(x1),(p)->y1=(y1), \
     (p)->x2=(x2),(p)->y2=(y2),(p)->x3=((x2)+(x0)-(x1)), \
     (p)->y3=((y2)+(y0)-(y1)))
#define BS_PS1_SETXY4_9(p,x0,y0,x1,y1,x2,y2,x3,y3) \
    ((p)->x0=(x0),(p)->y0=(y0),(p)->x1=(x1),(p)->y1=(y1), \
     (p)->x2=(x2),(p)->y2=(y2),(p)->x3=(x3),(p)->y3=(y3))
#define BS_PS1_SETXY4_SELECT(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME
#define BS_PS1_SETXY4(...) \
    BS_PS1_SETXY4_SELECT(__VA_ARGS__, BS_PS1_SETXY4_9, BS_PS1_SETXY4_9, BS_PS1_SETXY4_7)(__VA_ARGS__)
#undef setXY4
#define setXY4(...) BS_PS1_SETXY4(__VA_ARGS__)

Renderer* Ps1Renderer_create(void);
void Ps1Renderer_present(void);
#endif
