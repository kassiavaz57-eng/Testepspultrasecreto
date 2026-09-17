#include <string.h>
#include <stddef.h>
#include <stdint.h>

static int ps1_ascii_tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int strcasecmp(const char *a, const char *b) {
    unsigned char ca, cb;
    while (a && b && *a && *b) { ca=(unsigned char)ps1_ascii_tolower((unsigned char)*a++); cb=(unsigned char)ps1_ascii_tolower((unsigned char)*b++); if(ca!=cb)return ca<cb?-1:1; }
    if(!a||!b)return a==b?0:(a?1:-1);
    ca=(unsigned char)ps1_ascii_tolower((unsigned char)*a); cb=(unsigned char)ps1_ascii_tolower((unsigned char)*b);
    return ca==cb?0:(ca<cb?-1:1);
}
int isalnum(int c) { return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'); }
void *bsearch(const void *key,const void *base,size_t nmemb,size_t size,int(*compar)(const void*,const void*)) {
    size_t lo=0,hi=nmemb; const unsigned char *p=(const unsigned char*)base;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;int r=compar(key,p+mid*size);if(!r)return(void*)(p+mid*size);if(r<0)hi=mid;else lo=mid+1;}return NULL;
}
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *p = (unsigned char *)base;
    unsigned char tmp[64];
    if (!base || !compar || size == 0 || size > sizeof(tmp)) return;
    for (size_t i = 1; i < nmemb; ++i) {
        size_t j = i;
        while (j > 0 && compar(p + (j - 1) * size, p + j * size) > 0) {
            memcpy(tmp, p + (j - 1) * size, size);
            memcpy(p + (j - 1) * size, p + j * size, size);
            memcpy(p + j * size, tmp, size);
            --j;
        }
    }
}
char *getenv(const char *name) { (void)name; return NULL; }
double strtod(const char *nptr, char **endptr);
double atof(const char *s) { return strtod(s, NULL); }
double strtod(const char *nptr,char **endptr) {
    const char *p=nptr; int sign=1; double v=0.0,scale=0.1;
    while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r')++p;
    if(*p=='+'||*p=='-'){if(*p=='-')sign=-1;++p;}
    while(*p>='0'&&*p<='9'){v=v*10.0+(*p-'0');++p;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){v+=(*p-'0')*scale;scale*=0.1;++p;}}
    if(endptr)*endptr=(char*)p; return sign*v;
}
/* PS1 has no hosted process to return to; Butterscotch uses exit() only for fatal paths. */
void exit(int status) {
    (void)status;
    for (;;) { }
}
