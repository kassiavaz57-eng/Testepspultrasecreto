#ifndef _BS_PS1_FILE_SYSTEM_H_
#define _BS_PS1_FILE_SYSTEM_H_
#include "common.h"
#include "../file_system.h"
#include "../json_reader.h"
FileSystem* Ps1FileSystem_create(JsonValue* configRoot, const char* gameTitle);
void Ps1FileSystem_destroy(FileSystem* fs);
#endif
