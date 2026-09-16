#ifndef PS1_COMPAT_TIME_H
#define PS1_COMPAT_TIME_H
#include <stdint.h>
#include <psxetc.h>
#include <psxapi.h>

#define CLOCK_MONOTONIC 1

typedef long time_t;

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

static inline uint64_t ps1_compat_ticks(void) {
    static int initialized = 0;
    static uint32_t last = 0;
    static uint64_t high = 0;
    uint32_t now;
    if (!initialized) {
        SetRCnt(RCntCNT2, 0xffff, RCntMdNOINTR | RCntMdFR);
        StartRCnt(RCntCNT2);
        last = (uint32_t)GetRCnt(RCntCNT2);
        initialized = 1;
    }
    now = (uint32_t)GetRCnt(RCntCNT2);
    if (now < last) high += 65536u;
    last = now;
    return high + now;
}

static inline int clock_gettime(int clock_id, struct timespec *ts) {
    uint64_t ticks;
    (void)clock_id;
    if (!ts) return -1;
    ticks = ps1_compat_ticks();
    ts->tv_sec = (long)(ticks / 4515840ull);
    ts->tv_nsec = (long)(((ticks % 4515840ull) * 1000000000ull) / 4515840ull);
    return 0;
}

/*
 * PS1 has no POSIX wall-clock implementation. These provide the small
 * time() / localtime() API used by the real Butterscotch VM builtins.
 * The returned epoch is monotonic from program start; calendar fields are
 * therefore a deterministic UTC-like fallback rather than host wall time.
 */
static inline time_t time(time_t *out) {
    time_t value = (time_t)(ps1_compat_ticks() / 4515840ull);
    if (out) *out = value;
    return value;
}

static inline struct tm *localtime(const time_t *value) {
    static struct tm result;
    time_t seconds = value ? *value : 0;
    int64_t days = seconds / 86400;
    int64_t day_seconds = seconds % 86400;
    if (day_seconds < 0) { day_seconds += 86400; --days; }

    result.tm_sec = (int)(day_seconds % 60);
    result.tm_min = (int)((day_seconds / 60) % 60);
    result.tm_hour = (int)(day_seconds / 3600);
    result.tm_mday = (int)(days % 31) + 1;
    result.tm_mon = 0;
    result.tm_year = 70;
    result.tm_wday = (int)((days + 4) % 7);
    if (result.tm_wday < 0) result.tm_wday += 7;
    result.tm_yday = (int)(days % 365);
    if (result.tm_yday < 0) result.tm_yday += 365;
    result.tm_isdst = 0;
    return &result;
}

#endif
