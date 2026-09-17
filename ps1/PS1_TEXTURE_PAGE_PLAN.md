# PS1 texture page implementation plan

1. Keep ATLAS.BIN parsing authoritative: atlas table, TPAG table, TILE table.
2. Preserve atlas compression metadata and port the upstream decompressor exactly.
3. Replace whole-atlas PS1 residency with fixed-size page/region residency.
4. Use 32 KiB texture-page units: 4bpp pages represent 256x256 indexed pixels; 8bpp pages represent 128x256 indexed pixels.
5. Track `(atlasId, pageX, pageY)` plus bpp as the cache key.
6. Decode/copy only the source region required for a page, including correct 4bpp nibble packing.
7. Translate atlas coordinates to local page UVs.
8. Split TPAG/TILE draws at page boundaries.
9. Keep CLUT4/CLUT8 separate from texture-page eviction.
10. Only after the implementation is complete, run a single validation build.
