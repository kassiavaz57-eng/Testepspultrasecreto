#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <psxetc.h>
#include <psxgpu.h>

#include "runner.h"
#include "runner_keyboard.h"
#include "runner_gamepad.h"
#include "../data_win.h"
#include "../vm.h"
#include "../noop_audio_system.h"
#include "ps1_utils.h"
#include "ps1_file_system.h"
#include "ps1_renderer.h"
#include "ps1_fast_renderer.h"
#include "ps1_gamepad.h"
#include "log.h"
#include "gettime.h"

#define PS1_GAME_WIDTH 320
#define PS1_GAME_HEIGHT 240
#define PS1_FRAME_NS 33333333ULL

/* Temporary on-screen checkpoints: keep the real runtime intact while locating the early boot stop. */
static DISPENV ps1DebugDisp;
static DRAWENV ps1DebugDraw;
static int ps1DebugFontId = -1;

static void ps1DebugColor(uint8_t r, uint8_t g, uint8_t b);

void ps1DataWinDebugStage(const char* stage) {
    ps1DebugColor(96, 32, 0);
    if (ps1DebugFontId >= 0) {
        FntPrint(ps1DebugFontId, "GEN8 %s", stage);
        FntFlush(-1);
    }
}

static void ps1DebugGpuInit(void) {
    ResetGraph(0);
    SetDefDispEnv(&ps1DebugDisp, 0, 0, PS1_GAME_WIDTH, PS1_GAME_HEIGHT);
    SetDefDrawEnv(&ps1DebugDraw, 0, 0, PS1_GAME_WIDTH, PS1_GAME_HEIGHT);
    PutDispEnv(&ps1DebugDisp);
    PutDrawEnv(&ps1DebugDraw);
    SetDispMask(1);
    FntLoad(960, 0);
    ps1DebugFontId = FntOpen(8, 8, 304, 16, 0, 80);
}

static void ps1DebugColor(uint8_t r, uint8_t g, uint8_t b) {
    /* Use the normal DRAWENV clear path instead of ClearImage, keeping this
       checkpoint compatible with the same GPU setup used by the real renderer. */
    setRGB0(&ps1DebugDraw, r, g, b);
    ps1DebugDraw.isbg = 1;
    PutDrawEnv(&ps1DebugDraw);
    DrawSync(0);
    VSync(0);
}

static void ps1ParseProgress(const char* chunkName, int chunkIndex, int totalChunks, DataWin* dataWin, void* userData) {
    (void)dataWin;
    (void)userData;
    /* Temporary parser probe: each chunk gets a distinct visible colour.
       If parsing aborts/hangs, DuckStation shows the last chunk reached. */
    static const uint8_t palette[][3] = {
        {32, 32, 32}, {64, 0, 96}, {0, 64, 96}, {0, 96, 64},
        {96, 64, 0}, {96, 0, 64}, {64, 96, 0}, {0, 96, 96},
        {96, 32, 32}, {32, 96, 32}, {32, 32, 96}, {96, 96, 32}
    };
    const unsigned p = (unsigned)chunkIndex % (sizeof(palette) / sizeof(palette[0]));
    ps1DebugColor(palette[p][0], palette[p][1], palette[p][2]);
    if (ps1DebugFontId >= 0) { FntPrint(ps1DebugFontId, "DATA.WIN %d/%d %.4s", chunkIndex + 1, totalChunks, chunkName); FntFlush(-1); }
    logInfo("PS1 DataWin chunk %d/%d: %.4s\\n", chunkIndex + 1, totalChunks, chunkName);
}

static bool ps1LoadDataWin(DataWin** outDataWin) {
    DataWinParserOptions options = {0};
    /* First-boot subset: keep the REAL DataWin/VM path, but parse only the
       asset tables required to construct the first room and execute its GML.
       Large optional metadata (audio, shaders, extensions, paths, timelines,
       language tables) can be loaded later when the core loop is alive. */
    /* Chapter 1 boot subset: only keep DataWin chunks that the real
       Runner/VM path needs for rooms, sprites, objects, code and strings.
       Optional metadata is deliberately skipped on PS1 until Chapter 1
       proves that it needs it. */
    options.parseGen8 = true;
    options.parseOptn = false;
    options.parseLang = false;
    options.parseExtn = false;
    options.parseSond = false;
    options.parseAgrp = false;
    options.parseSprt = true;
    options.parseBgnd = true;
    options.parseScpt = true;
    options.parseGlob = true;
    options.parseShdr = false;
    options.parseFont = true;
    options.parseTmln = false;
    options.parseObjt = true;
    options.parseRoom = true;
    options.parseTpag = true;
    options.parseCode = true;
    options.parseVari = true;
    options.parseFunc = true;
    options.parseStrg = true;
    options.parseTxtr = false;
    options.parseAudo = false;
    options.skipLoadingPreciseMasksForNonPreciseSprites = true;
    options.lazyLoadRooms = true;
    options.lazyLoadTextures = true;
    options.lazyLoadAudio = true;
    options.loadType = DATAWINLOADTYPE_LOAD_PER_CHUNK;
    options.progressCallback = ps1ParseProgress;
    options.progressCallbackUserData = NULL;

    char* path = PS1Utils_createDevicePath("DATA.WIN");
    logInfo("Butterscotch PS1: loading real %s\n", path);
    DataWin* dataWin = DataWin_parse(path, options);
    free(path);

    if (dataWin == NULL) {
        logError("Butterscotch PS1: DataWin_parse failed\n");
        return false;
    }

    *outDataWin = dataWin;
    return true;
}

static void ps1ApplyMappedKey(Runner* runner, int32_t key, bool down) {
    if (down) RunnerKeyboard_onKeyDown(runner->keyboard, key);
    else RunnerKeyboard_onKeyUp(runner->keyboard, key);
}

/*
 * Deltarune on the original PC build reads keyboard-style GML input in a
 * number of paths. Keep the PS1 controller usable through the real Runner
 * keyboard state as well as the GameMaker gamepad API.
 *
 * Port 0 mapping:
 *   D-pad -> arrows
 *   Cross -> Z
 *   Square -> X
 *   Triangle -> C
 *   Start -> Enter
 *   Select -> Escape
 *
 * Analog sticks are also accepted as D-pad movement when they leave a small
 * deadzone, so a DualShock works without requiring the game's gamepad API.
 */
static void ps1ApplyDeltarunePadInput(Runner* runner, GamepadSlot* slot) {
    if (!runner || !slot) return;

    const float deadzone = 0.35f;
    const bool left  = slot->buttonDown[14] || slot->axisValue[0] < -deadzone;
    const bool right = slot->buttonDown[15] || slot->axisValue[0] >  deadzone;
    const bool up    = slot->buttonDown[12] || slot->axisValue[1] < -deadzone;
    const bool down  = slot->buttonDown[13] || slot->axisValue[1] >  deadzone;

    ps1ApplyMappedKey(runner, VK_LEFT, left);
    ps1ApplyMappedKey(runner, VK_RIGHT, right);
    ps1ApplyMappedKey(runner, VK_UP, up);
    ps1ApplyMappedKey(runner, VK_DOWN, down);
    ps1ApplyMappedKey(runner, 'Z', slot->buttonDown[0]);
    ps1ApplyMappedKey(runner, 'X', slot->buttonDown[2]);
    ps1ApplyMappedKey(runner, 'C', slot->buttonDown[3]);
    ps1ApplyMappedKey(runner, VK_ENTER, slot->buttonDown[9]);
    ps1ApplyMappedKey(runner, VK_ESCAPE, slot->buttonDown[8]);
}

static void ps1PollGamepads(Runner* runner) {
    if (runner == NULL || runner->gamepads == NULL) return;

    RunnerGamepad_beginFrame(runner->gamepads);
    Ps1Gamepad_poll(runner->gamepads, 0);
    Ps1Gamepad_poll(runner->gamepads, 1);

    /* The first controller is the gameplay controller. Release all mapped
       keys when it disconnects so a held direction cannot become stuck. */
    GamepadSlot* slot = &runner->gamepads->slots[0];
    if (slot->connected) {
        ps1ApplyDeltarunePadInput(runner, slot);
    } else if (slot->connectedPrev) {
        ps1ApplyMappedKey(runner, VK_LEFT, false);
        ps1ApplyMappedKey(runner, VK_RIGHT, false);
        ps1ApplyMappedKey(runner, VK_UP, false);
        ps1ApplyMappedKey(runner, VK_DOWN, false);
        ps1ApplyMappedKey(runner, 'Z', false);
        ps1ApplyMappedKey(runner, 'X', false);
        ps1ApplyMappedKey(runner, 'C', false);
        ps1ApplyMappedKey(runner, VK_ENTER, false);
        ps1ApplyMappedKey(runner, VK_ESCAPE, false);
    }
}

static void ps1RunFrame(Runner* runner, int32_t gameW, int32_t gameH) {
    Runner_step(runner);

    Runner_drawPre(runner, PS1_GAME_WIDTH, PS1_GAME_HEIGHT);
    Runner_beginFrame(runner, gameW, gameH,
                      PS1_GAME_WIDTH, PS1_GAME_HEIGHT,
                      PS1_GAME_WIDTH, PS1_GAME_HEIGHT);
    Runner_drawViews(runner, gameW, gameH, false);
    runner->viewCurrent = 0;
    runner->renderer->vtable->endFrameInit(runner->renderer);
    Runner_drawPost(runner, PS1_GAME_WIDTH, PS1_GAME_HEIGHT);
    runner->renderer->vtable->endFrameEnd(runner->renderer);
    Runner_drawGUI(runner, PS1_GAME_WIDTH, PS1_GAME_HEIGHT, gameW, gameH);

    RunnerKeyboard_beginFrame(runner->keyboard);

    if (runner->audioSystem != NULL && runner->audioSystem->vtable != NULL &&
        runner->audioSystem->vtable->update != NULL) {
        float dt = runner->currentRoom != NULL && runner->currentRoom->speed > 0
            ? 1.0f / (float) runner->currentRoom->speed
            : 0.0f;
        if (dt > 0.1f) dt = 0.1f;
        runner->audioSystem->vtable->update(runner->audioSystem, dt);
    }

    Ps1Renderer_present();
    Runner_handlePendingRoomChange(runner);
}

int main(void) {
    ps1DebugGpuInit();
    ps1DebugColor(255, 0, 0);      /* RED: before any platform/runtime init */

    PS1Utils_init();
    Ps1Gamepad_init();
    ps1DebugColor(255, 128, 0);    /* ORANGE: CD/gamepad init OK */

    DataWin* dataWin = NULL;
    if (!ps1LoadDataWin(&dataWin)) {
        while (true) VSync(0);
    }
    ps1DebugColor(255, 255, 0);    /* YELLOW: real DataWin_parse OK */

    logInfo("Butterscotch PS1: WAD version %u, game=%s\n",
            dataWin->gen8.wadVersion,
            dataWin->gen8.displayName);

    /* Same architectural order as the PS2 backend, with PS1-specific
       implementations substituted at the platform boundaries. */
    FileSystem* fileSystem = Ps1FileSystem_create(NULL, dataWin->gen8.displayName);
    if (fileSystem == NULL) {
        logError("Butterscotch PS1: failed to create PS1 filesystem\n");
        DataWin_free(dataWin);
        while (true) VSync(0);
    }

    VMContext* vm = VM_create(dataWin);
    if (vm == NULL) {
        logError("Butterscotch PS1: VM_create failed\n");
        Ps1FileSystem_destroy(fileSystem);
        DataWin_free(dataWin);
        while (true) VSync(0);
    }
    ps1DebugColor(0, 255, 255);    /* CYAN: VM_create OK */

    Renderer* renderer = Ps1Renderer_create();
    if (renderer == NULL) {
        logError("Butterscotch PS1: Ps1Renderer_create failed\n");
        Ps1FileSystem_destroy(fileSystem);
        DataWin_free(dataWin);
        while (true) VSync(0);
    }

    /* The core renderer remains the same PS1 backend. These hooks only replace
       the two high-frequency room paths that were still placeholders. */
    Ps1FastRenderer_install(renderer);
    ps1DebugColor(0, 0, 255);      /* BLUE: renderer setup OK */

    AudioSystem* audioSystem = (AudioSystem*) NoopAudioSystem_create();
    Runner* runner = Runner_create(dataWin, vm, renderer, fileSystem, audioSystem, 0);
    if (runner == NULL) {
        logError("Butterscotch PS1: Runner_create failed\n");
        renderer->vtable->destroy(renderer);
        Ps1FileSystem_destroy(fileSystem);
        DataWin_free(dataWin);
        while (true) VSync(0);
    }
    ps1DebugColor(255, 0, 255);    /* MAGENTA: Runner_create OK */

    logInfo("Butterscotch PS1: initializing first real room\n");
    Runner_initFirstRoom(runner);
    ps1DebugColor(255, 255, 255);  /* WHITE: first real room OK; entering game loop */

    const int32_t gameW = (int32_t) dataWin->gen8.defaultWindowWidth;
    const int32_t gameH = (int32_t) dataWin->gen8.defaultWindowHeight;
    uint64_t lastFrame = nowNanos();

    while (!runner->shouldExit) {
        ps1PollGamepads(runner);

        uint64_t now = nowNanos();
        uint64_t elapsed = now - lastFrame;
        if (elapsed < PS1_FRAME_NS) {
            VSync(0);
            continue;
        }
        lastFrame = now;

        /* Butterscotch's real loop stores deltaTime in microseconds. */
        runner->deltaTime = (double) elapsed / 1000.0;
        ps1RunFrame(runner, gameW, gameH);
    }

    if (runner->audioSystem != NULL && runner->audioSystem->vtable != NULL &&
        runner->audioSystem->vtable->destroy != NULL) {
        runner->audioSystem->vtable->destroy(runner->audioSystem);
    }
    renderer->vtable->destroy(renderer);
    Ps1FileSystem_destroy(fileSystem);
    DataWin_free(dataWin);
    return 0;
}