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

/**
 * Extract the timecode from a DV frame to a string
 * @param dvFrame The full DV frame
 * @param timecodeBuffer Buffer for the string to be printed into (format: xx:xx:xx.xx)
 * @param isPAL Is this frame in PAL format?
 **/
void dv_getTimecode(unsigned char* dvFrame, char* timecodeBuffer, bool isPAL);

/**
 * Remove the audio from a DV frame
 * This is useful for "pausing" video
 * @param dvFrame The full DV frame
 * @param isPAL Is this frame in PAL format?
 **/
void dv_removeAudio(unsigned char* dvFrame, bool isPAL);

#endif