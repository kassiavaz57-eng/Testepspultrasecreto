# PS1 texture cache architecture

This document records the PS1 texture-cache design before the next validation build.

## Source format

The runtime consumes the assets produced by the official Butterscotch preprocessor:

- `ATLAS.BIN`
- `TEXTURES.BIN`
- `CLUT4.BIN`
- `CLUT8.BIN`

The PS1 backend must preserve the upstream atlas/TPAG coordinate semantics. It must not invent a second asset format.

## PS1 residency model

The PS1 cache is page-aware. A complete atlas is **not** treated as one permanent VRAM texture.

For indexed textures:

- 4bpp: one 32 KiB texture page represents 256x256 source pixels.
- 8bpp: one 32 KiB texture page represents 128x256 source pixels.

A cache entry is therefore keyed by atlas + page coordinates + bpp. Entries are loaded on demand and evicted with LRU when their VRAM slot is needed.

## TPAG resolution

A TPAG references a rectangle in an atlas. Resolution must:

1. identify the atlas and its bpp;
2. determine every PS1 page intersecting the requested rectangle;
3. load/decode only the required page(s);
4. convert atlas coordinates into page-local UV coordinates;
5. return the PS1 TPage/CLUT for each page.

A sprite rectangle that crosses a PS1 page boundary cannot be represented by one `POLY_FT4` with wrapped 8-bit UVs. The renderer must split it into page-contained pieces.

## Memory rules

Do not materialize all atlas pixels in RAM. The PS1 has limited RAM and the source atlas data may be substantially larger than a single VRAM page.

The implementation should reuse the upstream compression/decompression semantics and stream/decode only the page needed by the renderer.

## Validation policy

No GitHub Actions build is required for every intermediate edit. Code can be committed to the `ps1-experiment` branch as checkpointed progress. A validation build should be requested only after a coherent implementation milestone is complete.
