# PS1 Chapter 1 fast path

This checkpoint deliberately narrows implementation to the real Butterscotch execution paths needed by Deltarune Chapter 1. It does **not** replace DataWin, VM, Runner, or the real preprocessed assets.

## Execution path

`DATA.WIN -> DataWin -> VM -> Runner -> PS1 renderer/filesystem/input/audio`

## Renderer priority

1. `drawSprite`
2. `drawSpritePart`
3. `drawSpritePos`
4. `drawSpriteTiled`
5. `drawTiledPart`
6. `drawTile`
7. room/background atlas rendering
8. only the surface operations actually reached by Chapter 1
9. text/UI rendering

The page-aware texture backend already resolves real `ATLAS.BIN -> TPAG -> TEXTURES.BIN -> CLUT -> PS1 TPage` data. The next renderer work must preserve the two coordinate spaces explicitly:

- GameMaker/source sprite space (`cropX`, `cropY`, `cropW`, `cropH`)
- physical post-crop/post-resize atlas space (`atlasX`, `atlasY`, `width`, `height`)

A TPAG crossing a PS1 texture-page boundary remains split into multiple `POLY_FT4` primitives; no whole-atlas upload is allowed.

## What is intentionally deferred

- generic shader support
- desktop/overlay rendering paths
- profiler/debug UI
- screenshot/debug-only features
- arbitrary surface emulation not reached by Chapter 1
- Chapter Selector
- Chapters 2-5 specific features

## Validation rule

No claim of Chapter 1 running is made until one coherent PS1 build is manually dispatched and the resulting executable is tested in a PS1 emulator. Intermediate implementation commits do not trigger Actions builds.
