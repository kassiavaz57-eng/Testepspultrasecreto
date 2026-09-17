#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "../utils.h"

char* PS1Utils_createDevicePath(const char* path) {
    size_t pathLen = strlen(path);
    char* devicePath = (char*)safeMalloc(pathLen + 8);
    snprintf(devicePath, pathLen + 8, "\\%s;1", path);
    return devicePath;
}
