#ifndef PS1_COMPAT_STDLIB_H
#define PS1_COMPAT_STDLIB_H
#include_next <stdlib.h>
#include <stddef.h>

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
char *getenv(const char *name);
double atof(const char *s);
double strtod(const char *nptr, char **endptr);
void exit(int status);

#endif
