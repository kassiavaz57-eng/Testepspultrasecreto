#ifndef _BS_PS1_UTILS_H_
#define _BS_PS1_UTILS_H_

#include "common.h"
#include <stdint.h>
#include <stdbool.h>

/* PS1 has no PS2-style device-key/IOP-driver layer. The port's packaged
 * assets live on the CD-ROM and are addressed through the PS1 CD stream. */

void PS1Utils_init(void);

/* Resolve a GameMaker/Butterscotch boot-relative path to a PS1 CD path.
 * The returned string is heap allocated and must be freed by the caller. */
char* PS1Utils_createDevicePath(const char* path);

#endif /* _BS_PS1_UTILS_H_ */
