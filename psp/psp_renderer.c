    Matrix4f_identity(&world);
    renderer->gmlMatrices[MATRIX_WORLD] = world;
    renderer->drawColor = 0xFFFFFF;
    renderer->drawAlpha = 1.0f;
    renderer->drawFont = -1;
    renderer->currentShader = -1;

    void *fb0 = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_8888);
    void *fb1 = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_8888);
    void *zb = guGetStaticVramBuffer(PSP_BUF_W, PSP_H, GU_PSM_4444);

    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, PSP_BUF_W);
    sceGuDispBuffer(PSP_W, PSP_H, fb1, PSP_BUF_W);
    sceGuDepthBuffer(zb, PSP_BUF_W);
    sceGuOffset(2048 - PSP_W / 2, 2048 - PSP_H / 2);
    sceGuViewport(2048, 2048, PSP_W, PSP_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, PSP_W, PSP_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_LIGHTING);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuDisplay(GU_DISPLAY_ON);
    g_guReady = 1;
    logInfo("PSP GU renderer initialized\n");
}

static void pspDestroy(Renderer *renderer) {
    cacheClear();
    if (g_guReady) {
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        sceGuTerm();
        g_guReady = 0;
    }
    RendererVtable *vt = renderer->vtable;
    renderer->vtable = g_baseVtable;
    if (g_baseVtable && g_baseVtable->destroy)
        g_baseVtable->destroy(renderer);
    free(vt);
    g_pspVtable = NULL;
    g_baseVtable = NULL;
}

static void pspBeginFrame(Renderer *renderer, int32_t gameW, int32_t gameH, int32_t windowW, int32_t windowH) {
    (void)windowW; (void)windowH;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(GU_RGBA(0, 0, 0, 255));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    setOrtho(0, (float)gameW, 0, (float)gameH, 0, 0, PSP_W, PSP_H);
    renderer->CPortX = 0;
    renderer->CPortY = 0;
    renderer->CPortW = PSP_W;
    renderer->CPortH = PSP_H;
}

static void pspEndFrameInit(Renderer *renderer) {
    (void)renderer;
}

static void pspEndFrameEnd(Renderer *renderer) {
    (void)renderer;
    releaseLargePageCache();
}

static void pspBeginView(Renderer *renderer, int32_t viewX, int32_t viewY, int32_t viewW, int32_t viewH, int32_t portX, int32_t portY, int32_t portW, int32_t portH, float viewAngle) {
    (void)viewAngle;
    renderer->CPortX = portX; renderer->CPortY = portY; renderer->CPortW = portW; renderer->CPortH = portH;
    setOrtho((float)viewX, (float)(viewX + viewW), (float)viewY, (float)(viewY + viewH), portX, portY, portW, portH);
}

static void pspEndView(Renderer *renderer) { (void)renderer; }

static void pspBeginGUI(Renderer *renderer, int32_t guiW, int32_t guiH, int32_t portX, int32_t portY, int32_t portW, int32_t portH, int32_t targetSurfaceId) {
    (void)targetSurfaceId;
    renderer->CPortX = portX; renderer->CPortY = portY; renderer->CPortW = portW; renderer->CPortH = portH;
    setOrtho(0, (float)guiW, 0, (float)guiH, portX, portY, portW, portH);
}

static void pspSetGuiProjection(Renderer *renderer, int32_t guiW, int32_t guiH, int32_t portW, int32_t portH, bool renderingToUserSurface) {
    (void)renderer; (void)renderingToUserSurface;
    setOrtho(0, (float)guiW, 0, (float)guiH, 0, 0, portW, portH);
}

static void pspEndGUI(Renderer *renderer) { (void)renderer; }

static void drawQuad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                     float u0, float v0, float u1, float v1, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) {
    PSPVertex *v = (PSPVertex *)sceGuGetMemory(4 * sizeof(PSPVertex));