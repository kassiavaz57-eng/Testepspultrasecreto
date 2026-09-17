#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <psxetc.h>

#include "runner.h"
#include "runner_keyboard.h"
#include "runner_gamepad.h"
#include "../data_win.h"
#include "../vm.h"
#include "../noop_audio_system.h"
#include "ps1_utils.h"
#include "ps1_file_system.h"
#include "ps1_renderer.h"
#include "ps1_gamepad.h"
#include "log.h"
#include "gettime.h"

#define PS1_GAME_WIDTH 320
#define PS1_GAME_HEIGHT 240
#define PS1_FRAME_NS 16666667ULL

static bool ps1LoadDataWin(DataWin** outDataWin) {
    DataWinParserOptions options = {0};
    options.parseGen8 = true;
    options.parseOptn = true;
    options.parseLang = true;
    options.parseExtn = true;
    options.parseSond = true;
    options.parseAgrp = true;
    options.parseSprt = true;
    options.parseBgnd = true;
    options.parsePath = true;
    options.parseScpt = true;
    options.parseGlob = true;
    options.parseShdr = true;
    options.parseFont = true;
    options.parseTmln = true;
    options.parseObjt = true;
    options.parseRoom = true;
    options.parseTpag = true;
    options.parseCode = true;
    options.parseVari = true;
    options.parseFunc = true;
    options.parseStrg = true;

    /* Keep the real DataWin parser while avoiding eager materialization of the
       largest asset sections on the PS1's very small RAM budget. */
    options.parseTxtr = false;
    options.parseAudo = false;
    options.skipLoadingPreciseMasksForNonPreciseSprites = true;
    options.lazyLoadRooms = true;

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

static void ps1PollGamepads(Runner* runner) {
    if (runner == NULL || runner->gamepads == NULL) return;
    RunnerGamepad_beginFrame(runner->gamepads);
    Ps1Gamepad_poll(runner->gamepads, 0);
    Ps1Gamepad_poll(runner->gamepads, 1);
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
    PS1Utils_init();
    Ps1Gamepad_init();

    DataWin* dataWin = NULL;
    if (!ps1LoadDataWin(&dataWin)) {
        while (true) VSync(0);
    }

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

    Renderer* renderer = Ps1Renderer_create();
    if (renderer == NULL) {
        logError("Butterscotch PS1: Ps1Renderer_create failed\n");
        Ps1FileSystem_destroy(fileSystem);
        DataWin_free(dataWin);
        while (true) VSync(0);
    }

    AudioSystem* audioSystem = (AudioSystem*) NoopAudioSystem_create();
    Runner* runner = Runner_create(dataWin, vm, renderer, fileSystem, audioSystem, 0);
    if (runner == NULL) {
        logError("Butterscotch PS1: Runner_create failed\n");
        renderer->vtable->destroy(renderer);
        Ps1FileSystem_destroy(fileSystem);
        DataWin_free(dataWin);
        while (true) VSync(0);
    }

    logInfo("Butterscotch PS1: initializing first real room\n");
    Runner_initFirstRoom(runner);

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
