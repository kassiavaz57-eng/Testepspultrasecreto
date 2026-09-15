    Runner_step(runner);
    int32_t gameW=(int32_t)d->gen8.defaultWindowWidth, gameH=(int32_t)d->gen8.defaultWindowHeight;
    // The PSP renderer owns a live GU display list. Start the frame before any
    // draw call; unlike the PS2 queue renderer, drawPre cannot run before GU start.
    Runner_beginFrame(runner,gameW,gameH,480,272,480,272);
    Runner_drawPre(runner,480,272);
    Runner_drawViews(runner,gameW,gameH,false);
    runner->renderer->vtable->endFrameInit(runner->renderer);
    Runner_drawPost(runner,480,272);
    runner->renderer->vtable->endFrameEnd(runner->renderer);
    Runner_drawGUI(runner,480,272,gameW,gameH);
    RunnerKeyboard_beginFrame(runner->keyboard);
    sceGuFinish(); sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
sceKernelExitGame();
return 0;
}