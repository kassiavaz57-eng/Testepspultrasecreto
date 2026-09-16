#ifndef _BS_PSP_AUDIO_SYSTEM_H_
#define _BS_PSP_AUDIO_SYSTEM_H_

#include "audio_system.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    AudioSystem base;
    int channel;
    int sampleRate;
    int channels;
    volatile int running;
    volatile int stopRequested;
    volatile int instanceId;
    volatile int32_t soundIndex;
    volatile float gain;
    short* pcm;
    int totalFrames;
    int position;
    int pspThread;
    int mutex;
} PspAudioSystem;

PspAudioSystem* PspAudioSystem_create(void);

#endif
