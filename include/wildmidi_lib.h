/*
	wildmidi_lib.h

 	Midi Wavetable Processing library

    Copyright (C) Chris Ison 2001-2011
    Copyright (C) Bret Curtis 2013-2014

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the player
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
*/

#define WM_MO_LOG_VOLUME	0x0001
#define WM_MO_ENHANCED_RESAMPLING 0x0002
#define WM_MO_REVERB		0x0004
#define WM_MO_WHOLETEMPO      0x8000
#define WM_MO_ROUNDTEMPO      0xA000
#define WM_GS_VERSION		0x0001

#if defined(__cplusplus)
extern "C" {
#endif

struct _WM_Info {
	char *copyright;
	unsigned long int current_sample;
	unsigned long int approx_total_samples;
	unsigned short int mixer_options;
	unsigned long int total_midi_time;
};

typedef void midi;

SYMBOL extern const char * WildMidi_GetString (unsigned short int info);
SYMBOL extern int WildMidi_Init (const char * config_file, unsigned short int rate, unsigned short int options);
SYMBOL extern int WildMidi_MasterVolume (unsigned char master_volume);
SYMBOL extern midi * WildMidi_Open (const char *midifile);
SYMBOL extern midi * WildMidi_OpenBuffer (unsigned char *midibuffer, unsigned long int size);
SYMBOL extern int WildMidi_GetOutput (midi * handle, char * buffer, unsigned long int size);
SYMBOL extern int WildMidi_SetOption (midi * handle, unsigned short int options, unsigned short int setting);
SYMBOL extern struct _WM_Info * WildMidi_GetInfo ( midi * handle );
SYMBOL extern int WildMidi_FastSeek ( midi * handle, unsigned long int *sample_pos);
SYMBOL extern int WildMidi_Close (midi * handle);
SYMBOL extern int WildMidi_Shutdown ( void );
#ifdef _WIN32
 extern char *strdup(const char *str);
#endif

// NOTE: Not Yet Implemented Or Tested Properly
extern int WildMidi_Live(midi * handle, unsigned long int midi_event);


#if defined(__cplusplus)
}
#endif
