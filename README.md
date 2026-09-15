# Butterscotch PSP — platform overlay

Small PSP-only layer for the Butterscotch runtime. It deliberately excludes the upstream source tree.

The workflow clones upstream Butterscotch, checks that the expected APIs/CMake anchors still exist, applies the PSP files, validates the result, and then builds with PSPDEV.

Current milestone: real PSP bootstrap + DataWin/Runner integration, PSP filesystem and controller input, 30 Hz update target. Rendering/audio are intentionally still no-op; the next milestone replaces those with PSP GU/audio backends.

This is designed to fail clearly if upstream changes rather than silently compiling an incompatible overlay.
