#ifndef PS1_COMPAT_TIME_H
#define PS1_COMPAT_TIME_H
#include <stdint.h>
#include <psxetc.h>
#include <psxapi.h>

#define CLOCK_MONOTONIC 1

struct timespec {
    long tv_sec;
    long tv_nsec;
};

static inline int clock_gettime(int clock_id, struct timespec *ts) {
    static int initialized = 0;
    static uint32_t last = 0;
    static uint64_t high = 0;
    uint32_t now;
    uint64_t ticks;
    (void)clock_id;
    if (!ts) return -1;
    if (!initialized) {
        SetRCnt(RCntCNT2, 0xffff, RCntMdNOINTR | RCntMdFR);
        StartRCnt(RCntCNT2);
        last = (uint32_t)GetRCnt(RCntCNT2);
        initialized = 1;
    }
    now = (uint32_t)GetRCnt(RCntCNT2);
    if (now < last) high += 65536u;
    last = now;
    ticks = high + now;
    /* Root counter 2 runs from the system clock divided by 8: ~4.5 MHz. */
    ts->tv_sec = (long)(ticks / 4515840ull);
    ts->tv_nsec = (long)(((ticks % 4515840ull) * 1000000000ull) / 4515840ull);
    return 0;
}
#endif
