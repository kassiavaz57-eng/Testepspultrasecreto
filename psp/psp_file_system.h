#ifndef BS_PSP_FILE_SYSTEM_H
#define BS_PSP_FILE_SYSTEM_H
#include "file_system.h"
FileSystem* PspFileSystem_create(const char*rootPath);
void PspFileSystem_destroy(FileSystem*fs);
#endif
