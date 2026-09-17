#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "../utils.h"
#include <psxcd.h>

void PS1Utils_init(void) {
    CdInit();
}

char* PS1Utils_createDevicePath(const char* path) {
    const char* p = path;
    while (*p == '\\' || *p == '/') p++;

    size_t pathLen = strlen(p);
    char* devicePath = (char*)safeMalloc(pathLen + 8);
    snprintf(devicePath, pathLen + 8, "\\%s;1", p);
    return devicePath;
}
