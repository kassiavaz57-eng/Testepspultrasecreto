#ifndef BS_PS1_FAST_RENDERER_H
#define BS_PS1_FAST_RENDERER_H

#include "renderer.h"

/* Installs only the high-value renderer paths needed by real GameMaker rooms:
 * room tiles and tiled sprites. The underlying Runner/DataWin/TPAG interfaces
 * remain unchanged. */
void Ps1FastRenderer_install(Renderer* renderer);

#endif
