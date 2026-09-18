from pathlib import Path
import re

# This script is the PS1 overlay patcher. It runs only after the real
# Butterscotch upstream sources have been cloned into ./upstream.

p = Path("ps1/ps1_renderer.c")
s = p.read_text()
s = re.sub(r"static void ps1EndView\(Renderer\*r\)\{\(void\)r[^}]*\}", "static void ps1EndView(Renderer*r){(void)r;}", s)
s = s.replace("RendererVTable", "RendererVtable")

marker = "static void ps1DrawSetBlendFactors(Renderer*r,BlendFactors f){((Ps1Renderer*)r)->blendFactors=f;}"
wrappers = r"""
static BlendFactors ps1GpuGetBlendFactors(Renderer*r){return ((Ps1Renderer*)r)->blendFactors;}
static int32_t ps1GpuGetBlendMode(Renderer*r){return ((Ps1Renderer*)r)->blendMode;}
static void ps1GpuSetBlendModeExt(Renderer*r,int32_t s,int32_t d,int32_t sa,int32_t da){BlendFactors f={s,d,sa,da};((Ps1Renderer*)r)->blendFactors=f;}
static void ps1GpuSetAlphaTestEnable(Renderer*r,bool e){((Ps1Renderer*)r)->alphaTestEnable=e;}
static bool ps1GpuGetAlphaTestEnable(Renderer*r){return ((Ps1Renderer*)r)->alphaTestEnable;}
static void ps1GpuSetAlphaTestRef(Renderer*r,uint8_t ref){((Ps1Renderer*)r)->alphaTestRef=ref;}
static void ps1GpuGetColorWriteEnable(Renderer*r,bool*rr,bool*g,bool*b,bool*a){Ps1Renderer*p=(Ps1Renderer*)r;if(rr)*rr=p->colorWriteR;if(g)*g=p->colorWriteG;if(b)*b=p->colorWriteB;if(a)*a=p->colorWriteA;}
static bool ps1GpuGetBlendEnable(Renderer*r){return ((Ps1Renderer*)r)->blendEnable;}
static void ps1GpuSetFog(Renderer*r,bool e,uint32_t c){(void)r;(void)e;(void)c;}
"""
if marker in s and "ps1GpuGetBlendFactors" not in s:
    s = s.replace(marker, marker + wrappers)

old_present = "static void ps1Present(Renderer*r){Ps1Renderer*p=(Ps1Renderer*)r;DrawSync(0);VSync(0);PutDispEnv(&p->buffers[p->active].disp);PutDrawEnv(&p->buffers[p->active].draw);DrawOTagEnv(p->buffers[p->active].ot,&p->buffers[p->active].draw);p->active^=1;}"
new_present = "static void ps1Present(Renderer*r){Ps1Renderer*p=(Ps1Renderer*)r;DrawOTagEnv(p->buffers[p->active].ot,&p->buffers[p->active].draw);DrawSync(0);VSync(0);p->active^=1;PutDispEnv(&p->buffers[p->active].disp);PutDrawEnv(&p->buffers[p->active].draw);}"
s = s.replace(old_present, new_present)

start = s.find("Renderer* Ps1Renderer_create(void)")
if start < 0:
    raise SystemExit("Ps1Renderer_create not found")
prefix = s[:start]
tail = r"""static RendererVtable gPs1Vtable = {
    .init=ps1Init,
    .destroy=ps1Destroy,
    .beginFrame=ps1BeginFrame,
    .endFrameInit=ps1EndFrameInit,
    .endFrameEnd=ps1EndFrameEnd,
    .beginView=ps1BeginView,
    .endView=ps1EndView,
    .applyProjection=ps1ApplyProjection,
    .beginGUI=ps1BeginGUI,
    .setGuiProjection=ps1SetGuiProjection,
    .endGUI=ps1EndGUI,
    .drawSprite=ps1DrawSprite,
    .drawSpritePart=ps1DrawSpritePart,
    .drawSpritePartColor=ps1DrawSpritePartColor,
    .drawSpritePos=ps1DrawSpritePos,
    .drawRectangle=ps1DrawRectangle,
    .drawRectangleColor=ps1DrawRectangleColor,
    .drawLine=ps1DrawLine,
    .drawTriangle=ps1DrawTriangle,
    .drawLineColor=ps1DrawLineColor,
    .drawText=ps1DrawText,
    .drawTextColor=ps1DrawTextColor,
    .drawTextUI=ps1DrawTextColor,
    .gpuGetBlendFactors=ps1GpuGetBlendFactors,
    .gpuGetBlendMode=ps1GpuGetBlendMode,
    .gpuSetBlendMode=ps1DrawSetBlendMode,
    .gpuSetBlendModeExt=ps1GpuSetBlendModeExt,
    .gpuSetBlendEnable=ps1DrawSetBlendEnable,
    .gpuSetAlphaTestEnable=ps1GpuSetAlphaTestEnable,
    .gpuGetAlphaTestEnable=ps1GpuGetAlphaTestEnable,
    .gpuSetAlphaTestRef=ps1GpuSetAlphaTestRef,
    .gpuSetColorWriteEnable=ps1DrawSetColorWriteMask,
    .gpuGetColorWriteEnable=ps1GpuGetColorWriteEnable,
    .gpuGetBlendEnable=ps1GpuGetBlendEnable,
    .gpuSetFog=ps1GpuSetFog,
    .drawTile=ps1DrawTile,
    .drawSpriteTiled=ps1DrawSpriteTiled,
    .drawTiledPart=ps1DrawTiledPart
};
Renderer* Ps1Renderer_create(void){Ps1Renderer*p=(Ps1Renderer*)calloc(1,sizeof(*p));if(!p)return NULL;p->active=0;p->base.vtable=&gPs1Vtable;gPs1Renderer=p;return &p->base;}
void Ps1Renderer_present(void){if(gPs1Renderer)ps1Present(&gPs1Renderer->base);}
"""
p.write_text(prefix + tail)

p = Path("ps1/ps1_fast_renderer.c")
p.write_text(p.read_text().replace("RendererVTable", "RendererVtable"))

dp = Path("upstream/src/data_win.c")
ds = dp.read_text()
hook = """#ifdef PLATFORM_PS1
extern void ps1DataWinDebugStage(const char* stage);
#define PS1_DATAWIN_STAGE(stage) ps1DataWinDebugStage(stage)
#else
#define PS1_DATAWIN_STAGE(stage) ((void)0)
#endif
"""
if "PS1_DATAWIN_STAGE" not in ds:
    ds = hook + ds

start = ds.find("    g->timestamp = BinaryReader_readUint64(reader);")
end_marker = "    // Seed the detected version from GEN8.\n"
end = ds.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("modern GEN8 parser block not found")
modern_replacement = r"""    g->timestamp = BinaryReader_readUint64(reader);
    PS1_DATAWIN_STAGE("modern-timestamp");
    g->displayName = readStringPtr(reader, dw);
    PS1_DATAWIN_STAGE("modern-display");

    size_t ps1_gen8_remaining = reader->buffer != nullptr
        ? reader->bufferSize - reader->bufferPos
        : reader->fileSize - BinaryReader_getPosition(reader);

    g->activeTargets = 0;
    g->functionClassifications = 0;
    g->steamAppID = 0;
    g->debuggerPort = 0;
    g->roomOrderCount = 0;
    g->roomOrder = nullptr;
    g->gms2FPS = 0.0f;

    if (ps1_gen8_remaining >= 8) {
        g->activeTargets = BinaryReader_readUint64(reader);
        ps1_gen8_remaining -= 8;
    }
    if (ps1_gen8_remaining >= 8) {
        g->functionClassifications = BinaryReader_readUint64(reader);
        ps1_gen8_remaining -= 8;
    }
    if (ps1_gen8_remaining >= 4) {
        g->steamAppID = BinaryReader_readInt32(reader);
        ps1_gen8_remaining -= 4;
    }
    if (g->wadVersion >= 14 && ps1_gen8_remaining >= 4) {
        g->debuggerPort = BinaryReader_readUint32(reader);
        ps1_gen8_remaining -= 4;
    }

    if (ps1_gen8_remaining >= 4) {
        uint32_t count = BinaryReader_readUint32(reader);
        ps1_gen8_remaining -= 4;
        if ((uint64_t)count <= ps1_gen8_remaining / 4u) {
            g->roomOrderCount = count;
            if (count > 0) {
                g->roomOrder = (int32_t *)safeMalloc((size_t)count * sizeof(int32_t));
                repeat(count, i) {
                    g->roomOrder[i] = BinaryReader_readInt32(reader);
                }
                ps1_gen8_remaining -= (size_t)count * 4u;
            }
        }
    }

    if (g->major >= 2 && ps1_gen8_remaining >= 64) {
        BinaryReader_skip(reader, 8);
        BinaryReader_skip(reader, 8 * 4);
        g->gms2FPS = BinaryReader_readFloat32(reader);
        BinaryReader_skip(reader, 4);
        BinaryReader_skip(reader, 16);
    }
"""
ds = ds[:start] + modern_replacement + ds[end:]
complete_marker = "    // Seed the detected version from GEN8.\n"
if complete_marker not in ds:
    raise SystemExit("GEN8 completion marker not found")
ds = ds.replace(complete_marker, complete_marker + '    PS1_DATAWIN_STAGE("modern-complete");\n', 1)
sprt_mask_guard = 'if (spr->sepMasks == 1 || !skipLoadingPreciseMasksForNonPreciseSprites) {'
sprt_mask_replacement = '''#ifdef PLATFORM_PS1
            /*
             * Chapter 1 boot does not need pixel collision masks. Keep the real
             * SPRT metadata and consume the mask bytes, but do not allocate the
             * potentially huge mask arrays on the PS1 heap. Precise masks can be
             * restored once Chapter 1 reaches a path that demonstrably requires them.
             */
            if (false) {
#else
            if (spr->sepMasks == 1 || !skipLoadingPreciseMasksForNonPreciseSprites) {
#endif'''
if sprt_mask_guard not in ds:
    raise SystemExit("SPRT mask guard not found in upstream data_win.c")
ds = ds.replace(sprt_mask_guard, sprt_mask_replacement, 1)

dp.write_text(ds)
