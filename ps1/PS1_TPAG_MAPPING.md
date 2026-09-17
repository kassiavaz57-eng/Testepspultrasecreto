# PS1 TPAG crop-to-atlas mapping

This document records the mapping that the PS1 renderer must preserve when drawing real Butterscotch TPAGs.

The official Butterscotch atlas metadata distinguishes two coordinate spaces:

- `cropX/cropY/cropW/cropH`: original, pre-resize GameMaker sprite-space content.
- `atlasX/atlasY/width/height`: physical post-crop/post-resize atlas texels.

Therefore the PS1 renderer must not assume a 1:1 source-pixel-to-atlas-texel mapping. The upstream PS2 renderer explicitly derives:

```text
contentX = cropX - targetX
contentY = cropY - targetY
contentW = cropW
contentH = cropH
ratioX   = atlasWidth / contentW
ratioY   = atlasHeight / contentH
```

For a source-space point `(sx, sy)` inside the cropped content, its physical atlas coordinate is:

```text
atlasX = TPAG.atlasX + (sx - contentX) * ratioX
atlasY = TPAG.atlasY + (sy - contentY) * ratioY
```

A TPAG that crosses a PS1 texture-page boundary must then be split into page-local primitives. Each primitive keeps its own PS1 TPage/CLUT while preserving the above mapping.

The current page cache already preserves both coordinate spaces in `Ps1TexturePagePiece` so this mapping can be applied without reparsing `ATLAS.BIN`.

Source: official Butterscotch PS2 renderer (`gs_renderer.c`) and `AtlasTPAGEntry` definition. The PS1 implementation should mirror the semantics, not the PS2 API.
