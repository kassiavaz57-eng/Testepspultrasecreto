#include "psp_audio_system.h"
#include "data_win.h"
#include "stb_vorbis.h"
#include "utils.h"
#include <pspaudio.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PSP_AUDIO_FRAMES 2048
#define PSP_AUDIO_BUFFER_SAMPLES (PSP_AUDIO_FRAMES * 2)
#define PSP_SOUND_INSTANCE_BASE 100000
static volatile unsigned long g_audioPlayCalls=0,g_audioDecodeFails=0,g_audioBadData=0,g_audioOutputFails=0;

static int pspAudioThread(SceSize args, void* argp);

static void pspAudioInit(AudioSystem* audio, DataWin* dw, FileSystem* fs) {
    (void)fs;
    PspAudioSystem* a=(PspAudioSystem*)audio;
    a->base.dw=dw;
    a->channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,PSP_AUDIO_FRAMES,PSP_AUDIO_FORMAT_STEREO);
    if(a->channel<0){ logWarn("PSP audio: sceAudioChReserve failed %d\n",a->channel); return; }
    a->mutex=sceKernelCreateSema("psp_audio",0,1,1,NULL);
    a->running=1;
    a->stopRequested=0;
    a->instanceId=PSP_SOUND_INSTANCE_BASE;
    a->pspThread=sceKernelCreateThread("psp_audio_thread",pspAudioThread,0x20,0x4000,0,NULL);
    if(a->pspThread>=0) sceKernelStartThread(a->pspThread,sizeof(a),a);
    logInfo("PSP audio: hardware channel initialized\n");
}
static void pspAudioDiagReport(PspAudioSystem* a){
    static unsigned long last=0;
    unsigned long now=(unsigned long)(sceKernelGetSystemTimeWide()/1000000ULL);
    if(now==last)return;
    last=now;
    FILE *f=fopen("ms0:/PSP/GAME/BUTTERSCOTCH/psp_diag.txt","a");
    if(!f)return;
    fprintf(f,"PSP_AUDIO play=%lu decodeFail=%lu badData=%lu outputFail=%lu channel=%d\\n",
        g_audioPlayCalls,g_audioDecodeFails,g_audioBadData,g_audioOutputFails,a->channel);
    fclose(f);
}

static void pspAudioDestroy(AudioSystem* audio){
    PspAudioSystem* a=(PspAudioSystem*)audio;
    a->running=0;
    if(a->pspThread>=0){ sceKernelWaitThreadEnd(a->pspThread,NULL); sceKernelDeleteThread(a->pspThread); }
    if(a->channel>=0) sceAudioChRelease(a->channel);
    if(a->mutex>=0) sceKernelDeleteSema(a->mutex);
    free(a->pcm); free(a);
}
static void pspAudioUpdate(AudioSystem* audio,float dt){(void)dt;pspAudioDiagReport((PspAudioSystem*)audio);}
static int32_t pspPlaySound(AudioSystem* audio,int32_t soundIndex,int32_t priority,bool loop){
    (void)priority;
    PspAudioSystem* a=(PspAudioSystem*)audio;
    g_audioPlayCalls++;
    if(a->channel<0 || soundIndex<0 || (uint32_t)soundIndex>=a->base.dw->sond.count){g_audioBadData++;return -1;}
    Sound* s=&a->base.dw->sond.sounds[soundIndex];
    if(s->audioGroup<0)return -1;
    DataWin* group=(s->audioGroup==0)?a->base.dw:NULL;
    if(!group || s->audioFile<0 || (uint32_t)s->audioFile>=group->audo.count){g_audioBadData++;return -1;}
    DataWin_loadAudoIfNeeded(group,(uint32_t)s->audioFile);
    AudioEntry* e=&group->audo.entries[s->audioFile];
    if(!e->data || !e->dataSize){g_audioBadData++;return -1;}
    int channels=0,rate=0;
    short* decoded=NULL;
    int frames=stb_vorbis_decode_memory(e->data,(int)e->dataSize,&channels,&rate,&decoded);
    if(frames<=0 || !decoded){ logWarn("PSP audio: Vorbis decode failed sound=%d\n",soundIndex); free(decoded); return -1; }
    if(channels!=1 && channels!=2){free(decoded);return -1;}
    sceKernelWaitSema(a->mutex,1,NULL);
    free(a->pcm);
    a->pcm=decoded; a->totalFrames=frames; a->position=0; a->sampleRate=rate; a->channels=channels;
    a->gain=s->volume; a->soundIndex=soundIndex; a->instanceId++;
    a->stopRequested=loop?0:1;
    sceKernelSignalSema(a->mutex,1);
    if(a->sampleRate!=44100){
        sceAudioSetChannelDataLen(a->channel,PSP_AUDIO_FRAMES);
        sceAudioChangeChannelConfig(a->channel,PSP_AUDIO_FORMAT_STEREO);
        // The hardware channel is fixed-rate; common Undertale assets are 44.1 kHz.
    }
    return a->instanceId;
}
static void pspStopSound(AudioSystem* audio,int32_t id){
    PspAudioSystem* a=(PspAudioSystem*)audio;
    if(id>=PSP_SOUND_INSTANCE_BASE || id==a->soundIndex){
        sceKernelWaitSema(a->mutex,1,NULL); a->position=a->totalFrames; sceKernelSignalSema(a->mutex,1);
    }
}
static void pspStopAll(AudioSystem* audio){PspAudioSystem* a=(PspAudioSystem*)audio;sceKernelWaitSema(a->mutex,1,NULL);a->position=a->totalFrames;sceKernelSignalSema(a->mutex,1);}
static bool pspIsPlaying(AudioSystem* audio,int32_t id){PspAudioSystem* a=(PspAudioSystem*)audio;return a->pcm && a->position<a->totalFrames && (id<PSP_SOUND_INSTANCE_BASE || id==a->instanceId);}
static void pspPause(AudioSystem* a,int32_t id){(void)a;(void)id;}
static void pspResume(AudioSystem* a,int32_t id){(void)a;(void)id;}
static void pspPauseAll(AudioSystem* a){(void)a;}
static void pspResumeAll(AudioSystem* a){(void)a;}
static void pspSuspend(AudioSystem* a){(void)a;}
static void pspResumeAudio(AudioSystem* a){(void)a;}
static void pspSetGain(AudioSystem* audio,int32_t id,float gain,uint32_t ms){(void)id;(void)ms;PspAudioSystem*a=(PspAudioSystem*)audio;a->gain=gain;}
static float pspGetGain(AudioSystem* audio,int32_t id){(void)id;return ((PspAudioSystem*)audio)->gain;}
static void pspSetPitch(AudioSystem*a,int32_t id,float p){(void)a;(void)id;(void)p;}
static float pspGetPitch(AudioSystem*a,int32_t id){(void)a;(void)id;return 1.0f;}
static float pspGetPos(AudioSystem*a,int32_t id){(void)id;PspAudioSystem*x=(PspAudioSystem*)a;return x->sampleRate?((float)x->position/(float)x->sampleRate):0.0f;}
static void pspSetPos(AudioSystem*a,int32_t id,float sec){(void)id;PspAudioSystem*x=(PspAudioSystem*)a;int p=(int)(sec*x->sampleRate);if(p<0)p=0;if(p>x->totalFrames)p=x->totalFrames;x->position=p;}
static float pspLength(AudioSystem*a,int32_t id){(void)id;PspAudioSystem*x=(PspAudioSystem*)a;return x->sampleRate?((float)x->totalFrames/(float)x->sampleRate):0.0f;}
static void pspMaster(AudioSystem*a,float g){((PspAudioSystem*)a)->gain=g;}
static void pspMasterListener(AudioSystem*a,float g,int32_t id){(void)id;pspMaster(a,g);}
static void pspChannels(AudioSystem*a,int32_t n){(void)a;(void)n;}
static void pspGroupLoad(AudioSystem*a,int32_t i){(void)a;(void)i;}
static bool pspGroupLoaded(AudioSystem*a,int32_t i){(void)a;(void)i;return true;}
static int32_t pspCreateStream(AudioSystem*a,const char*f){(void)a;(void)f;return -1;}
static bool pspDestroyStream(AudioSystem*a,int32_t i){(void)a;(void)i;return false;}

static AudioSystemVtable g_vt={
 pspAudioInit,pspAudioDestroy,pspAudioUpdate,pspPlaySound,pspStopSound,pspStopAll,pspIsPlaying,
 pspPause,pspResume,pspPauseAll,pspResumeAll,pspSuspend,pspResumeAudio,pspSetGain,pspGetGain,
 pspSetPitch,pspGetPitch,pspGetPos,pspSetPos,pspLength,pspMaster,pspMasterListener,pspChannels,
 pspGroupLoad,pspGroupLoaded,pspCreateStream,pspDestroyStream
};

static int pspAudioThread(SceSize args,void*argp){
    (void)args;
    PspAudioSystem*a=(PspAudioSystem*)argp;
    static short out[PSP_AUDIO_BUFFER_SAMPLES] __attribute__((aligned(64)));
    while(a->running){
        int localFrames=0, localChannels=2;
        float gain=1.0f;
        sceKernelWaitSema(a->mutex,1,NULL);
        if(a->pcm && a->position<a->totalFrames){
            localFrames=PSP_AUDIO_FRAMES;
            if(a->totalFrames-a->position<localFrames)localFrames=a->totalFrames-a->position;
            gain=a->gain;
            for(int i=0;i<PSP_AUDIO_FRAMES;i++){
                int src=(i<localFrames)?(a->position+i):(a->totalFrames-1);
                if(src<0)src=0;
                short l,r;
                if(a->channels==1) l=r=a->pcm[src];
                else {l=a->pcm[src*2];r=a->pcm[src*2+1];}
                int lv=(int)(l*gain),rv=(int)(r*gain);
                if(lv>32767)lv=32767;if(lv<-32768)lv=-32768;
                if(rv>32767)rv=32767;if(rv<-32768)rv=-32768;
                out[i*2]=(short)lv;out[i*2+1]=(short)rv;
            }
            a->position+=localFrames;
            if(a->position>=a->totalFrames && !a->stopRequested) a->position=0;
        } else memset(out,0,sizeof(out));
        sceKernelSignalSema(a->mutex,1);
        int audioResult=sceAudioOutputBlocking(a->channel,PSP_AUDIO_VOLUME_MAX,out);
        if(audioResult<0)g_audioOutputFails++;
    }
    return 0;
}

PspAudioSystem* PspAudioSystem_create(void){
    PspAudioSystem*a=(PspAudioSystem*)calloc(1,sizeof(PspAudioSystem));
    if(!a)return NULL;
    a->base.vtable=&g_vt;a->channel=-1;a->mutex=-1;a->pspThread=-1;a->gain=1.0f;
    return a;
}
