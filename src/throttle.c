/*
    throttle.c - developer benchmark program, not designed for general use
    Copyright (C) 2001-2008 Chris Ison

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the players
    under the terms of the GNU General Public License and you can redistribute
    and/or modify the library under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation, either version 3 of
   the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.
	
    Email: cisos@bigpond.net.au
    	 wildcode@users.sourceforge.net
 
    gcc -Wall -Werror -O3 -funroll-loops -ffast-math -pg -fexpensive-optimizations -fstrict-aliasing -fno-common -g -march=i686-I ../include -o throttle throttle.c wildmidi_lib.c -lm

    $Id: throttle.c,v 1.10 2008/06/04 13:08:27 wildcode Exp $ 
 */

#include <stdio.h>
#include <stdlib.h>

#include "wildmidi_lib.h"

int
main (int argc, char **argv) {
	struct _WM_Info * wm_info = NULL;
	int file = 1;
	int file_count = 0;
	unsigned long int total_time = 0;
	midi * midi_ptr;
	int rate = 32072;
	char * output_buffer;
	unsigned long int count_diff, output_result;
	char config_file[] = "../../guspat/Eawpatches/timidity.cfg\0";
	if (WildMidi_Init (config_file, rate, (WM_MO_ENHANCED_RESAMPLING | WM_MO_REVERB)) != 0) {
		return 0;
	}

	output_buffer = malloc(16384);

	while  (file < argc) {
		printf ("\r%s\n",argv[file]);
		if ((midi_ptr = WildMidi_Open (argv[file])) == NULL) {
			file++;
			continue;
		}
		file_count++;
		wm_info = WildMidi_GetInfo(midi_ptr);
		total_time += wm_info->approx_total_samples / rate;
		WildMidi_LoadSamples(midi_ptr);
		while (1) {
			count_diff = wm_info->approx_total_samples - wm_info->current_sample;
			if (count_diff == 0)
				break;
			if (count_diff < 1024) {
				output_result = WildMidi_GetOutput (midi_ptr, output_buffer, (count_diff * 4));
			} else {
				output_result = WildMidi_GetOutput (midi_ptr, output_buffer, 4096);
			}
			wm_info = WildMidi_GetInfo(midi_ptr);
			
			if (output_result == 0)
				break;
		}
		WildMidi_Close(midi_ptr);
		file++;
	}
	WildMidi_Shutdown();
	free (output_buffer);
	printf("Count %i, Time %lu\n", file_count, total_time);
	return 0;
}
