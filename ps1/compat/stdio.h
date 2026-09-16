#ifndef PS1_COMPAT_STDIO_H
#define PS1_COMPAT_STDIO_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <psxcd.h>
typedef struct {
    CdlFILE cd;
    uint32_t pos;
    uint32_t size;
    uint8_t sector[2048];
    uint32_t sectorBase;
    uint8_t sectorValid;
    uint8_t mode;
} FILE;
#define _IOFBF 0
#define EOF (-1)
#define stdin ((FILE*)0)
#define stdout ((FILE*)0)
#define stderr ((FILE*)0)
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
size_t fread(void *ptr, size_t size, size_t count, FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *f);
int fputc(int c, FILE *f);
int setvbuf(FILE *f, char *buf, int mode, size_t size);
int fileno(FILE *f);
int fflush(FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int fputs(const char *s, FILE *f);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
#endif
