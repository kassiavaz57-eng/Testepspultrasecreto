#include "ps1_spu_adpcm.h"

static const int16_t imaStepTable[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,
    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794,32767
};

static const int8_t imaIndexTable[16] = {
    -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};

uint32_t Ps1ImaDecode(const uint8_t* input, uint32_t inputBytes, int16_t* output, int32_t* predictor, int32_t* stepIndex) {
    if (!input || !output || !predictor || !stepIndex) return 0;
    uint32_t written = 0;
    for (uint32_t i = 0; i < inputBytes; i++) {
        uint8_t byte = input[i];
        for (int n = 0; n < 2; n++) {
            uint8_t code = (n == 0) ? (byte & 0x0f) : (byte >> 4);
            int32_t step = imaStepTable[*stepIndex];
            int32_t delta = step >> 3;
            if (code & 1) delta += step >> 2;
            if (code & 2) delta += step >> 1;
            if (code & 4) delta += step;
            if (code & 8) delta = -delta;
            *predictor += delta;
            if (*predictor > 32767) *predictor = 32767;
            if (*predictor < -32768) *predictor = -32768;
            output[written++] = (int16_t)*predictor;
            *stepIndex += imaIndexTable[code & 15];
            if (*stepIndex < 0) *stepIndex = 0;
            if (*stepIndex > 88) *stepIndex = 88;
        }
    }
    return written;
}

static const int filterF0[5] = {0, 60, 115, 98, 122};
static const int filterF1[5] = {0, 0, -52, -55, -60};

static int clamp16(int x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return x;
}

static int predictSample(int s1, int s2, int filter) {
    return (s1 * filterF0[filter] + s2 * filterF1[filter] + 32) >> 6;
}

static int quantizeNibble(int residual, int shift) {
    if (shift == 0) {
        if (residual > 7) return 7;
        if (residual < -8) return -8;
        return residual;
    }
    int bias = 1 << (shift - 1);
    int q = (residual + (residual >= 0 ? bias : -bias)) >> shift;
    if (q > 7) q = 7;
    if (q < -8) q = -8;
    return q;
}

static int chooseShift(const int16_t* samples, int count, int filter, int s1, int s2) {
    int maxAbs = 0;
    for (int i = 0; i < count; i++) {
        int predicted = predictSample(s1, s2, filter);
        int residual = (int)samples[i] - predicted;
        int a = residual < 0 ? -residual : residual;
        if (a > maxAbs) maxAbs = a;
        s2 = s1;
        s1 = samples[i];
    }
    int shift = 0;
    while (shift < 12 && (maxAbs >> shift) > 7) shift++;
    return shift;
}

static int blockError(const int16_t* samples, int count, int filter, int shift, int s1, int s2) {
    int error = 0;
    for (int i = 0; i < count; i++) {
        int predicted = predictSample(s1, s2, filter);
        int residual = (int)samples[i] - predicted;
        int q = quantizeNibble(residual, shift);
        int reconstructed = clamp16(predicted + (q << shift));
        int diff = (int)samples[i] - reconstructed;
        error += diff < 0 ? -diff : diff;
        s2 = s1;
        s1 = reconstructed;
    }
    return error;
}

uint32_t Ps1SpuEncode(const int16_t* pcm, uint32_t samples, uint8_t* output, uint32_t outputCapacity, bool loop) {
    if (!pcm || !output || samples == 0) return 0;
    uint32_t blocks = (samples + 27) / 28;
    uint32_t bytes = blocks * 16;
    if (outputCapacity < bytes) return 0;

    int s1 = 0;
    int s2 = 0;
    for (uint32_t block = 0; block < blocks; block++) {
        int16_t temp[28];
        uint32_t remaining = samples - block * 28;
        int count = remaining > 28 ? 28 : (int)remaining;
        for (int i = 0; i < 28; i++) temp[i] = (i < count) ? pcm[block * 28 + i] : 0;

        int bestFilter = 0;
        int bestShift = 0;
        int bestError = 0x7fffffff;
        for (int filter = 0; filter < 5; filter++) {
            int shift = chooseShift(temp, 28, filter, s1, s2);
            int error = blockError(temp, 28, filter, shift, s1, s2);
            if (error < bestError) {
                bestError = error;
                bestFilter = filter;
                bestShift = shift;
            }
        }

        uint8_t* dst = output + block * 16;
        uint8_t flags = 0;
        if (loop && block == 0) flags |= 2;
        if (block == blocks - 1) flags |= loop ? 3 : 1;
        dst[0] = (uint8_t)(bestShift | (bestFilter << 4));
        dst[1] = flags;

        for (int i = 0; i < 14; i++) {
            int q0 = quantizeNibble((int)temp[i * 2] - predictSample(s1, s2, bestFilter), bestShift);
            int r0 = clamp16(predictSample(s1, s2, bestFilter) + (q0 << bestShift));
            s2 = s1; s1 = r0;
            int q1 = quantizeNibble((int)temp[i * 2 + 1] - predictSample(s1, s2, bestFilter), bestShift);
            int r1 = clamp16(predictSample(s1, s2, bestFilter) + (q1 << bestShift));
            s2 = s1; s1 = r1;
            dst[2 + i] = (uint8_t)((q0 & 0x0f) | ((q1 & 0x0f) << 4));
        }
    }
    return bytes;
}
