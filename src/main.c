/*
 * libiec61883 - Linux IEEE 1394 streaming media library.
 * Copyright (C) 2004 Dan Dennedy
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <libiec61883/iec61883.h>
#include <ncurses.h>

#include <stdio.h>
#include <sys/poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include <stdbool.h>

#define PACKET_SIZE 480

static bool interrupted = false;

static bool isPaused = false;

static int currentDIFBlocks = 0;
static int currentFrame = 0;

//TODO: this is not reading a full frame, but likely 6 DIF blocks
static int readFrame(unsigned char *data, int n, unsigned int dropped, void *callback_data)
{
    FILE *f = (FILE*) callback_data;

    if(n == 1)
    {
        if(fread(data, PACKET_SIZE, 1, f) < 1)
        {
            return -1;
        }
        else
        {
            currentDIFBlocks++;
            if(currentDIFBlocks == 150 * 2)
            {
                currentDIFBlocks = 0;

                if(isPaused)
                {
                    //This will read the same bit over and over, thus "pausing" the video
                    fseek(f, -150 * 2 * PACKET_SIZE, SEEK_CUR);
                }
                else
                {
                    currentFrame++;
                }
            }
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

static void sighandler(int sig)
{
    interrupted = true;
}

static void dv_transmit(raw1394handle_t handle, FILE *f, int channel)
{	
    unsigned char data[PACKET_SIZE];
    fread(data, PACKET_SIZE, 1, f);

    int isPAL = (data[3] & 0x80) != 0;

    iec61883_dv_t dv = iec61883_dv_xmit_init(handle, isPAL, readFrame, (void *)f);
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
    fprintf(stderr, "Starting to transmit %s.\n", isPAL ? "PAL" : "NTSC");

    int result = 0;
    do
    {
        int r = 0;
        if((r = poll(&pfd, 1, 100)) > 0 && (pfd.revents & POLLIN))
        {
            result = raw1394_loop_iterate(handle);
            char buffer[80];
            sprintf(buffer, "Current frame: %5d Paused: %3s\r", currentFrame, isPaused ? "Yes" : "No");
            mvaddstr(0, 0, buffer);
            if(getch() == 'p')
            {
                isPaused = !isPaused;
            }
        }
        
    }
    while(!interrupted && result == 0);
    
    iec61883_dv_close(dv);
    fprintf(stderr, "Done.\n");
}

const char* helpText = "usage: dvplayer [-t node-id] [- | file]\n"
                       "       Use - to transmit raw DV from stdin, or\n"
                       "       supply a filename to to transmit from a raw DV file.\n";

int main(int argc, char* argv[])
{
    nodeid_t node = 0xffc0;
    bool nodeSpecified = false;

    FILE* inputFile = NULL;

    for(int i = 1; i < argc; i++)
    {
        if(strncmp(argv[i], "-h", 2) == 0 ||  strncmp(argv[i], "--h", 3) == 0)
        {
            fprintf(stderr, helpText);
            return 1;
        }
        else if(strncmp(argv[i], "-t", 2) == 0)
        {
            node |= atoi(argv[++i]);
            nodeSpecified = true;
        }
        else if(strcmp(argv[i], "-") != 0)
        {
            inputFile = fopen(argv[i], "rb");
        }
    }

    raw1394handle_t handle = raw1394_new_handle_on_port(0);
    int channel;
    int bandwidth = -1;
    
    if(!handle)
    {
        fprintf(stderr, "Failed to get libraw1394 handle\n");
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

    //Set up ncurses
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, true);
    timeout(0);
    
    if(nodeSpecified)
    {
        channel = iec61883_cmp_connect(handle, raw1394_get_local_id(handle), &oplug, node, &iplug, &bandwidth);
        if(channel > -1)
        {
            fprintf(stderr, "Connect succeeded, transmitting on channel %d.\n", channel);
            dv_transmit(handle, inputFile, channel);
            iec61883_cmp_disconnect(handle, raw1394_get_local_id(handle), oplug, node, iplug, channel, bandwidth);
        }
        else
        {
            fprintf(stderr, "Connect failed, reverting to broadcast channel 63.\n");
            dv_transmit(handle, inputFile, 63);
        }
    }
    else
    {
        dv_transmit(handle, inputFile, 63);
    }

    //Quit ncurses
    endwin();

    if(inputFile != stdin)
    {
        fclose(inputFile);
    }

    raw1394_destroy_handle(handle);
    
    return 0;
}
