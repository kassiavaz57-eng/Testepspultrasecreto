# PS1 VRAM layout used by the real renderer path

The 320x240 double-buffered display uses:

- buffer A: `(0, 0)` through `(319, 239)`
- buffer B: `(0, 240)` through `(319, 479)`

The right-hand VRAM area is therefore available for indexed texture pages without overlapping either framebuffer:

- texture pages: `x = 320..1023`, `y = 0..255`
- each page slot: 64 VRAM words x 256 lines = 32 KiB
- 11 page slots are available at x = 320, 384, 448, ..., 960

The bottom 32 VRAM lines are outside both framebuffers and the page row:

- CLUT8: y = 480..495, four 256-word palettes per line
- CLUT4: y = 496, 64 sixteen-word palettes

This layout is intentional: PS1 texture-page coordinates are aligned to 64 VRAM words horizontally and 256 lines vertically. The page cache therefore maps each logical atlas page directly to one valid PS1 TPage.

## Consequences

- A 4bpp page contains 256x256 logical pixels.
- An 8bpp page contains 128x256 logical pixels.
- A TPAG crossing a page boundary must be split before primitive emission.
- A page touched by the current frame is pinned until the frame's GPU work is safely submitted/completed; otherwise LRU replacement could overwrite VRAM still referenced by an already-built primitive.
- The page cache must never use the left 320 pixels of the framebuffer rows for textures.
