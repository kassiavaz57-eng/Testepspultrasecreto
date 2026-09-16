#ifndef PS1_COMPAT_UNISTD_H
#define PS1_COMPAT_UNISTD_H
#define _SC_PAGESIZE 30
static inline long sysconf(int name) { (void)name; return 2048; }
#endif
