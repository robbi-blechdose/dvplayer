#ifndef DV_H
#define DV_H

#include <stdbool.h>

#define DV_SSYB_PACK_SIZE 8
#define DV_SSYB_PACKS_PER_DIF_BLOCK 6

#define DV_DIF_BLOCK_SIZE 80
#define DV_DIF_BLOCKS_PER_SEQUENCE 150
#define DV_SEQUENCES_PER_FRAME_PAL  12
#define DV_SEQUENCES_PER_FRAME_NTSC 10

#define DV_FRAME_SIZE_PAL (DV_DIF_BLOCK_SIZE * DV_DIF_BLOCKS_PER_SEQUENCE * DV_SEQUENCES_PER_FRAME_PAL)
#define DV_FRAME_SIZE_NTSC (DV_DIF_BLOCK_SIZE * DV_DIF_BLOCKS_PER_SEQUENCE * DV_SEQUENCES_PER_FRAME_NTSC)

void dv_getTimecode(unsigned char* dvFrame, char* timecodeBuffer, bool isPAL);

void dv_removeAudio(unsigned char* dvFrame, bool isPAL);

#endif