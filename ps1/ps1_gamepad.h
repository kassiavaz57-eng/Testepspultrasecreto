#ifndef _BS_PS1_GAMEPAD_H_
#define _BS_PS1_GAMEPAD_H_

#include "runner_gamepad.h"

void Ps1Gamepad_init(void);
void Ps1Gamepad_poll(RunnerGamepadState* gp, int port);

#endif /* _BS_PS1_GAMEPAD_H_ */
