/*
 * dvplayer
 *
 * Program to play back DV via a FireWire/IEEE1394 port.
 *
 * Copyright (C) 2024 Robbi Blechdose
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libiec61883/iec61883.h>
#include <ncurses.h>

#include <stdio.h>
#include <sys/poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include <stdbool.h>

#include "dv.h"

//TODO: ntsc support?

#define NUM_FRAMES_FFRW 50

#define PACKET_SIZE (DV_DIF_BLOCK_SIZE * 6)

const char* fileName = NULL;
bool uiEnabled = true;

static bool interrupted = false;

bool seekRequested = false;
int seekRequestedDiff = 0;

bool pauseChangeRequested = false;
bool pauseRequested = false;
bool isPaused = false;

bool frameReloadRequested = false;

int currentPackets = 0;
int currentFrame = 0;

unsigned char dvFrame[DV_FRAME_SIZE_PAL];
char timecodeBuffer[64];

void seekFrame(FILE* file, int diff)
{
    //This accounts for the fact we're at the end of the currently displaying frame
    //So if we want to go back one frame, we have to go to the beginning of the *previous* frame, meaning we seek back 2 frames in total
    diff -= 1;
    if(currentFrame + diff > 0)
    {
        currentFrame += diff;
    }
    fseek(file, diff * DV_FRAME_SIZE_PAL, SEEK_CUR);
}

static int readPacket(unsigned char* data, int n, unsigned int dropped, void* callback_data)
{
    FILE* file = (FILE*) callback_data;

    if(n != 1)
    {
        return 0;
    }

    currentPackets++;
    if(currentPackets == DV_DIF_BLOCKS_PER_SEQUENCE * 2)
    {
        currentPackets = 0;

        //If we're paused, don't advance the frame counter and don't read a new frame
        if(!isPaused || frameReloadRequested)
        {
            currentFrame++;
            //This is used to reload the frame when seeking while paused, or when pausing
            frameReloadRequested = false;

            //Read new frame
            if(fread(dvFrame, DV_FRAME_SIZE_PAL, 1, file) != 1)
            {
                return -1;
            }

            if(isPaused)
            {
                dv_removeAudio(dvFrame);
            }
        }

        dv_getTimecode(dvFrame, timecodeBuffer);

        if(pauseChangeRequested)
        {
            isPaused = pauseRequested;
            pauseChangeRequested = false;
            frameReloadRequested = true;
        }
        else if(seekRequested)
        {
            seekFrame(file, seekRequestedDiff);
            seekRequested = false;
            frameReloadRequested = true;
        }
    }

    memcpy(data, dvFrame + currentPackets * PACKET_SIZE, PACKET_SIZE);
    return 0;
}

static void sighandler(int sig)
{
    interrupted = true;
}

void handleInput(FILE* file)
{
    int c = getch();
    if(c == 'p')
    {
        pauseChangeRequested = true;
        pauseRequested = !isPaused;
    }
    else if(c == 'f')
    {
        seekRequested = true;
        seekRequestedDiff = NUM_FRAMES_FFRW;
    }
    else if(c == 'r')
    {
        seekRequested = true;
        seekRequestedDiff = -NUM_FRAMES_FFRW;
    }
    else if(c == KEY_RIGHT)
    {
        seekRequested = true;
        seekRequestedDiff = 1;
    }
    else if(c == KEY_LEFT)
    {
        seekRequested = true;
        seekRequestedDiff = -1;
    }
}

void drawNcursesUI(bool isPAL)
{
    char buffer[128];
    box(stdscr, 0, 0);
    mvaddstr(0, 8, "dvplayer");
    sprintf(buffer, "File: %s", fileName == NULL ? "stdin" : fileName);
    mvaddstr(1, 1, buffer);
    sprintf(buffer, "Format: %4s", isPAL ? "PAL" : "NTSC");
    mvaddstr(2, 1, buffer);
    sprintf(buffer, "Timecode: %s   Frame: %5d", timecodeBuffer, currentFrame);
    mvaddstr(3, 1, buffer);
    sprintf(buffer, "Paused: %3s", isPaused ? "Yes" : "No");
    mvaddstr(4, 1, buffer);
    mvaddstr(8, 1, "P - Play/Pause   F - Forward 1s   R - Rewind 1s   Left Arrow - Previous Frame   Right Arrow - Next Frame   Ctrl+C - Quit");
}

static void transmitDV(raw1394handle_t handle, FILE* file, int channel)
{	
    unsigned char data[PACKET_SIZE];
    fread(data, PACKET_SIZE, 1, file);

    int isPAL = (data[3] & 0x80) != 0;

    iec61883_dv_t dv = iec61883_dv_xmit_init(handle, isPAL, readPacket, (void*) file);
    if(dv == NULL)
    {
        return;
    }

    if(iec61883_dv_xmit_start(dv, channel) != 0)
    {
        iec61883_dv_close(dv);
        return;
    }
    
    struct pollfd pfd = {
        fd: raw1394_get_fd(handle),
        events: POLLIN,
        revents: 0
    };
    
    signal(SIGINT, sighandler);
    signal(SIGPIPE, sighandler);
    if(!uiEnabled)
    {
        printf("Starting to transmit %s.\n", isPAL ? "PAL" : "NTSC");
    }

    int result = 0;
    do
    {
        int r = 0;
        if((r = poll(&pfd, 1, 100)) > 0 && (pfd.revents & POLLIN))
        {
            result = raw1394_loop_iterate(handle);
            if(uiEnabled)
            {
                handleInput(file);
                drawNcursesUI(isPAL);
            }
        }
        
    }
    while(!interrupted && result == 0);
    
    iec61883_dv_close(dv);
    if(!uiEnabled)
    {
        printf("Done.\n");
    }
}

const char* helpText = "Usage: dvplayer [-n | --noui] [-t node-id] [- | file]\n"
                       "Options:\n"
                       "  -n | --noui  Disable ncurses UI. This also disables the input handling, only normal streaming is supported.\n"
                       "  -t node-id   Supply a node ID for the FireWire device to stream to.\n"
                       "  - | file     Read DV from stdin or from a file.\n";

int main(int argc, char* argv[])
{
    nodeid_t node = 0xffc0;
    bool nodeSpecified = false;

    FILE* inputFile = NULL;

    for(int i = 1; i < argc; i++)
    {
        if(strncmp(argv[i], "-h", 2) == 0 ||  strncmp(argv[i], "--help", 6) == 0)
        {
            printf(helpText);
            return 1;
        }
        else if(strncmp(argv[i], "-n", 2) == 0 ||strncmp(argv[i], "--noui", 6) == 0)
        {
            uiEnabled = false;
        }
        else if(strncmp(argv[i], "-t", 2) == 0)
        {
            node |= atoi(argv[++i]);
            nodeSpecified = true;
        }
        else if(strcmp(argv[i], "-") != 0)
        {
            inputFile = fopen(argv[i], "rb");
            fileName = argv[i];
        }
    }

    raw1394handle_t handle = raw1394_new_handle_on_port(0);
    int channel;
    int bandwidth = -1;
    
    if(!handle)
    {
        printf("Failed to get libraw1394 handle.\n");
        if(inputFile != NULL)
        {
            fclose(inputFile);
        }
        return -1;
    }
    
    int oplug = -1, iplug = -1;
    
    if(inputFile == NULL)
    {
        inputFile = stdin;
    }

    if(uiEnabled)
    {
        //Set up ncurses
        initscr();
        noecho();
        curs_set(0);
        keypad(stdscr, true);
        timeout(0);
    }
    
    if(nodeSpecified)
    {
        channel = iec61883_cmp_connect(handle, raw1394_get_local_id(handle), &oplug, node, &iplug, &bandwidth);
        if(channel > -1)
        {
            printf("Connect succeeded, transmitting on channel %d.\n", channel);
            transmitDV(handle, inputFile, channel);
            iec61883_cmp_disconnect(handle, raw1394_get_local_id(handle), oplug, node, iplug, channel, bandwidth);
        }
        else
        {
            printf("Connect failed.\n");
        }
    }
    else
    {
        transmitDV(handle, inputFile, 63);
    }

    if(uiEnabled)
    {
        //Quit ncurses
        endwin();
    }

    if(inputFile != stdin)
    {
        fclose(inputFile);
    }

    raw1394_destroy_handle(handle);
    
    return 0;
}
