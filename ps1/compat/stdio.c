#include "stdio.h"
#include <string.h>
#include <stdlib.h>

static int ps1_file_initialized;

static void ps1_file_init(void) {
    if (!ps1_file_initialized) {
        CdInit();
        ps1_file_initialized = 1;
    }
}

static int ps1_load_sector(FILE *f, uint32_t sector) {
    CdlLOC loc;
    ps1_file_init();
    if (sector * 2048u >= f->size) return 0;
    CdIntToPos(CdPosToInt(&f->cd.pos) + (int)sector, &loc);
    if (!CdControl(CdlSetloc, (uint8_t *)&loc, 0)) return 0;
    if (!CdRead(1, (uint32_t *)f->sector, CdlModeSpeed)) return 0;
    if (CdReadSync(0, 0) < 0) return 0;
    f->sectorBase = sector * 2048u;
    f->sectorValid = 1;
    return 1;
}

FILE *fopen(const char *path, const char *mode) {
    FILE *f;
    if (!path || !mode || mode[0] != 'r') return NULL;
    f = (FILE *)malloc(sizeof(FILE));
    if (!f) return NULL;
    memset(f, 0, sizeof(FILE));
    ps1_file_init();

    if (!CdSearchFile(&f->cd, path)) {
        if (path[0] != '\\') {
            char p[128];
            size_t n = strlen(path);
            if (n > sizeof(p) - 4) { free(f); return NULL; }
            p[0] = '\\';
            memcpy(p + 1, path, n);
            p[n + 1] = ';';
            p[n + 2] = '1';
            p[n + 3] = 0;
            if (!CdSearchFile(&f->cd, p)) { free(f); return NULL; }
        } else {
            free(f);
            return NULL;
        }
    }

    f->size = f->cd.size;
    f->pos = 0;
    f->mode = 0;
    return f;
}

int fclose(FILE *f) {
    if (!f) return EOF;
    free(f);
    return 0;
}

int fseek(FILE *f, long offset, int whence) {
    int64_t p;
    if (!f) return -1;
    p = (whence == SEEK_SET) ? offset :
        (whence == SEEK_CUR) ? (int64_t)f->pos + offset :
        (whence == SEEK_END) ? (int64_t)f->size + offset : -1;
    if (p < 0) return -1;
    if ((uint64_t)p > f->size) p = f->size;
    f->pos = (uint32_t)p;
    f->sectorValid = 0;
    return 0;
}

long ftell(FILE *f) {
    return f ? (long)f->pos : -1;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *f) {
    size_t want, done = 0;
    uint8_t *out = (uint8_t *)ptr;
    if (!f || !ptr || size == 0 || count == 0) return 0;
    want = size * count;
    if (want > f->size - f->pos) want = f->size - f->pos;
    while (done < want) {
        uint32_t sector = f->pos / 2048u;
        uint32_t off = f->pos & 2047u;
        size_t n;
        if (!f->sectorValid || f->sectorBase != sector * 2048u) {
            if (!ps1_load_sector(f, sector)) break;
        }
        n = 2048u - off;
        if (n > want - done) n = want - done;
        memcpy(out + done, f->sector + off, n);
        f->pos += (uint32_t)n;
        done += n;
    }
    return done / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *f) {
    (void)ptr; (void)f;
    return size && count ? count : 0;
}

int fputc(int c, FILE *f) {
    (void)c; (void)f;
    return 0;
}

int setvbuf(FILE *f, char *buf, int mode, size_t size) {
    (void)f; (void)buf; (void)mode; (void)size;
    return 0;
}

int fileno(FILE *f) {
    (void)f;
    return -1;
}

int fflush(FILE *f) {
    (void)f;
    return 0;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    (void)f; (void)fmt; (void)ap;
    return 0;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int fputs(const char *s, FILE *f) {
    (void)s; (void)f;
    return 0;
}
