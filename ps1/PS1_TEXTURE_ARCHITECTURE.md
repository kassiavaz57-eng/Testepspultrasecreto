# PS1 texture architecture

The PS1 backend must consume the same preprocessed assets as the upstream Butterscotch PS2 path, but cannot assume that a complete atlas can remain resident as one GPU texture.

## Real asset pipeline

`DATA.WIN` is processed by the official Butterscotch preprocessor into `ATLAS.BIN`, `TEXTURES.BIN`, `CLUT4.BIN`, `CLUT8.BIN` and audio assets. The PS1 backend must not invent a second asset format.

## Runtime model

```text
ATLAS.BIN
  -> atlas metadata + TPAG/TILE metadata
TEXTURES.BIN
  -> compressed indexed atlas payload
  -> decode only the PS1 pages/regions needed by a draw
  -> VRAM texture page(s)
CLUT4/CLUT8
  -> PS1 CLUT slots
TPAG/TILE
  -> atlas coordinates
  -> page coordinates + local UV
  -> POLY_FT4
```

## PS1 cache requirements

- Cache entries are page/region based, not simply `atlasId -> one VRAM allocation`.
- 4bpp and 8bpp use different PS1 texture-page widths.
- Cache entries need an LRU timestamp and must be evictable unless pinned by a draw/surface requirement.
- Atlas payloads may be compressed using the format already implemented by the upstream PS2 renderer; compression must be ported, not replaced with an invented codec.
- A TPAG whose requested rectangle crosses a PS1 texture-page boundary must be rendered as multiple textured primitives with local UVs. UV arithmetic must never silently wrap at 8-bit coordinates.
- CLUT allocation is independent from texture-page allocation.
- The runtime should avoid materializing an entire large atlas in PS1 RAM merely to draw one TPAG.

## Current implementation status

This document is an architecture checkpoint only. The renderer/cache rewrite must be completed and reviewed before the next PS1 build. No build is implied by this checkpoint.
