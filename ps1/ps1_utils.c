#include "ps1_utils.h"
#include "stdio_compat.h"
#include "string_compat.h"
#include "../utils.h"
#include <psxcd.h>
#include <psxapi.h>

/* Keep the heap below the top-of-RAM stack with a safety margin. */
#define PS1_STACK_TOP     0x801FFFF0u
#define PS1_STACK_RESERVE (16u * 1024u)

extern uint8_t _end[];

void PS1Utils_init(void) {
    uint32_t heapStart = (uint32_t)_end;
    uint32_t heapSize = PS1_STACK_TOP - PS1_STACK_RESERVE - heapStart;

    InitHeap((void*)heapStart, heapSize);
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
