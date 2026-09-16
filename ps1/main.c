#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psxetc.h>
#include <psxgpu.h>
#include <psxcd.h>
#define W 320
#define H 240
static DISPENV disp[2]; static DRAWENV draw[2];
static void video(void){ResetGraph(0);SetDefDispEnv(&disp[0],0,0,W,H);SetDefDispEnv(&disp[1],0,H,W,H);SetDefDrawEnv(&draw[0],0,H,W,H);SetDefDrawEnv(&draw[1],0,0,W,H);draw[0].isbg=1;draw[1].isbg=1;setRGB0(&draw[0],0,0,0);setRGB0(&draw[1],0,0,0);PutDispEnv(&disp[0]);PutDrawEnv(&draw[0]);SetDispMask(1);FntLoad(960,0);FntOpen(8,8,304,224,0,512);}
static void status(const char*a,const char*b,const char*c){FntPrint(0,"BUTTERSCOTCH PS1
----------------
%s
%s
%s
",a,b,c);DrawSync(0);VSync(0);PutDispEnv(&disp[0]);PutDrawEnv(&draw[0]);FntFlush(-1);}
static int probe(void){CdlFILE f;uint32_t sector[512];if(!CdSearchFile(&f,"\\DATA.WIN;1"))return -1;if(CdRead(1,sector,CdlModeSize)<0)return -2;if(CdReadSync(0,0)<0)return -3;if(memcmp(sector,"GEN8",4)!=0)return -4;return 0;}
int main(void){video();CdInit();status("Initializing CD-ROM...","","");int r=probe();if(r==0)status("DATA.WIN found.","GEN8 header OK.","PS1 streaming path ready.");else if(r==-1)status("DATA.WIN not found.","Put Chapter 1 data.win","in the disc root.");else if(r==-4)status("DATA.WIN found.","Header is not GEN8.","Not a normal GameMaker WAD.");else status("DATA.WIN found.","CD read failed.","Stopped before loading the file.");for(;;)VSync(0);return 0;}