#include <pspctrl.h>
#include <stdbool.h>
#include "psp_input.h"
#include "runner_keyboard.h"
static int key(uint32_t b){switch(b){case PSP_CTRL_UP:return VK_UP;case PSP_CTRL_DOWN:return VK_DOWN;case PSP_CTRL_LEFT:return VK_LEFT;case PSP_CTRL_RIGHT:return VK_RIGHT;case PSP_CTRL_CROSS:return 'Z';case PSP_CTRL_CIRCLE:return 'X';case PSP_CTRL_SQUARE:return 'C';case PSP_CTRL_TRIANGLE:return 'V';case PSP_CTRL_START:return VK_ENTER;case PSP_CTRL_SELECT:return VK_ESCAPE;default:return -1;}}
void PspInput_poll(Runner*r,SceCtrlData*p){static uint32_t prev=0;uint32_t cur=p->Buttons;const uint32_t bs[]={PSP_CTRL_UP,PSP_CTRL_DOWN,PSP_CTRL_LEFT,PSP_CTRL_RIGHT,PSP_CTRL_CROSS,PSP_CTRL_CIRCLE,PSP_CTRL_SQUARE,PSP_CTRL_TRIANGLE,PSP_CTRL_START,PSP_CTRL_SELECT};for(unsigned i=0;i<sizeof(bs)/sizeof(bs[0]);++i){int k=key(bs[i]);bool was=(prev&bs[i])!=0,now=(cur&bs[i])!=0;if(now&&!was)RunnerKeyboard_onKeyDown(r->keyboard,k);else if(!now&&was)RunnerKeyboard_onKeyUp(r->keyboard,k);}prev=cur;}
