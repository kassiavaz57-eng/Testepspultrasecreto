#ifndef _BS_PS1_UTILS_H_
#define _BS_PS1_UTILS_H_

#include "common.h"
#include <psxetc.h>
#include <stdint.h>

#define GS_VRAM_SIZE (4 * 1024 * 1024)

// Clamp alpha to 0.0-1.0, then scale to PS1 GS range (0-128).
// Without clamping, values > 1.0 cause uint8_t overflow/wrapping, making fades repeat.
static inline uint8_t alphaToGS(float alpha) {
    if (alpha > 1.0f) alpha = 1.0f;
    else if (0.0f > alpha) alpha = 0.0f;
    return (uint8_t) (alpha * 128.0f);
}

typedef struct {
    char* key;
    bool usesISO9660;
} PS1DeviceKey;

extern PS1DeviceKey deviceKey;
extern bool deviceKeyLoaded;

void PS1Utils_extractDeviceKey(const char* path);
void PS1Utils_loadFSDrivers();
char* PS1Utils_createDevicePath(const char* path);

#ifdef GPROF_PROFILING
// Loads USB mass storage IOP drivers (usbd, bdm, bdmfs_fatfs, usbmass_bd)
// so gprof can write gmon.out to mass: when not running from host:
void PS1Utils_loadMassStorageDrivers();
#endif

#endif /* _BS_PS1_UTILS_H_ */
