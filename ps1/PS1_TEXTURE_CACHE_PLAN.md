# PS1 texture cache architecture

This document records the PS1 texture-cache design and the remaining renderer work before validation.

## Source format

The runtime consumes the assets produced by the official Butterscotch preprocessor:

- `ATLAS.BIN`
- `TEXTURES.BIN`
- `CLUT4.BIN`
- `CLUT8.BIN`

The PS1 backend preserves the upstream atlas/TPAG coordinate semantics and does not invent a second asset format.

## PS1 residency model

The PS1 cache is page-aware. A complete atlas is **not** treated as one permanent VRAM texture.

For indexed textures:

- 4bpp: one 32 KiB texture page represents 256x256 source pixels.
- 8bpp: one 32 KiB texture page represents 128x256 source pixels.

A cache entry is keyed by atlas + page coordinates + bpp. Entries are loaded on demand and evicted with LRU when their VRAM slot is needed.

## TPAG resolution

A TPAG references a rectangle in an atlas. Resolution must:

1. identify the atlas and its bpp;
2. determine every PS1 page intersecting the requested rectangle;
3. load/decode only the required page(s);
4. convert atlas coordinates into page-local UV coordinates;
5. return the PS1 TPage/CLUT for each page.

A sprite rectangle that crosses a PS1 page boundary cannot be represented by one `POLY_FT4` with wrapped 8-bit UVs. The renderer therefore consumes page-contained pieces.

## Implemented page backend

`ps1_texture_pages.c/.h` contains the concrete page-oriented backend:

- parses the official `ATLAS.BIN` atlas and TPAG tables;
- preserves the official RLE compression type 1 semantics;
- decodes compressed atlas data directly into one 32 KiB page buffer instead of materializing a whole atlas;
- reads only the required rows for uncompressed atlases;
- uses fixed 64-word x 256-line PS1 texture-page slots outside the 320-pixel framebuffer width;
- tracks `(atlasId, pageX, pageY, bpp)` and frame LRU state;
- treats pages touched by the current frame as pinned, preventing a later texture lookup from silently overwriting a page referenced by an already-built primitive;
- converts TPAG rectangles into page-contained pieces with local U/V coordinates;
- uploads CLUT4/CLUT8 palettes into the unused bottom VRAM lines using PS1-valid CLUT alignment;
- keeps the 32 KiB decode buffer inside the heap-owned cache object rather than allocating it on the PS1 thread stack.

The CMake target compiles this backend as part of the PS1 executable source set, and the PS1 renderer now resolves sprites through this page backend.

## Remaining renderer correctness work

The current page splitting is structurally real, but TPAG sprite rendering still needs to mirror the upstream renderer's crop coordinate system exactly. In particular, `cropX/cropY/cropW/cropH` are sprite/source-space data while `atlasX/atlasY/width/height` are atlas-space data; the PS1 renderer must preserve that distinction, including atlas down-scaling ratios.

Background/tile rendering and surface-backed rendering are also still incomplete. They must be implemented from the corresponding upstream renderer/data structures rather than replaced with placeholder geometry.

The page cache has 11 resident texture-page slots. Pages are pinned for the current frame, so if a frame requires more than 11 distinct pages before submission, the backend currently fails the lookup instead of overwriting an in-use page. A later batching/submission strategy must address this safely.

## Memory rules

Do not materialize all atlas pixels in RAM. The PS1 has limited RAM and the source atlas data may be substantially larger than a single VRAM page.

The implementation reuses the upstream compression/decompression semantics and streams/decodes only the page needed by the renderer.

## Validation policy

No GitHub Actions build is required for every intermediate edit. Code can be committed to the `ps1-experiment` branch as checkpointed progress. A validation build should be requested only after the page backend, renderer piece splitting, background/tile paths, and asset packaging form one coherent implementation milestone.
