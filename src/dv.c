#include "dv.h"

#include <stdbool.h>
#include <stdio.h>

void dvGetTimecode(unsigned char* dvFrame, char* timecodeBuffer)
{
    for(int blockIndex = 0; blockIndex < DV_DIF_BLOCKS_PER_SEQUENCE * DV_SEQUENCES_PER_FRAME_PAL; blockIndex++)
    {
        int blockOffset = blockIndex * DV_DIF_BLOCK_SIZE;

        int blockType = dvFrame[blockOffset] >> 5;
        if(blockType != 1)
        {
            continue;
        }

        for(int ssybIndex = 0; ssybIndex < DV_SSYB_BLOCKS_PER_DIF_BLOCK; ssybIndex++)
        {
            int ssybOffset = blockOffset + 3 + ssybIndex * DV_SSYB_BLOCK_SIZE;

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