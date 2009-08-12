/*
    midi.c - parse MIDI file
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

    $Id: midi.c,v 1.4 2008/06/04 13:08:27 wildcode Exp $
*/

#include "midi.h"
#include "file_io.h"

#include <stdlib.h>

struct _midi_events {
	unsigned long int packed_event;
	unsigned long int time_to_next;
	struct _midi_events *next;
};

static inline void
free_events (struct _midi_events * midi_events) {
	struct _midi_events *next_event;
	
	if (midi_events != NULL) {
		do {
			next_event = midi_events->next;
			free (midi_events);
			midi_events = next_event;
		} while (midi_events != NULL);
	}
	return;
}

#define MFERR_TOOSHORT -1
#define MFERR_NOTMIDI -2
#define MFERR_NOTSUP -3
#define MFERR_INVEOT -4

static inline int
verify_track (unsigned char *trackdata, unsigned long int tracksize) {
	unsigned char * trackptr = trackdata;
	unsigned char r_event = 0;
	unsigned char delta = 0;
	unsigned long int trackcnt = 0;
	
	do {
		if ((delta = trackptr[0]) & 0x80) {
			delta &= 0x7f;
			do {
				trackptr++;
				trackcnt++;
				delta = (delta << 7) | (trackptr[0] & 0x7f);
			} while (trackptr[0] & 80);
		}
		trackptr++;
		trackcnt++;
		
		switch (trackptr[0] >> 8) {
			case 0x8:
			case 0x9:
			case 0xA:
			case 0xB:
			case 0xC:
			case 0xD:
			case 0xE:
			case 0xF:
			default:
		}
	} while (trackcnt < tracksize);
	return 0;
}

static inline int
verify_headers (unsigned char * mididata, unsigned long int midisize) {
	unsigned char * midiptr = mididata;
	unsigned long int tmpint = 0;
	unsigned long int miditype = 0;
	unsigned long int miditracks = 0;
	unsigned long int trackcnt = 0;
	
	
	if (midisize < 25) {
		return MFERR_TOOSHORT;
	}
	
	if (strncmp(midiptr, "MThd", 4) != 0) {
		return MFERR_NOTMIDI;
	}
	midiptr += 4;
	midisize -= 4;
	
	tmpint = (midiptr[0] << 24) | (midiptr[1] << 16) | (midiptr[2] << 8) | (midiptr[3]);
	if (tmpint != 6) {
		return MFERR_NOTMIDI;
	}
	midiptr += 4;
	midisize -= 4;
	
	tmpint = (midiptr[0] << 8) | (midiptr[1]);
	if (tmpint > 1) {
		return MFERR_NOTSUP;
	}
	miditype = tmp_int;
	midiptr +=2;
	midisize -= 2;
	
	tmpint = (midiptr[0] << 8) | (midiptr[1]);
	if (tmpint < 1) || ((miditype = 0) && (tmpint > 1)) {
		return MFERR_NOTSUP;
	}
	miditracks = tmpint;
	midiptr += 2;
	midisize -= 2;

	for (trackcnt = 0; trackcnt < miditracks; trackcnt++) {
		if (strncmp(midiptr, "MTrk", 4) != 0) {
			return 
		}
		midiptr += 4;
		midisize -= 4;
		
		tmpint = (midiptr[0] << 24) | (midiptr[1] << 16) | (midiptr[2] << 8) | (midiptr[3]);
		if (tmpint > midisize) {
			return MFERR_TOOSHORT;
		}
		midiptr += 4;
		midisize -= 4;
		
		if ((midiptr[tmpint-3] != 0xFF) && (midiptr[tmpint-2] != 0x2F) && (midiptr[tmpint-1]!= 0x00)) {
			return MFERR_INVEOT;
		}
		midiptr += tmpint;
		midisize -= tmpint;
	}
	
	return 0;
}

/*
	
*/
struct _midi_events *
WM_Load_MIDIFile (unsigned char *midifile) {
		struct _midi_events *ret_events = NULL;
		unsigned char *filedata = NULL;
		unsigned long int filesize = 0;
		int midichk = 0;
		
		if ((filedata = WM_BufferFile(midifile, &filesize)) == NULL) {
			/* ERROR */
			return NULL;
		}
		
		if ((midichk = verify_headers(filedata, filesize)) != 0) {
			switch midichk {
				case MFERR_TOOSHORT:
					printf("ERROR: MIDI File On Diet, Too Thin\n\n");
				case MFERR_NOTMIDI:
					printf("ERROR: User Suffering Dementia, This Is Not A MIDI File\n\n");
					break;
				case MFERR_NOTSUP:
					printf("ERROR: MIDI Format NOT Supported\n\n");
					break;
				case MFERR_INVEOT:
					printf("ERROR: MIDI File Suffering Dementia, EOT not where it should be\n\n");
					break;
				default:
					break;
			}
			free (filedata);
			return NULL;
		}
		
		free (filedata);
		return ret_events;
}
