#include "psp_file_system.h"
#include "utils.h"
#include "stb_ds.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct{FileSystem base;char*root;}PspFS;
static char*path_for(PspFS*f,const char*r){size_t n=strlen(f->root)+strlen(r)+2;char*p=safeMalloc(n);snprintf(p,n,"%s/%s",f->root,r);return p;}
static char*resolve(FileSystem*f,const char*r){return path_for((PspFS*)f,r);}
static bool exists(FileSystem*f,const char*r){char*p=path_for((PspFS*)f,r);struct stat s;bool ok=stat(p,&s)==0&&S_ISREG(s.st_mode);free(p);return ok;}
static bool readbin(FileSystem*f,const char*r,uint8_t**out,int32_t*n){char*p=path_for((PspFS*)f,r);FILE*h=fopen(p,"rb");free(p);if(!h)return false;if(fseek(h,0,SEEK_END)){fclose(h);return false;}long z=ftell(h);if(z<0||z>INT32_MAX){fclose(h);return false;}rewind(h);uint8_t*d=safeMalloc((size_t)z);size_t got=fread(d,1,(size_t)z,h);fclose(h);if(got!=(size_t)z){free(d);return false;}*out=d;*n=(int32_t)z;return true;}
static bool writebin(FileSystem*f,const char*r,const uint8_t*d,int32_t n){char*p=path_for((PspFS*)f,r);FILE*h=fopen(p,"wb");if(!h){free(p);return false;}size_t w=fwrite(d,1,(size_t)n,h);fclose(h);free(p);return w==(size_t)n;}
static char*readtext(FileSystem*f,const char*r){uint8_t*d;int32_t n;if(!readbin(f,r,&d,&n))return NULL;char*s=safeMalloc((size_t)n+1);memcpy(s,d,(size_t)n);s[n]='\0';free(d);return s;}
static bool writetext(FileSystem*f,const char*r,const char*s){return writebin(f,r,(const uint8_t*)s,(int32_t)strlen(s));}
static bool del(FileSystem*f,const char*r){char*p=path_for((PspFS*)f,r);bool ok=remove(p)==0;free(p);return ok;}
static void*openbin(FileSystem*f,const char*r,int32_t mode){char*p=path_for((PspFS*)f,r);const char*m=mode==GML_FILE_BIN_READ?"rb":mode==GML_FILE_BIN_WRITE?"wb":mode==GML_FILE_BIN_READWRITE?"r+b":NULL;if(!m){free(p);return NULL;}FILE*h=fopen(p,m);if(!h&&mode==GML_FILE_BIN_READWRITE)h=fopen(p,"w+b");free(p);return h;}
static void closebin(FileSystem*f,void*h){(void)f;if(h)fclose(h);}static int32_t readstream(FileSystem*f,void*h,void*d,int32_t n){(void)f;return(int32_t)fread(d,1,(size_t)n,(FILE*)h);}static int32_t writestream(FileSystem*f,void*h,const void*d,int32_t n){(void)f;return(int32_t)fwrite(d,1,(size_t)n,(FILE*)h);}static int32_t tell(FileSystem*f,void*h){(void)f;long p=ftell((FILE*)h);return p<0?-1:(int32_t)p;}static bool seek(FileSystem*f,void*h,int32_t p){(void)f;return fseek((FILE*)h,p,SEEK_SET)==0;}static int32_t size(FileSystem*f,void*h){(void)f;FILE*x=(FILE*)h;long p=ftell(x);if(p<0||fseek(x,0,SEEK_END))return-1;long z=ftell(x);fseek(x,p,SEEK_SET);return z<0||z>INT32_MAX?-1:(int32_t)z;}static void rewrite(FileSystem*f,void*h){(void)f;int fd=fileno((FILE*)h);if(fd>=0)ftruncate(fd,0);fseek((FILE*)h,0,SEEK_SET);}
static bool dexists(FileSystem*f,const char*r){char*p=path_for((PspFS*)f,r);struct stat s;bool ok=stat(p,&s)==0&&S_ISDIR(s.st_mode);free(p);return ok;}static bool mkdirx(FileSystem*f,const char*r){char*p=path_for((PspFS*)f,r);bool ok=mkdir(p,0777)==0||errno==EEXIST;free(p);return ok;}static bool rmdirx(FileSystem*f,const char*r){char*p=path_for((PspFS*)f,r);bool ok=rmdir(p)==0;free(p);return ok;}
static FileSystemDirEntry*listdir(FileSystem*f,const char*r){PspFS*x=(PspFS*)f;char*p=path_for(x,r);DIR*d=opendir(p);free(p);if(!d)return NULL;FileSystemDirEntry*out=NULL;struct dirent*e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;FileSystemDirEntry v={0};v.name=safeStrdup(e->d_name);char*rel=safeMalloc(strlen(r)+strlen(e->d_name)+2);snprintf(rel,strlen(r)+strlen(e->d_name)+2,"%s/%s",r,e->d_name);char*cp=path_for(x,rel);struct stat s;v.isDirectory=stat(cp,&s)==0&&S_ISDIR(s.st_mode);free(cp);free(rel);arrput(out,v);}closedir(d);return out;}
static FileSystemVtable vt={resolve,exists,readtext,writetext,del,readbin,writebin,openbin,closebin,readstream,writestream,tell,seek,size,rewrite,dexists,mkdirx,rmdirx,listdir};
FileSystem*PspFileSystem_create(const char*r){PspFS*f=safeCalloc(1,sizeof(*f));f->base.vtable=&vt;f->root=safeStrdup(r);return(FileSystem*)f;}void PspFileSystem_destroy(FileSystem*b){if(!b)return;PspFS*f=(PspFS*)b;free(f->root);free(f);}