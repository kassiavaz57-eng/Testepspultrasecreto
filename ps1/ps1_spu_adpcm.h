#ifndef _BS_PS1_SPU_ADPCM_H_
#define _BS_PS1_SPU_ADPCM_H_

#include <stdint.h>
#include <stdbool.h>

/* Decode the same IMA-ADPCM stream used by Butterscotch's PS2 backend. */
uint32_t Ps1ImaDecode(const uint8_t* input, uint32_t inputBytes, int16_t* output, int32_t* predictor, int32_t* stepIndex);

/* Encode mono signed 16-bit PCM into Sony/PS1 SPU-ADPCM blocks.
 * Returns the number of output bytes, or 0 if the destination is too small. */
uint32_t Ps1SpuEncode(const int16_t* pcm, uint32_t samples, uint8_t* output, uint32_t outputCapacity, bool loop);

#endif /* _BS_PS1_SPU_ADPCM_H_ */
