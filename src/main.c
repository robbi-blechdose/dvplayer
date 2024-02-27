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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libiec61883/iec61883.h>
#include <stdio.h>
#include <sys/poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

static int g_done = 0;

static int read_frame (unsigned char *data, int n, unsigned int dropped, void *callback_data)
{
	FILE *f = (FILE*) callback_data;

	if (n == 1)
		if (fread (data, 480, 1, f) < 1)
			return -1;
		else
			return 0;
	else
		return 0;
}

static void sighandler (int sig)
{
	g_done = 1;
}

static void dv_transmit( raw1394handle_t handle, FILE *f, int channel)
{	
	iec61883_dv_t dv;
	unsigned char data[480];
	int ispal;
	
	fread (data, 480, 1, f);
	ispal = (data[ 3 ] & 0x80) != 0;
	dv = iec61883_dv_xmit_init (handle, ispal, read_frame, (void *)f );
	
	if (dv && iec61883_dv_xmit_start (dv, channel) == 0)
	{
		struct pollfd pfd = {
			fd: raw1394_get_fd (handle),
			events: POLLIN,
			revents: 0
		};
		int result = 0;
		
		signal (SIGINT, sighandler);
		signal (SIGPIPE, sighandler);
		fprintf (stderr, "Starting to transmit %s\n", ispal ? "PAL" : "NTSC");

		do {
			int r = 0;
			if ((r = poll (&pfd, 1, 100)) > 0 && (pfd.revents & POLLIN))
				result = raw1394_loop_iterate (handle);
			
		} while (g_done == 0 && result == 0);
		
		fprintf (stderr, "done.\n");
	}
	iec61883_dv_close (dv);
}

int main (int argc, char *argv[])
{
	raw1394handle_t handle = raw1394_new_handle_on_port (0);
	nodeid_t node = 0xffc0;
	FILE *f = NULL;
	int node_specified = 0;
	int i;
	int channel;
	int bandwidth = -1;
	
	for (i = 1; i < argc; i++) {
		
		if (strncmp (argv[i], "-h", 2) == 0 || 
			strncmp (argv[i], "--h", 3) == 0)
		{
			fprintf (stderr, 
			"usage: %s [[-r | -t] node-id] [- | file]\n"
			"       Use - to transmit raw DV from stdin, or\n"
			"       supply a filename to to transmit from a raw DV file.\n", argv[0]);
			raw1394_destroy_handle (handle);
			return 1;
		} else if (strncmp (argv[i], "-t", 2) == 0) {
			node |= atoi (argv[++i]);
			node_specified = 1;
		} else if (strcmp (argv[i], "-") != 0) {
			f = fopen (argv[i], "rb");
		}
	}
		
	if (handle) {
		int oplug = -1, iplug = -1;
		
		if (f == NULL)
			f = stdin;
		if (node_specified) {
			channel = iec61883_cmp_connect (handle,
				raw1394_get_local_id (handle), &oplug, node, &iplug,
				&bandwidth);
			if (channel > -1) {
				fprintf (stderr, "Connect succeeded, transmitting on channel %d.\n", channel);
				dv_transmit (handle, f, channel);
				iec61883_cmp_disconnect (handle,
					raw1394_get_local_id (handle), oplug, node, iplug,
					channel, bandwidth);
			} else {
				fprintf (stderr, "Connect failed, reverting to broadcast channel 63.\n");
				dv_transmit (handle, f, 63);
			}
		} else {
			dv_transmit (handle, f, 63);
		}
		if (f != stdin)
			fclose (f);
		raw1394_destroy_handle (handle);
	} else {
		fprintf (stderr, "Failed to get libraw1394 handle\n");
		return -1;
	}
	
	return 0;
}
