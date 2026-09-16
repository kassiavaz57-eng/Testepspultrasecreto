#include <stdint.h>
#include <psxetc.h>
#include <psxapi.h>
uint64_t nowNanos(void) {
 static int init=0; static uint32_t last=0; static uint64_t high=0; uint32_t now;
 if(!init){SetRCnt(RCntCNT2,0xffff,RCntMdNOINTR|RCntMdFR);StartRCnt(RCntCNT2);last=(uint32_t)GetRCnt(RCntCNT2);init=1;}
 now=(uint32_t)GetRCnt(RCntCNT2); if(now<last)high+=65536u; last=now;
 return ((high+now)*1000000000ull)/4515840ull;
}