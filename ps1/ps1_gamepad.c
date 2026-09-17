#include <common.h>
#include "ps1_gamepad.h"

#include <psxpad.h>
#include "stdio_compat.h"
#include "string_compat.h"
#include "log.h"

/* PSn00bSDK requires two pad buffers for the two controller ports. */
static uint8_t ps1_pad_buffers[2][34];

void Ps1Gamepad_poll(RunnerGamepadState* gp, int port) {
    if (gp == nullptr || port < 0 || port >= 2 || port >= MAX_GAMEPADS) return;

    GamepadSlot* slot = &gp->slots[port];
    memcpy(slot->buttonDownPrev, slot->buttonDown, sizeof(slot->buttonDown));
    memset(slot->buttonDown, 0, sizeof(slot->buttonDown));
    memset(slot->buttonValue, 0, sizeof(slot->buttonValue));
    memset(slot->axisValue, 0, sizeof(slot->axisValue));

    /* PSn00bSDK's PADTYPE is populated by InitPAD/StartPAD. */
    PADTYPE* pad = (PADTYPE*) ps1_pad_buffers[port];
    if (pad->stat != 0) {
        slot->connected = false;
        slot->guid[0] = '\0';
        return;
    }

    if (pad->type != PAD_ID_DIGITAL &&
        pad->type != PAD_ID_ANALOG_STICK &&
        pad->type != PAD_ID_ANALOG) {
        slot->connected = false;
        slot->guid[0] = '\0';
        return;
    }

    /* PS1 buttons are active-low: 0 means pressed. Keep the same button
       ordering used by the PS2 backend so Runner/gamepad GML sees the
       identical logical layout. */
    uint16_t buttons = pad->btn;
    if (!(buttons & PAD_CROSS))    slot->buttonDown[0] = true;
    if (!(buttons & PAD_CIRCLE))   slot->buttonDown[1] = true;
    if (!(buttons & PAD_SQUARE))   slot->buttonDown[2] = true;
    if (!(buttons & PAD_TRIANGLE)) slot->buttonDown[3] = true;
    if (!(buttons & PAD_L1))       slot->buttonDown[4] = true;
    if (!(buttons & PAD_R1))       slot->buttonDown[5] = true;
    if (!(buttons & PAD_L2))       slot->buttonDown[6] = true;
    if (!(buttons & PAD_R2))       slot->buttonDown[7] = true;
    if (!(buttons & PAD_SELECT))   slot->buttonDown[8] = true;
    if (!(buttons & PAD_START))    slot->buttonDown[9] = true;
    if (!(buttons & PAD_UP))       slot->buttonDown[12] = true;
    if (!(buttons & PAD_DOWN))     slot->buttonDown[13] = true;
    if (!(buttons & PAD_LEFT))     slot->buttonDown[14] = true;
    if (!(buttons & PAD_RIGHT))    slot->buttonDown[15] = true;

    for (int i = 0; i < GP_BUTTON_COUNT; i++)
        slot->buttonValue[i] = slot->buttonDown[i] ? 1.0f : 0.0f;

    if (!slot->connected) {
        snprintf(slot->description, sizeof(slot->description), "PlayStation Controller (port %d)", port);
        slot->guid[0] = '\0';
        slot->jid = port;
    }
    slot->connected = true;

    for (int btn = 0; btn < GP_BUTTON_COUNT; btn++) {
        bool wasDown = slot->buttonDownPrev[btn];
        if (slot->buttonDown[btn] && !wasDown) slot->buttonPressed[btn] = true;
        if (!slot->buttonDown[btn] && wasDown) slot->buttonReleased[btn] = true;
    }

    gp->connectedCount++;
}

/* Called once during platform startup. */
void Ps1Gamepad_init(void) {
    InitPAD(ps1_pad_buffers[0], sizeof(ps1_pad_buffers[0]),
            ps1_pad_buffers[1], sizeof(ps1_pad_buffers[1]));
    StartPAD();
    ChangeClearPAD(0);
}
