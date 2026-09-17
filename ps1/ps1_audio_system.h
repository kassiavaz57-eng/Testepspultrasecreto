#ifndef _BS_PS1_AUDIO_SYSTEM_H_
#define _BS_PS1_AUDIO_SYSTEM_H_

#include "common.h"
#include "audio_system.h"
#include <stdint.h>
#include <stdbool.h>
#include "stdio_compat.h"

/* Keep the Butterscotch audio resource model; only the hardware backend changes. */
#define MAX_PS1_SOUND_INSTANCES 32
#define PS1_SOUND_INSTANCE_ID_BASE 100000
#define PS1_AUDIO_CACHE_SIZE 16
#define PS1_MIX_BUFFER_SAMPLES 256
#define PS1_OUTPUT_FREQ 22050
#define MAX_PS1_MUSIC_STREAMS 2
#define PS1_AUDIO_STREAM_INDEX_BASE 300000
#define PS1_STREAM_ADPCM_CHUNK_BYTES 2048
#define PS1_STREAM_DECODE_SAMPLES (PS1_STREAM_ADPCM_CHUNK_BYTES * 2)
#define PS1_SFX_CACHE_MAX_BYTES (128 * 1024)

typedef struct {
    uint16_t audoIndex;
    uint32_t flags;
    int16_t volume;
    int16_t pitch;
} Ps1SondEntry;

typedef struct {
    uint32_t dataOffset;
    uint32_t dataSize;
    uint16_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    uint8_t format;
    uint32_t sampleCount;
} Ps1AudoEntry;

typedef struct {
    char* name;
    uint32_t dataOffset;
    uint32_t dataSize;
    uint16_t sampleRate;
    uint8_t channels;
    uint8_t format;
    uint32_t sampleCount;
} Ps1MusEntry;

typedef struct {
    int32_t audoIndex;
    int16_t* pcmData;
    uint32_t pcmSampleCount;
    uint32_t pcmDataBytes;
    uint32_t lastAccessCounter;
} Ps1DecodedPcmEntry;

typedef struct {
    bool active;
    int32_t soundIndex;
    int32_t audoIndex;
    int32_t instanceId;
    int32_t priority;
    bool loop;
    bool paused;
    uint32_t positionInt;
    uint32_t positionFrac;
    uint32_t totalSamples;
    float pitch;
    float sondPitch;
    float currentGain;
    float targetGain;
    float startGain;
    float fadeTimeRemaining;
    float fadeTotalTime;
    float sondVolume;
} Ps1SoundInstance;

typedef struct {
    bool active;
    int32_t soundIndex;
    int32_t audoIndex;
    int32_t instanceId;
    int32_t priority;
    bool loop;
    bool paused;
    float currentGain;
    float targetGain;
    float startGain;
    float fadeTimeRemaining;
    float fadeTotalTime;
    float sondVolume;
    float pitch;
    float sondPitch;
    uint32_t fileOffset;
    uint32_t fileStartOffset;
    uint32_t fileEndOffset;
    int32_t decoderPredictor;
    int32_t decoderStepIndex;
    int16_t buffers[2][PS1_STREAM_DECODE_SAMPLES];
    uint32_t bufferSampleCount[2];
    int activeBuffer;
    uint32_t readPosition;
    uint32_t readPositionFrac;
    bool needsRefill;
    bool endOfTrack;
} Ps1MusicStream;

typedef struct {
    AudioSystem base;
    uint16_t sondEntryCount;
    uint16_t audoEntryCount;
    uint16_t musEntryCount;
    Ps1SondEntry* sondEntries;
    Ps1AudoEntry* audoEntries;
    Ps1MusEntry* musEntries;
    FILE* soundsFile;
    Ps1DecodedPcmEntry cacheEntries[PS1_AUDIO_CACHE_SIZE];
    uint32_t cacheAccessCounter;
    Ps1SoundInstance instances[MAX_PS1_SOUND_INSTANCES];
    int32_t nextInstanceCounter;
    Ps1MusicStream musicStreams[MAX_PS1_MUSIC_STREAMS];
    int16_t mixBuffer[PS1_MIX_BUFFER_SAMPLES * 2];
    int32_t mixAccum[PS1_MIX_BUFFER_SAMPLES];
    float masterGain;
    bool initialized;
} Ps1AudioSystem;

Ps1AudioSystem* Ps1AudioSystem_create(void);

#endif /* _BS_PS1_AUDIO_SYSTEM_H_ */
