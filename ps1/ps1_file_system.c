#include "ps1_file_system.h"
#include "../utils.h"
#include "stdio_compat.h"
#include <stdlib.h>
#include <string.h>
#include "stb_ds.h"

typedef struct { char* key; char** value; } Ps1FileMapping;
typedef struct { FileSystem base; Ps1FileMapping* mappings; } Ps1FileSystem;

static char* resolveConfiguredPath(const char* raw) {
    if (strncmp(raw, "$BOOT:", 6) == 0) {
        const char* p = raw + 6;
        size_t n = strlen(p) + 16;
        char* out = (char*)safeMalloc(n);
        snprintf(out, n, "\\%s;1", p[0] == '/' ? p + 1 : p);
        return out;
    }
    return safeStrdup(raw);
}
static ptrdiff_t find(Ps1FileSystem* pfs,const char* name){return shgeti(pfs->mappings,name);}
static char* resolvePath(FileSystem* fs,const char* name){Ps1FileSystem*p=(Ps1FileSystem*)fs;ptrdiff_t i=find(p,name);if(i<0||arrlen(p->mappings[i].value)==0)return NULL;return safeStrdup(p->mappings[i].value[0]);}
static bool fileExists(FileSystem* fs,const char* name){Ps1FileSystem*p=(Ps1FileSystem*)fs;ptrdiff_t i=find(p,name);if(i<0)return false;for(int j=0;j<arrlen(p->mappings[i].value);j++){FILE*f=fopen(p->mappings[i].value[j],"rb");if(f){fclose(f);return true;}}return false;}
static char* readText(FileSystem* fs,const char* name){Ps1FileSystem*p=(Ps1FileSystem*)fs;ptrdiff_t i=find(p,name);if(i<0)return NULL;for(int j=0;j<arrlen(p->mappings[i].value);j++){FILE*f=fopen(p->mappings[i].value[j],"rb");if(!f)continue;fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);char*b=(char*)safeMalloc((size_t)n+1);size_t got=fread(b,1,(size_t)n,f);b[got]=0;fclose(f);return b;}return NULL;}
static bool writeText(FileSystem*fs,const char*name,const char*data){(void)fs;(void)name;(void)data;return false;}
static bool deleteFile(FileSystem*fs,const char*name){(void)fs;(void)name;return false;}
static bool readBinary(FileSystem*fs,const char*name,uint8_t**out,int32_t*size){char*t=readText(fs,name);if(!t)return false;*size=(int32_t)strlen(t);*out=(uint8_t*)t;return true;}
static bool writeBinary(FileSystem*fs,const char*name,const uint8_t*d,int32_t n){(void)fs;(void)name;(void)d;(void)n;return false;}
typedef struct{FILE*f;} Ps1Binary;
static void* binOpen(FileSystem*fs,const char*name,int32_t mode){if(mode!=GML_FILE_BIN_READ)return NULL;char*p=resolvePath(fs,name);if(!p)return NULL;FILE*f=fopen(p,"rb");free(p);if(!f)return NULL;Ps1Binary*h=(Ps1Binary*)safeMalloc(sizeof(*h));h->f=f;return h;}
static void binClose(FileSystem*fs,void*h){(void)fs;if(!h)return;Ps1Binary*b=h;fclose(b->f);free(b);}
static int32_t binRead(FileSystem*fs,void*h,void*d,int32_t n){(void)fs;if(!h||n<=0)return 0;return (int32_t)fread(d,1,n,((Ps1Binary*)h)->f);}
static int32_t binWrite(FileSystem*fs,void*h,const void*d,int32_t n){(void)fs;(void)h;(void)d;(void)n;return 0;}
static int32_t binTell(FileSystem*fs,void*h){(void)fs;return h?(int32_t)ftell(((Ps1Binary*)h)->f):0;}
static bool binSeek(FileSystem*fs,void*h,int32_t p){(void)fs;return h&&fseek(((Ps1Binary*)h)->f,p,SEEK_SET)==0;}
static int32_t binSize(FileSystem*fs,void*h){(void)fs;if(!h)return 0;FILE*f=((Ps1Binary*)h)->f;long p=ftell(f);fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,p,SEEK_SET);return(int32_t)n;}
static void binRewrite(FileSystem*fs,void*h){(void)fs;(void)h;}
static bool dirExists(FileSystem*fs,const char*p){(void)fs;(void)p;return true;}
static bool makeDir(FileSystem*fs,const char*p){(void)fs;(void)p;return true;}
static bool delDir(FileSystem*fs,const char*p){(void)fs;(void)p;return true;}
static FileSystemDirEntry* listDir(FileSystem*fs,const char*p){(void)fs;(void)p;return NULL;}
static FileSystemVtable vt;
FileSystem* Ps1FileSystem_create(JsonValue*root,const char*title){(void)title;JsonValue*obj=JsonReader_getJsonValueByKey(root,"fileSystem");if(!obj||!JsonReader_isObject(obj))return NULL;Ps1FileSystem*p=(Ps1FileSystem*)safeCalloc(1,sizeof(*p));p->base.vtable=&vt;vt.resolvePath=resolvePath;vt.fileExists=fileExists;vt.readFileText=readText;vt.writeFileText=writeText;vt.deleteFile=deleteFile;vt.readFileBinary=readBinary;vt.writeFileBinary=writeBinary;vt.binaryOpen=binOpen;vt.binaryClose=binClose;vt.binaryRead=binRead;vt.binaryWrite=binWrite;vt.binaryTell=binTell;vt.binarySeek=binSeek;vt.binarySize=binSize;vt.binaryRewrite=binRewrite;vt.directoryExists=dirExists;vt.createDirectory=makeDir;vt.deleteDirectory=delDir;vt.listDirectory=listDir;sh_new_strdup(p->mappings);int n=JsonReader_objectLength(obj);repeat(n,i){const char*k=JsonReader_getJsonKeyByIndex(obj,i);JsonValue*a=JsonReader_getJsonValueByIndex(obj,i);if(!JsonReader_isArray(a))continue;char**paths=NULL;repeat(JsonReader_arrayLength(a),j){JsonValue*v=JsonReader_getArrayElement(a,j);if(JsonReader_isString(v))arrput(paths,resolveConfiguredPath(JsonReader_getString(v)));}shput(p->mappings,k,paths);}return(FileSystem*)p;}
void Ps1FileSystem_destroy(FileSystem*fs){if(!fs)return;Ps1FileSystem*p=(Ps1FileSystem*)fs;for(int i=0;i<shlen(p->mappings);i++){for(int j=0;j<arrlen(p->mappings[i].value);j++)free(p->mappings[i].value[j]);arrfree(p->mappings[i].value);}shfree(p->mappings);free(p);}
