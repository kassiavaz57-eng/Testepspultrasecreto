#ifndef _BS_PS1_UTILS_H_
#define _BS_PS1_UTILS_H_

#include "common.h"
#include <stdint.h>

/* PS1 has no PS2-style device/IOP layer in this port. Resources are read
   through the PSn00bSDK CD-ROM stream exposed by stdio_compat. */
static inline uint8_t PS1Utils_alphaToGpu(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    return (uint8_t)(alpha * 255.0f);
}

/* Converts a logical asset name to the ISO9660 path used by the PS1 CD stream. */
char* PS1Utils_createDevicePath(const char* path);

#endif /* _BS_PS1_UTILS_H_ */
