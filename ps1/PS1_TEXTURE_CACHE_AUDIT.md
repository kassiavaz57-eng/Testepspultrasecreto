# PS1 texture cache audit

This checkpoint records the implementation gap before replacing the current whole-atlas cache with a real PS1 page cache.

## Confirmed upstream facts

The current Butterscotch PS2 renderer parses `ATLAS.BIN` as:

1. header
2. atlas table
3. TPAG table
4. TILE table

Atlas metadata is `offset, width, height, bpp, dataSize, compression`. TPAG metadata is `atlasId, atlasX, atlasY, width, height, cropX, cropY, cropW, cropH, clutIndex`. TILE records contain the additional background/source rectangle and atlas/crop metadata.

`TEXTURES.BIN` stores indexed atlas pixels. The upstream renderer's compression type `1` is byte RLE: `(runLength, value)` pairs expanded until the exact uncompressed pixel-byte count is reached. Other compression types are copied as uncompressed data. The uncompressed size is `(width * height + 1) / 2` for 4bpp and `width * height` for 8bpp.

## What the current PS1 implementation gets right

- ATLAS table/TPAG parsing order follows the current PS2 renderer.
- 4bpp atlas storage is treated as packed indexed pixels rather than one byte per pixel.
- The RLE algorithm matches the upstream PS2 semantics for compression type 1.
- CLUT4 and CLUT8 are read from the official preprocessor outputs.
- Renderer texture lookup is already connected to the PS1 cache instead of using a placeholder rectangle.

## What must change before real sprite rendering can be considered correct

The current PS1 cache still uploads an entire atlas into one VRAM rectangle. That is not the final architecture because a GameMaker atlas can be larger than a single PS1 texture page and PS1 texture addressing is page-based.

The replacement cache must therefore:

- use fixed 32 KiB cache units;
- key a cached unit by `(atlasId, pageX, pageY, bpp)`;
- use 4bpp source pages of 256x256 pixels (32 KiB), which map to four horizontal PS1 64x256 texture pages;
- use 8bpp source pages of 128x256 pixels (32 KiB), which map to one PS1 128x256 texture page;
- load/decompress only the source region needed by a TPAG rectangle;
- keep an LRU timestamp per resident page/unit;
- never require the complete atlas to exist simultaneously in PS1 RAM;
- convert atlas coordinates to page-local UVs;
- detect TPAG rectangles crossing page boundaries;
- expose enough information for the renderer to split one logical sprite into multiple PS1 textured primitives when necessary;
- keep CLUT residency independent from atlas-page residency.

## Important implementation constraint

Do not invent another texture compression format. The PS1 implementation must reuse the exact upstream `TEXTURES.BIN` RLE semantics above and the official preprocessor output files `ATLAS.BIN`, `TEXTURES.BIN`, `CLUT4.BIN`, and `CLUT8.BIN`.

## Validation policy

This file is a code checkpoint only. No GitHub Actions build is intentionally triggered for this intermediate state. The first validation build should happen after the page cache, TPAG splitting, and renderer integration form one coherent block.
