#include "dv.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    DIFT_HEADER  = 0b000,
    DIFT_SUBCODE = 0b001,
    DIFT_VAUX    = 0b010,
    DIFT_AUDIO   = 0b011
} DIFBlockType;

void dv_getTimecode(unsigned char* dvFrame, char* timecodeBuffer, bool isPAL)
{
    int sequencesPerFrame = isPAL ? DV_SEQUENCES_PER_FRAME_PAL : DV_SEQUENCES_PER_FRAME_NTSC;

    for(int blockIndex = 0; blockIndex < DV_DIF_BLOCKS_PER_SEQUENCE * sequencesPerFrame; blockIndex++)
    {
        int blockOffset = blockIndex * DV_DIF_BLOCK_SIZE;

        DIFBlockType blockType = dvFrame[blockOffset] >> 5;
        if(blockType != DIFT_SUBCODE)
        {
            continue;
        }

        for(int ssybIndex = 0; ssybIndex < DV_SSYB_PACKS_PER_DIF_BLOCK; ssybIndex++)
        {
            int ssybOffset = blockOffset + 3 + ssybIndex * DV_SSYB_PACK_SIZE;

            int ssybNumber = dvFrame[ssybOffset + 1] & 0x0F;

            if(ssybNumber == 3)
            {
                int frames =  (dvFrame[ssybOffset + 4] & 0x0F) + ((dvFrame[ssybOffset + 4] & 0x30) >> 4) * 10;
                int seconds = (dvFrame[ssybOffset + 5] & 0x0F) + ((dvFrame[ssybOffset + 5] & 0x70) >> 4) * 10;
                int minutes = (dvFrame[ssybOffset + 6] & 0x0F) + ((dvFrame[ssybOffset + 6] & 0x70) >> 4) * 10;
                int hours =   (dvFrame[ssybOffset + 7] & 0x0F) + ((dvFrame[ssybOffset + 7] & 0x30) >> 4) * 10;
                sprintf(timecodeBuffer, "%02d:%02d:%02d.%02d", hours, minutes, seconds, frames);
                return;
            }
        }
    }
}

void dv_removeAudio(unsigned char* dvFrame, bool isPAL)
{
    int sequencesPerFrame = isPAL ? DV_SEQUENCES_PER_FRAME_PAL : DV_SEQUENCES_PER_FRAME_NTSC;

    for(int blockIndex = 0; blockIndex < DV_DIF_BLOCKS_PER_SEQUENCE * sequencesPerFrame; blockIndex++)
    {
        int blockOffset = blockIndex * DV_DIF_BLOCK_SIZE;

        DIFBlockType blockType = dvFrame[blockOffset] >> 5;
        if(blockType != DIFT_AUDIO)
        {
            continue;
        }

        //Zero out audio data, leaving ID and audio auxiliary data intact
        memset(dvFrame + blockOffset + 8, 0, DV_DIF_BLOCK_SIZE - 8);
    }
}