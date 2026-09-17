# Butterscotch PS1 backend plan

The PS1 port follows the existing Butterscotch platform architecture rather than replacing the Runner/VM/DataWin.

Reference backend: `upstream/src/ps2/`.

## Mapping

- PS2 main/bootstrap -> PS1 main/bootstrap
- PS2 FileSystem -> PS1 CD-ROM/FileSystem + future Memory Card save backend
- PS2 renderer (GS) -> PS1 renderer (GPU primitives/VRAM/CLUT)
- PS2 audio (audsrv/SPU2) -> PS1 audio (SPU), retaining SOND/AUDO/MUS parsing and streaming strategy where memory permits
- PS2 gamepad -> PS1 pad/DualShock input
- Runner, VM, DataWin and resource formats remain upstream

## Chapter 1 priority

Required: room rendering, sprites/text, movement/input, transitions, gameplay/VM execution, audio/music/SFX.

Optional later: screenshots, profiler/debug overlays, Chapter Selector, Chapters 2-5.

Save should ultimately map GameMaker save operations to PS1 Memory Card, not emulator save states.

## Important memory rule

DATA.WIN and large resource files must not be copied wholesale into the ~2 MB PS1 RAM. FileSystem/renderer/audio backends must stream or load only the required chunks/resources.

## Current state

The existing `main.c` still uses the generic `loop()` and `NOOP`; this is transitional only. The long-term bootstrap should mirror the platform-specific PS2 structure and inject PS1 FileSystem/Renderer/Audio/Gamepad backends.

Do not add diagnostic-only builds as a substitute for implementing these backends. Compile only after the backend structure is coherent.
