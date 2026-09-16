#ifndef PSP_STB_VORBIS_H
#define PSP_STB_VORBIS_H
#ifdef __cplusplus
extern "C" {
#endif
int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels, int *sample_rate, short **output);
#ifdef __cplusplus
}
#endif
#endif
