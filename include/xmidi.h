/*
 Copyright (C) 2000 - 2001  Ryan Nunn
 Copyright (C) 2013 - 2014 Bret Curtis

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

// XMIDI/MIDI Converter/Loader
#ifndef RANDGEN_XMIDI_H
#define RANDGEN_XMIDI_H

#include <stdbool.h> // C99

#include <stddef.h>
#include <string.h>

struct DataSource{
	unsigned char *buf, *buf_ptr;
	unsigned int size;
};


// Conversion types for Midi files
#define XMIDI_CONVERT_NOCONVERSION		0
#define XMIDI_CONVERT_MT32_TO_GM		1
#define XMIDI_CONVERT_MT32_TO_GS		2
#define XMIDI_CONVERT_MT32_TO_GS127		3 // This one is broken so don't use
#define XMIDI_CONVERT_MT32_TO_GS127DRUM	4 // This one is broken so don't use
#define XMIDI_CONVERT_GS127_TO_GS		5

// Midi Status Bytes
#define MIDI_STATUS_NOTE_OFF		0x8
#define MIDI_STATUS_NOTE_ON			0x9
#define MIDI_STATUS_AFTERTOUCH		0xA
#define MIDI_STATUS_CONTROLLER		0xB
#define MIDI_STATUS_PROG_CHANGE		0xC
#define MIDI_STATUS_PRESSURE		0xD
#define MIDI_STATUS_PITCH_WHEEL		0xE
#define MIDI_STATUS_SYSEX			0xF

// XMIDI Controllers
#define XMIDI_CONTROLLER_FOR_LOOP	116
#define XMIDI_CONTROLLER_NEXT_BREAK	117

// Maximum number of for loops we'll allow
#define XMIDI_MAX_FOR_LOOP_COUNT	128

struct midi_event {
	int time;
	unsigned char status;

	unsigned char data[2];

	unsigned int len;
	unsigned char *buffer;

	struct midi_event *next;
};


struct midi_descriptor {
	unsigned short type;
	unsigned short tracks;
};


struct midi_descriptor info;

struct midi_event **events;
signed short *timing;

struct midi_event *list;
struct midi_event *current;

bool bank127[16] = {0};
int convert_type = 0;
bool *fixed;

int number_of_tracks() {
	if (info.type != 1)
		return (info.tracks);
	else
		return (1);
}

// Retrieve it to a data source
int retrieve(unsigned int track, struct DataSource *source, struct DataSource *dest);

// External Event list functions
int retrieveEventList(unsigned int track, struct midi_event **dest, int *ppqn);
void DeleteEventList(struct midi_event *mlist);

// Not yet implemented
// int apply_patch (int track, DataSource *source);


// Private
// List manipulation
void CreateNewEvent(int time);

// Variable length quantity
int GetVLQ(struct DataSource *source, unsigned int quant);
int GetVLQ2(struct DataSource *source, unsigned int quant);
int PutVLQ(struct DataSource *dest, unsigned int value);

void MovePatchVolAndPan(int channel);
void DuplicateAndMerge(int num);

int ConvertEvent(const int time, const unsigned char status,
		struct DataSource *source, const int size);
int ConvertSystemMessage(const int time, const unsigned char status,
		struct DataSource *source);

int ConvertFiletoList(struct DataSource *source, bool is_xmi);
unsigned int ConvertListToMTrk(struct DataSource *dest, struct midi_event *mlist);

int ExtractTracksFromXmi(struct DataSource *source);
int ExtractTracksFromMid(struct DataSource *source);

int ExtractTracks(struct DataSource *source);

#endif //RANDGEN_XMIDI_H
