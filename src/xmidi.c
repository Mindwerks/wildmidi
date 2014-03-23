/*
 Copyright (C) 2000, 2001  Ryan Nunn
 Copyright (C) 2013 Bret Curtis

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
#include "xmidi.h"
#include <stdio.h>
#include <stdlib.h>

static bool bank127[16] = {0};
static int convert_type = 0;

static struct DataSource *dest;
static struct DataSource *source;

int number_of_tracks() {
	if (info.type != 1)
		return (info.tracks);
	else
		return (1);
}

unsigned int read1(struct DataSource *data)
{
	unsigned char b0;
	b0 = (unsigned char)data->buf_ptr++;
	return (b0);
}

unsigned int read2(struct DataSource *data)
{
	unsigned char b0, b1;
	b0 = (unsigned char)data->buf_ptr++;
	b1 = (unsigned char)data->buf_ptr++;
	return (b0 + (b1 << 8));
}

unsigned int read2high(struct DataSource *data)
{
	unsigned char b0, b1;
	b1 = (unsigned char)data->buf_ptr++;
	b0 = (unsigned char)data->buf_ptr++;
	return (b0 + (b1 << 8));
}

unsigned int read4(struct DataSource *data)
{
	unsigned char b0, b1, b2, b3;
	b0 = (unsigned char)data->buf_ptr++;
	b1 = (unsigned char)data->buf_ptr++;
	b2 = (unsigned char)data->buf_ptr++;
	b3 = (unsigned char)data->buf_ptr++;
	return (b0 + (b1<<8) + (b2<<16) + (b3<<24));
}

unsigned int read4high(struct DataSource *data)
{
	unsigned char b0, b1, b2, b3;
	b3 = (unsigned char)data->buf_ptr++;
	b2 = (unsigned char)data->buf_ptr++;
	b1 = (unsigned char)data->buf_ptr++;
	b0 = (unsigned char)data->buf_ptr++;
	printf("data %d\n\r", (b0 + (b1<<8) + (b2<<16) + (b3<<24)) );
	return (b0 + (b1<<8) + (b2<<16) + (b3<<24));
}

void copy(char *b, int len, struct DataSource *data) {
	memcpy(b, data->buf_ptr, len);
	data->buf_ptr += len;
}

void write1(unsigned int val, struct DataSource *data)
{
	data->buf_ptr = val & 0xff;
	data->buf_ptr++;
}

void write2(unsigned int val, struct DataSource *data)
{
	data->buf_ptr = val & 0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>8) & 0xff;
	data->buf_ptr++;
}

void write2high(unsigned int val, struct DataSource *data)
{
	data->buf_ptr = (val>>8) & 0xff;
	data->buf_ptr++;
	data->buf_ptr = val & 0xff;
	data->buf_ptr++;
}


void write4(unsigned int val, struct DataSource *data)
{
	data->buf_ptr = val & 0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>8) & 0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>16)&0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>24)&0xff;
	data->buf_ptr++;
}

void write4high(unsigned int val, struct DataSource *data)
{
	data->buf_ptr = (val>>24)&0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>16)&0xff;
	data->buf_ptr++;
	data->buf_ptr = (val>>8) & 0xff;
	data->buf_ptr++;
	data->buf_ptr = val & 0xff;
}

void seek(unsigned int pos, struct DataSource *data) { data->buf_ptr = data->buf+pos; }

void skip(int pos, struct DataSource *data) { data->buf_ptr += pos; }

unsigned int getSize(struct DataSource *data) { return (data->size); }

unsigned int getPos(struct DataSource *data) { return (data->buf_ptr-data->buf); }

unsigned char *getPtr(struct DataSource *data) { return (data->buf_ptr); }

// This is used to correct incorrect patch, vol and pan changes in midi files
// The bias is just a value to used to work out if a vol and pan belong with a 
// patch change. This is to ensure that the default state of a midi file is with
// the tracks centred, unless the first patch change says otherwise.
#define PATCH_VOL_PAN_BIAS	5

// This is a default set of patches to convert from MT32 to GM
// The index is the MT32 Patch nubmer and the value is the GM Patch
// This is only suitable for music that doesn't do timbre changes
// XMIDIs that contain Timbre changes will not convert properly
const char mt32asgm[128] = { 0,	// 0	Piano 1
		1,	// 1	Piano 2
		2,	// 2	Piano 3 (synth)
		4,	// 3	EPiano 1
		4,	// 4	EPiano 2
		5,	// 5	EPiano 3
		5,	// 6	EPiano 4
		3,	// 7	Honkytonk
		16,	// 8	Organ 1
		17,	// 9	Organ 2
		18,	// 10	Organ 3
		16,	// 11	Organ 4
		19,	// 12	Pipe Organ 1
		19,	// 13	Pipe Organ 2
		19,	// 14	Pipe Organ 3
		21,	// 15	Accordion
		6,	// 16	Harpsichord 1
		6,	// 17	Harpsichord 2
		6,	// 18	Harpsichord 3
		7,	// 19	Clavinet 1
		7,	// 20	Clavinet 2
		7,	// 21	Clavinet 3
		8,	// 22	Celesta 1
		8,	// 23	Celesta 2
		62,	// 24	Synthbrass 1 (62)
		63,	// 25	Synthbrass 2 (63)
		62,	// 26	Synthbrass 3 Bank 8
		63,	// 27	Synthbrass 4 Bank 8
		38,	// 28	Synthbass 1
		39,	// 29	Synthbass 2
		38,	// 30	Synthbass 3 Bank 8
		39,	// 31	Synthbass 4 Bank 8
		88,	// 32	Fantasy
		90,	// 33	Harmonic Pan - No equiv closest is polysynth(90) :(
		52,	// 34	Choral ?? Currently set to SynthVox(54). Should it be ChoirAhhs(52)???
		92,	// 35	Glass
		97,	// 36	Soundtrack
		99,	// 37	Atmosphere
		14,	// 38	Warmbell, sounds kind of like crystal(98) perhaps Tubular Bells(14) would be better. It is!
		54,	// 39	FunnyVox, sounds alot like Bagpipe(109) and Shania(111)
		98,	// 40	EchoBell, no real equiv, sounds like Crystal(98)
		96,	// 41	IceRain
		68,	// 42	Oboe 2001, no equiv, just patching it to normal oboe(68)
		95,	// 43	EchoPans, no equiv, setting to SweepPad
		81,	// 44	DoctorSolo Bank 8
		87,	// 45	SchoolDaze, no real equiv
		112,	// 46	Bell Singer
		80,	// 47	SquareWave
		48,	// 48	Strings 1
		48,	// 49	Strings 2 - should be 49
		44,	// 50	Strings 3 (Synth) - Experimental set to Tremollo Strings - should be 50
		45,	// 51	Pizzicato Strings
		40,	// 52	Violin 1
		40,	// 53	Violin 2 ? Viola
		42,	// 54	Cello 1
		42,	// 55	Cello 2
		43,	// 56	Contrabass
		46,	// 57	Harp 1
		46,	// 58	Harp 2
		24,	// 59	Guitar 1 (Nylon)
		25,	// 60	Guitar 2 (Steel)
		26,	// 61	Elec Guitar 1
		27,	// 62	Elec Guitar 2
		104,	// 63	Sitar
		32,	// 64	Acou Bass 1
		32,	// 65	Acou Bass 2
		33,	// 66	Elec Bass 1
		34,	// 67	Elec Bass 2
		36,	// 68	Slap Bass 1
		37,	// 69	Slap Bass 2
		35,	// 70	Fretless Bass 1
		35,	// 71	Fretless Bass 2
		73,	// 72	Flute 1
		73,	// 73	Flute 2
		72,	// 74	Piccolo 1
		72,	// 75	Piccolo 2
		74,	// 76	Recorder
		75,	// 77	Pan Pipes
		64,	// 78	Sax 1
		65,	// 79	Sax 2
		66,	// 80	Sax 3
		67,	// 81	Sax 4
		71,	// 82	Clarinet 1
		71,	// 83	Clarinet 2
		68,	// 84	Oboe
		69,	// 85	English Horn (Cor Anglais)
		70,	// 86	Bassoon
		22,	// 87	Harmonica
		56,	// 88	Trumpet 1
		56,	// 89	Trumpet 2
		57,	// 90	Trombone 1
		57,	// 91	Trombone 2
		60,	// 92	French Horn 1
		60,	// 93	French Horn 2
		58,	// 94	Tuba
		61,	// 95	Brass Section 1
		61,	// 96	Brass Section 2
		11,	// 97	Vibes 1
		11,	// 98	Vibes 2
		99,	// 99	Syn Mallet Bank 1
		112,	// 100	WindBell no real equiv Set to TinkleBell(112)
		9,	// 101	Glockenspiel
		14,	// 102	Tubular Bells
		13,	// 103	Xylophone
		12,	// 104	Marimba
		107,	// 105	Koto
		111,	// 106	Sho?? set to Shanai(111)
		77,	// 107	Shakauhachi
		78,	// 108	Whistle 1
		78,	// 109	Whistle 2
		76,	// 110	Bottle Blow
		76,	// 111	Breathpipe no real equiv set to bottle blow(76)
		47,	// 112	Timpani
		117,	// 113	Melodic Tom
		116,	// 114	Deap Snare no equiv, set to Taiko(116)
		118,	// 115	Electric Perc 1
		118,	// 116	Electric Perc 2
		116,	// 117	Taiko
		115,	// 118	Taiko Rim, no real equiv, set to Woodblock(115)
		119,	// 119	Cymbal, no real equiv, set to reverse cymbal(119)
		115,	// 120	Castanets, no real equiv, in GM set to Woodblock(115)
		112,	// 121	Triangle, no real equiv, set to TinkleBell(112)
		55,	// 122	Orchestral Hit
		124,	// 123	Telephone
		123,	// 124	BirdTweet
		94,	// 125	Big Notes Pad no equiv, set to halo pad (94)
		98,	// 126	Water Bell set to Crystal Pad(98)
		121	// 127	Jungle Tune set to Breath Noise
		};

// Same as above, except include patch changes
// so GS instruments can be used
const char mt32asgs[256] = { 0, 0,	// 0	Piano 1
		1, 0,	// 1	Piano 2
		2, 0,	// 2	Piano 3 (synth)
		4, 0,	// 3	EPiano 1
		4, 0,	// 4	EPiano 2
		5, 0,	// 5	EPiano 3
		5, 0,	// 6	EPiano 4
		3, 0,	// 7	Honkytonk
		16, 0,	// 8	Organ 1
		17, 0,	// 9	Organ 2
		18, 0,	// 10	Organ 3
		16, 0,	// 11	Organ 4
		19, 0,	// 12	Pipe Organ 1
		19, 0,	// 13	Pipe Organ 2
		19, 0,	// 14	Pipe Organ 3
		21, 0,	// 15	Accordion
		6, 0,	// 16	Harpsichord 1
		6, 0,	// 17	Harpsichord 2
		6, 0,	// 18	Harpsichord 3
		7, 0,	// 19	Clavinet 1
		7, 0,	// 20	Clavinet 2
		7, 0,	// 21	Clavinet 3
		8, 0,	// 22	Celesta 1
		8, 0,	// 23	Celesta 2
		62, 0,	// 24	Synthbrass 1 (62)
		63, 0,	// 25	Synthbrass 2 (63)
		62, 0,	// 26	Synthbrass 3 Bank 8
		63, 0,	// 27	Synthbrass 4 Bank 8
		38, 0,	// 28	Synthbass 1
		39, 0,	// 29	Synthbass 2
		38, 0,	// 30	Synthbass 3 Bank 8
		39, 0,	// 31	Synthbass 4 Bank 8
		88, 0,	// 32	Fantasy
		90, 0,	// 33	Harmonic Pan - No equiv closest is polysynth(90) :(
		52, 0,// 34	Choral ?? Currently set to SynthVox(54). Should it be ChoirAhhs(52)???
		92, 0,	// 35	Glass
		97, 0,	// 36	Soundtrack
		99, 0,	// 37	Atmosphere
		14, 0,// 38	Warmbell, sounds kind of like crystal(98) perhaps Tubular Bells(14) would be better. It is!
		54, 0,	// 39	FunnyVox, sounds alot like Bagpipe(109) and Shania(111)
		98, 0,	// 40	EchoBell, no real equiv, sounds like Crystal(98)
		96, 0,	// 41	IceRain
		68, 0,	// 42	Oboe 2001, no equiv, just patching it to normal oboe(68)
		95, 0,	// 43	EchoPans, no equiv, setting to SweepPad
		81, 0,	// 44	DoctorSolo Bank 8
		87, 0,	// 45	SchoolDaze, no real equiv
		112, 0,	// 46	Bell Singer
		80, 0,	// 47	SquareWave
		48, 0,	// 48	Strings 1
		48, 0,	// 49	Strings 2 - should be 49
		44, 0,// 50	Strings 3 (Synth) - Experimental set to Tremollo Strings - should be 50
		45, 0,	// 51	Pizzicato Strings
		40, 0,	// 52	Violin 1
		40, 0,	// 53	Violin 2 ? Viola
		42, 0,	// 54	Cello 1
		42, 0,	// 55	Cello 2
		43, 0,	// 56	Contrabass
		46, 0,	// 57	Harp 1
		46, 0,	// 58	Harp 2
		24, 0,	// 59	Guitar 1 (Nylon)
		25, 0,	// 60	Guitar 2 (Steel)
		26, 0,	// 61	Elec Guitar 1
		27, 0,	// 62	Elec Guitar 2
		104, 0,	// 63	Sitar
		32, 0,	// 64	Acou Bass 1
		32, 0,	// 65	Acou Bass 2
		33, 0,	// 66	Elec Bass 1
		34, 0,	// 67	Elec Bass 2
		36, 0,	// 68	Slap Bass 1
		37, 0,	// 69	Slap Bass 2
		35, 0,	// 70	Fretless Bass 1
		35, 0,	// 71	Fretless Bass 2
		73, 0,	// 72	Flute 1
		73, 0,	// 73	Flute 2
		72, 0,	// 74	Piccolo 1
		72, 0,	// 75	Piccolo 2
		74, 0,	// 76	Recorder
		75, 0,	// 77	Pan Pipes
		64, 0,	// 78	Sax 1
		65, 0,	// 79	Sax 2
		66, 0,	// 80	Sax 3
		67, 0,	// 81	Sax 4
		71, 0,	// 82	Clarinet 1
		71, 0,	// 83	Clarinet 2
		68, 0,	// 84	Oboe
		69, 0,	// 85	English Horn (Cor Anglais)
		70, 0,	// 86	Bassoon
		22, 0,	// 87	Harmonica
		56, 0,	// 88	Trumpet 1
		56, 0,	// 89	Trumpet 2
		57, 0,	// 90	Trombone 1
		57, 0,	// 91	Trombone 2
		60, 0,	// 92	French Horn 1
		60, 0,	// 93	French Horn 2
		58, 0,	// 94	Tuba
		61, 0,	// 95	Brass Section 1
		61, 0,	// 96	Brass Section 2
		11, 0,	// 97	Vibes 1
		11, 0,	// 98	Vibes 2
		99, 0,	// 99	Syn Mallet Bank 1
		112, 0,	// 100	WindBell no real equiv Set to TinkleBell(112)
		9, 0,	// 101	Glockenspiel
		14, 0,	// 102	Tubular Bells
		13, 0,	// 103	Xylophone
		12, 0,	// 104	Marimba
		107, 0,	// 105	Koto
		111, 0,	// 106	Sho?? set to Shanai(111)
		77, 0,	// 107	Shakauhachi
		78, 0,	// 108	Whistle 1
		78, 0,	// 109	Whistle 2
		76, 0,	// 110	Bottle Blow
		76, 0,	// 111	Breathpipe no real equiv set to bottle blow(76)
		47, 0,	// 112	Timpani
		117, 0,	// 113	Melodic Tom
		116, 0,	// 114	Deap Snare no equiv, set to Taiko(116)
		118, 0,	// 115	Electric Perc 1
		118, 0,	// 116	Electric Perc 2
		116, 0,	// 117	Taiko
		115, 0,	// 118	Taiko Rim, no real equiv, set to Woodblock(115)
		119, 0,	// 119	Cymbal, no real equiv, set to reverse cymbal(119)
		115, 0,	// 120	Castanets, no real equiv, in GM set to Woodblock(115)
		112, 0,	// 121	Triangle, no real equiv, set to TinkleBell(112)
		55, 0,	// 122	Orchestral Hit
		124, 0,	// 123	Telephone
		123, 0,	// 124	BirdTweet
		94, 0,	// 125	Big Notes Pad no equiv, set to halo pad (94)
		98, 0,	// 126	Water Bell set to Crystal Pad(98)
		121, 0	// 127	Jungle Tune set to Breath Noise
		};


// Constructor
/*
XMIDI(struct DataSource *source, int pconvert) :
		events(NULL), timing(NULL), convert_type(pconvert), fixed(NULL) {
	int i = 16;
	while (i--)
		bank127[i] = 0;

	ExtractTracks(source);
}

~XMIDI() {
	if (events) {
		for (int i = 0; i < info.tracks; i++)
			DeleteEventList(events[i]);
		delete[] events;
	}
	if (timing)
		delete[] timing;
	if (fixed)
		delete[] fixed;
}
*/

int retrieve(unsigned int track, struct DataSource *in, struct DataSource *out) {
	int len = 0;
	source = in;
	dest = out;
	ExtractTracks(source);
	/*
	if (!events) {
		printf("No midi data in loaded.\n");
		return (0);
	}
	*/

	// Convert type 1 midi's to type 0
	if (info.type == 1) {
		DuplicateAndMerge(-1);

		for (int i = 0; i < info.tracks; i++)
			DeleteEventList(events[i]);

		free(events);
		events = malloc(sizeof(struct midi_event));
		events[0] = list;

		info.tracks = 1;
		info.type = 0;
	}

	if (track >= info.tracks) {
		printf("Can't retrieve MIDI data, track out of range\n");
		return (0);
	}

	// And fix the midis if they are broken
	if (!fixed[track]) {
		list = events[track];
		MovePatchVolAndPan(-1);
		fixed[track] = true;
		events[track] = list;
	}

	// This is so if using buffer datasource, the caller can know how big to make the buffer
	if (!dest) {
		// Header is 14 bytes long and add the rest as well
		len = ConvertListToMTrk(NULL, events[track]);
		return (14 + len);
	}

	write1('M', dest);
	write1('T', dest);
	write1('h', dest);
	write1('d', dest);

	write4high(6, dest);

	write2high(0, dest);
	write2high(1, dest);
	write2high(timing[track], dest);

	len = ConvertListToMTrk(dest, events[track]);

	return (len + 14);
}

int retrieveEventList(unsigned int track, struct midi_event **dest, int *ppqn) {
	if (!events) {
		printf("No midi data in loaded.\n");
		return (0);
	}

	if ((info.type == 1 && track != 0) || (track >= info.tracks)) {
		printf("Can't retrieve MIDI data, track out of range.\n");
		return (0);
	}
	DuplicateAndMerge(track);
	MovePatchVolAndPan(-1);

	*dest = list;
	ppqn = timing[track];

	return (1);
}

void DeleteEventList(struct midi_event *mlist) {
	struct midi_event *event;
	struct midi_event *next;

	next = mlist;
	event = mlist;

	while ((event = next)) {
		next = event->next;
		free(event);
	}
}

// Sets current to the new event and updates list
void CreateNewEvent(int time) {
	if (!list) {
		list = current = malloc(sizeof(struct midi_event));
		current->next = NULL;
		if (time < 0)
			current->time = 0;
		else
			current->time = time;
		current->buffer = NULL;
		current->len = 0;
		return;
	}

	if (time < 0) {
		struct midi_event *event = malloc(sizeof(struct midi_event));
		event->next = list;
		list = current = event;
		current->time = 0;
		current->buffer = NULL;
		current->len = 0;
		return;
	}

	if (current->time > time)
		current = list;

	while (current->next) {
		if (current->next->time > time) {
			struct midi_event *event = malloc(sizeof(struct midi_event));

			event->next = current->next;
			current->next = event;
			current = event;
			current->time = time;
			current->buffer = NULL;
			current->len = 0;
			return;
		}

		current = current->next;
	}

	current->next = malloc(sizeof(struct midi_event));
	current = current->next;
	current->next = NULL;
	current->time = time;
	current->buffer = NULL;
	current->len = 0;
}

// Conventional Variable Length Quantity
int GetVLQ(struct DataSource *source, unsigned int quant) {
	int i;
	quant = 0;
	unsigned int data;

	for (i = 0; i < 4; i++) {
		data = read1(source);
		quant <<= 7;
		quant |= data & 0x7F;

		if (!(data & 0x80)) {
			i++;
			break;
		}

	}
	return (i);
}

// XMIDI Delta Variable Length Quantity
int GetVLQ2(struct DataSource *source, unsigned int quant) {
	int i;
	quant = 0;
	int data = 0;

	for (i = 0; i < 4; i++) {
		data = read1(source);
		if (data & 0x80) {
			skip(-1, source);
			break;
		}
		quant += data;
	}
	return (i);
}

int PutVLQ(struct DataSource *dest, unsigned int value) {
	int buffer;
	int i = 1;
	buffer = value & 0x7F;
	while (value >>= 7) {
		buffer <<= 8;
		buffer |= ((value & 0x7F) | 0x80);
		i++;
	}
	if (!dest)
		return (i);
	for (int j = 0; j < i; j++) {
		write1(buffer & 0xFF, dest);
		buffer >>= 8;
	}

	return (i);
}

// MovePatchVolAndPan
//
// This little function attempts to correct errors in midi files
// that relate to patch, volume and pan changing
void MovePatchVolAndPan(int channel) {
	if (channel == -1) {
		for (int i = 0; i < 16; i++)
			MovePatchVolAndPan(i);

		return;
	}

	struct midi_event *patch = NULL;
	struct midi_event *vol = NULL;
	struct midi_event *pan = NULL;
	struct midi_event *bank = NULL;
	struct midi_event *temp;

	for (current = list; current;) {
		if (!patch && (current->status >> 4) == 0xC
				&& (current->status & 0xF) == channel)
			patch = current;
		else if (!vol && (current->status >> 4) == 0xB && current->data[0] == 7
				&& (current->status & 0xF) == channel)
			vol = current;
		else if (!pan && (current->status >> 4) == 0xB && current->data[0] == 10
				&& (current->status & 0xF) == channel)
			pan = current;
		else if (!bank && (current->status >> 4) == 0xB && current->data[0] == 0
				&& (current->status & 0xF) == channel)
			bank = current;

		if (pan && vol && patch)
			break;

		if (current)
			current = current->next;
		else
			current = list;
	}

	// Got no patch change, return and don't try fixing it
	if (!patch)
		return;

	// Copy Patch Change Event
	temp = patch;
	patch = malloc(sizeof(struct midi_event));
	patch->time = temp->time;
	patch->status = channel + 0xC0;
	patch->len = 0;
	patch->buffer = NULL;
	patch->data[0] = temp->data[0];

	// Copy Volume
	if (vol
			&& (vol->time > patch->time + PATCH_VOL_PAN_BIAS
					|| vol->time < patch->time - PATCH_VOL_PAN_BIAS))
		vol = NULL;

	temp = vol;
	vol = malloc(sizeof(struct midi_event));
	vol->status = channel + 0xB0;
	vol->data[0] = 7;
	vol->len = 0;
	vol->buffer = NULL;

	if (!temp)
		vol->data[1] = 64;
	else
		vol->data[1] = temp->data[1];

	// Copy Bank
	if (bank
			&& (bank->time > patch->time + PATCH_VOL_PAN_BIAS
					|| bank->time < patch->time - PATCH_VOL_PAN_BIAS))
		bank = NULL;

	temp = bank;

	bank = malloc(sizeof(struct midi_event));
	bank->status = channel + 0xB0;
	bank->data[0] = 0;
	bank->len = 0;
	bank->buffer = NULL;

	if (!temp)
		bank->data[1] = 0;
	else
		bank->data[1] = temp->data[1];

	// Copy Pan
	if (pan
			&& (pan->time > patch->time + PATCH_VOL_PAN_BIAS
					|| pan->time < patch->time - PATCH_VOL_PAN_BIAS))
		pan = NULL;

	temp = pan;
	pan = malloc(sizeof(struct midi_event));
	pan->status = channel + 0xB0;
	pan->data[0] = 10;
	pan->len = 0;
	pan->buffer = NULL;

	if (!temp)
		pan->data[1] = 64;
	else
		pan->data[1] = temp->data[1];

	vol->time = 0;
	pan->time = 0;
	patch->time = 0;
	bank->time = 0;

	bank->next = vol;
	vol->next = pan;
	pan->next = patch;
	patch->next = list;
	list = bank;
}

// DuplicateAndMerge
void DuplicateAndMerge(int num) {
	int i;
	struct midi_event **track;
	int time = 0;
	int start = 0;
	int end = 1;

	if (info.type == 1) {
		start = 0;
		end = info.tracks;
	} else if (num >= 0 && num < info.tracks) {
		start += num;
		end += num;
	}

	track = malloc(sizeof(struct midi_event) * info.tracks);

	for (i = 0; i < info.tracks; i++)
		track[i] = events[i];

	current = list = NULL;

	while (1) {
		int lowest = 1 << 30;
		int selected = -1;
		int num_na = end - start;

		// Firstly we find the track with the lowest time
		// for it's current event
		for (i = start; i < end; i++) {
			if (!track[i]) {
				num_na--;
				continue;
			}
			if (track[i]->time < lowest) {
				selected = i;
				lowest = track[i]->time;
			}
		}

		// This is just so I don't have to type [selected] all the time
		i = selected;

		// None left to convert
		if (!num_na)
			break;

		// Only need 1 end of track
		// So take the last one and ignore the rest;
		if ((num_na != 1) && (track[i]->status == 0xff)
				&& (track[i]->data[0] == 0x2f)) {
			track[i] = NULL;
			continue;
		}

		if (current) {
			current->next = malloc(sizeof(struct midi_event));
			current = current->next;
		} else
			list = current = malloc(sizeof(struct midi_event));

		current->next = NULL;

		time = track[i]->time;
		current->time = time;

		current->status = track[i]->status;
		current->data[0] = track[i]->data[0];
		current->data[1] = track[i]->data[1];

		current->len = track[i]->len;

		if (current->len) {
			current->buffer = malloc(sizeof(unsigned char)*current->len);
			memcpy(current->buffer, track[i]->buffer, current->len);
		} else
			current->buffer = NULL;

		track[i] = track[i]->next;
	}
}

// Converts Events
//
// Source is at the first data byte
// size 1 is single data byte
// size 2 is dual data byte
// size 3 is XMI Note on
// Returns bytes converted

int ConvertEvent(const int time, const unsigned char status,
		struct DataSource *source, const int size) {
	unsigned int delta = 0;
	int data;

	data = read1(source);

	// Bank changes are handled here
	if ((status >> 4) == 0xB && data == 0) {
		data = read1(source);

		bank127[status & 0xF] = false;

		if (convert_type == XMIDI_CONVERT_MT32_TO_GM
				|| convert_type == XMIDI_CONVERT_MT32_TO_GS
				|| convert_type == XMIDI_CONVERT_MT32_TO_GS127
				|| (convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM
						&& (status & 0xF) == 9))
			return (2);

		CreateNewEvent(time);
		current->status = status;
		current->data[0] = 0;
		current->data[1] = data;

		if (convert_type == XMIDI_CONVERT_GS127_TO_GS && data == 127)
			bank127[status & 0xF] = true;

		return (2);
	}

	// Handling for patch change mt32 conversion, probably should go elsewhere
	if ((status >> 4)
			== 0xC&& (status&0xF) != 9 && convert_type != XMIDI_CONVERT_NOCONVERSION) {if (convert_type == XMIDI_CONVERT_MT32_TO_GM)
	{
		data = mt32asgm[data];
	}
	else if ((convert_type == XMIDI_CONVERT_GS127_TO_GS && bank127[status&0xF]) ||
			convert_type == XMIDI_CONVERT_MT32_TO_GS ||
			convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM)
	{
		CreateNewEvent (time);
		current->status = 0xB0 | (status&0xF);
		current->data[0] = 0;
		current->data[1] = mt32asgs[data*2+1];

		data = mt32asgs[data*2];
	}
	else if (convert_type == XMIDI_CONVERT_MT32_TO_GS127)
	{
		CreateNewEvent (time);
		current->status = 0xB0 | (status&0xF);
		current->data[0] = 0;
		current->data[1] = 127;
	}
}	// Drum track handling
else if ((status >> 4) == 0xC && (status&0xF) == 9 &&
		(convert_type == XMIDI_CONVERT_MT32_TO_GS127DRUM || convert_type == XMIDI_CONVERT_MT32_TO_GS127))
{
	CreateNewEvent (time);
	current->status = 0xB9;
	current->data[0] = 0;
	current->data[1] = 127;
}

	CreateNewEvent(time);
	current->status = status;

	current->data[0] = data;

	if (size == 1)
		return (1);

	current->data[1] = read1(source);

	if (size == 2)
		return (2);

	// XMI Note On handling
	struct midi_event *prev = current;
	int i = GetVLQ(source, delta);
	CreateNewEvent(time + delta * 3);

	current->status = status;
	current->data[0] = data;
	current->data[1] = 0;
	current = prev;

	return (i + 2);
}

// Simple routine to convert system messages
int ConvertSystemMessage(const int time, const unsigned char status,
		struct DataSource *source) {
	int i = 0;

	CreateNewEvent(time);
	current->status = status;

	// Handling of Meta events
	if (status == 0xFF) {
		current->data[0] = read1(source);
		i++;
	}

	i += GetVLQ(source, current->len);

	if (!current->len)
		return (i);

	current->buffer = malloc(sizeof(unsigned char)*current->len);

	copy((char *) current->buffer, current->len, source);

	return (i + current->len);
}

// XMIDI and Midi to List
// Returns XMIDI PPQN
int ConvertFiletoList(struct DataSource *source, const bool is_xmi) {
	int time = 0;
	unsigned int data;
	int end = 0;
	int tempo = 500000;
	int tempo_set = 0;
	unsigned int status = 0;
	int play_size = 2;
	unsigned int file_size = getSize(source);

	if (is_xmi)
		play_size = 3;

	// Set Drum track to correct setting if required
	if (convert_type == XMIDI_CONVERT_MT32_TO_GS127) {
		CreateNewEvent(0);
		current->status = 0xB9;
		current->data[0] = 0;
		current->data[1] = 127;
	}

	while (!end && getPos(source) < file_size) {

		if (!is_xmi) {
			GetVLQ(source, data);
			time += data;

			data = read1(source);

			if (data >= 0x80) {
				status = data;
			} else
				skip(-1,source);
		} else {
			GetVLQ2(source, data);
			time += data * 3;

			status = read1(source);
		}

		switch (status >> 4) {
		case MIDI_STATUS_NOTE_ON:
			ConvertEvent(time, status, source, play_size);
			break;

			// 2 byte data
		case MIDI_STATUS_NOTE_OFF:
		case MIDI_STATUS_AFTERTOUCH:
		case MIDI_STATUS_CONTROLLER:
		case MIDI_STATUS_PITCH_WHEEL:
			ConvertEvent(time, status, source, 2);
			break;

			// 1 byte data
		case MIDI_STATUS_PROG_CHANGE:
		case MIDI_STATUS_PRESSURE:
			ConvertEvent(time, status, source, 1);
			break;

		case MIDI_STATUS_SYSEX:
			if (status == 0xFF) {
				int pos = getPos(source);
				unsigned int data = read1(source);

				if (data == 0x2F) // End
					end = 1;
				else if (data == 0x51 && !tempo_set) // Tempo. Need it for PPQN
						{
					skip(1,source);
					tempo = read1(source) << 16;
					tempo += read1(source) << 8;
					tempo += read1(source);
					tempo *= 3;
					tempo_set = 1;
				} else if (data == 0x51 && tempo_set && is_xmi) // Skip any other tempo changes
						{
					GetVLQ(source, data);
					skip(data,source);
					break;
				}

				seek(pos,source);
			}
			ConvertSystemMessage(time, status, source);
			break;

		default:
			break;
		}

	}
	return ((tempo * 3) / 25000);
}

// Converts and event list to a MTrk
// Returns bytes of the array
// buf can be NULL
unsigned int ConvertListToMTrk(struct DataSource *dest, struct midi_event *mlist) {
	int time = 0;
	struct midi_event *event;
	unsigned int delta;
	unsigned char last_status = 0;
	unsigned int i = 8;
	unsigned int j;
	unsigned int size_pos = 0;
	bool end = false;

	if (dest) {
		write1('M', dest);
		write1('T', dest);
		write1('r', dest);
		write1('k', dest);

		size_pos = getPos(dest);
		skip(4, dest);
	}

	for (event = mlist; event && !end; event = event->next) {
		delta = (event->time - time);
		time = event->time;

		i += PutVLQ(dest, delta);

		if ((event->status != last_status) || (event->status >= 0xF0)) {
			if (dest)
				write1(event->status, dest);
			i++;
		}

		last_status = event->status;

		switch (event->status >> 4) {
		// 2 bytes data
		// Note off, Note on, Aftertouch, Controller and Pitch Wheel
		case 0x8:
		case 0x9:
		case 0xA:
		case 0xB:
		case 0xE:
			if (dest) {
				write1(event->data[0], dest);
				write1(event->data[1], dest);
			}
			i += 2;
			break;

			// 1 bytes data
			// Program Change and Channel Pressure
		case 0xC:
		case 0xD:
			if (dest)
				write1(event->data[0], dest);
			i++;
			break;

			// Variable length
			// SysEx
		case 0xF:
			if (event->status == 0xFF) {
				if (event->data[0] == 0x2f)
					end = true;
				if (dest)
					write1(event->data[0], dest);
				i++;
			}

			i += PutVLQ(dest, event->len);

			if (event->len) {
				for (j = 0; j < event->len; j++) {
					if (dest)
						write1(event->buffer[j], dest);
					i++;
				}
			}

			break;

			// Never occur
		default:
			printf("Not supposed to see this.\n");
			break;
		}
	}

	if (dest) {
		int cur_pos = getPos(dest);
		seek(size_pos, dest);
		write4high(i - 8, dest);
		seek(cur_pos, dest);
	}
	return (i);
}

// Assumes correct xmidi
int ExtractTracksFromXmi(struct DataSource *source) {
	int num = 0;
	signed short ppqn;
	unsigned int len = 0;
	char buf[32];

	while (getPos(source) < getSize(source) && num != info.tracks) {
		// Read first 4 bytes of name
		copy(buf, 4, source);
		len = read4high(source);

		// Skip the FORM entries
		if (!memcmp(buf, "FORM", 4)) {
			skip(4,source);
			copy(buf, 4,source);
			len = read4high(source);
		}

		if (memcmp(buf, "EVNT", 4)) {
			skip((len + 1) & ~1, source);
			continue;
		}

		list = NULL;
		int begin = getPos(source);

		// Convert it
		if (!(ppqn = ConvertFiletoList(source, true))) {
			printf("Unable to convert data\n");
			break;
		}
		timing[num] = ppqn;
		events[num] = list;

		// Increment Counter
		num++;

		// go to start of next track
		seek(begin + ((len + 1) & ~1), source);
	}

	// Return how many were converted
	return (num);
}

int ExtractTracksFromMid(struct DataSource *source) {
	int num = 0;
	unsigned int len = 0;
	char buf[32];

	while (getPos(source) < getSize(source) && num != info.tracks) {
		// Read first 4 bytes of name
		copy(buf, 4, source);
		len = read4high(source);

		if (memcmp(buf, "MTrk", 4)) {
			skip(len, source);
			continue;
		}

		list = NULL;
		int begin = getPos(source);

		// Convert it
		if (!ConvertFiletoList(source, false)) {
			printf("Unable to convert data.\n");
			break;
		}

		events[num] = list;

		// Increment Counter
		num++;
		seek(begin + len,source);
	}

	// Return how many were converted
	return (num);
}

int ExtractTracks(struct DataSource *source) {
	unsigned int i = 0;
	int start;
	unsigned int len;
	unsigned int chunk_len;
	int count;
	char buf[32];

	// Read first 4 bytes of header
	copy(buf, 4, source);

	// Could be XMIDI
	if (!memcmp(buf, "FORM", 4)) {
		// Read length of 
		len = read4high(source);

		start = getPos(source);

		// Read 4 bytes of type
		copy(buf, 4, source);

		// XDIRless XMIDI, we can handle them here.
		if (!memcmp(buf, "XMID", 4)) {
			printf("Warning: XMIDI doesn't have XDIR.\n");
			info.tracks = 1;

		} // Not an XMIDI that we recognise
		else if (memcmp(buf, "XDIR", 4)) {
			printf("Not a recognised XMID.\n");
			return (0);

		} // Seems Valid
		else {
			info.tracks = 0;

			for (i = 4; i < len; i++) {
				// Read 4 bytes of type
				copy(buf, 4, source);

				// Read length of chunk
				chunk_len = read4high(source);

				// Add eight bytes
				i += 8;

				if (memcmp(buf, "INFO", 4)) {
					// Must allign
					skip((chunk_len + 1) & ~1, source);
					i += (chunk_len + 1) & ~1;
					continue;
				}

				// Must be at least 2 bytes long
				if (chunk_len < 2)
					break;

				info.tracks = read2(source);
				break;
			}

			// Didn't get to fill the header
			if (info.tracks == 0) {
				printf("Not a valid XMID.\n");
				return (0);
			}

			// Ok now to start part 2
			// Goto the right place
			printf("source %s %d\n\r", source->buf_ptr, len);
			printf("len %d %d\n\r", len, (len + 1) & ~1);
			//seek(start + ((len + 1) & ~1), source);
			printf("source %s - %d %d %d\n\r", source->buf_ptr, start, (len + 1),  ((len + 1) & ~1));

			// Read 4 bytes of type
			copy(buf, 4, source);

			// Not an XMID
			if (memcmp(buf, "CAT ", 4)) {
				printf("Not a recognised XMID (%s%s%s%s) should be (CAT )\n", buf[0],buf[1],buf[2],buf[3]);
				return (0);
			}

			// Now read length of this track
			len = read4high(source);

			// Read 4 bytes of type
			copy(buf, 4, source);

			// Not an XMID
			if (memcmp(buf, "XMID", 4)) {
				printf("Not a recognised XMID (%s%s%s%s) should be (XMID)\n", buf[0],buf[1],buf[2],buf[3]);
				return (0);
			}

		}

		// Ok it's an XMID, so pass it to the ExtractCode

		events = malloc(sizeof(struct midi_event) *info.tracks);
		timing = malloc(sizeof(signed short)*info.tracks);
		fixed = malloc(sizeof(bool)*info.tracks);
		info.type = 0;

		for (i = 0; i < info.tracks; i++) {
			events[i] = NULL;
			fixed[i] = false;
		}

		count = ExtractTracksFromXmi(source);

		if (count != info.tracks) {
			printf("Error: unable to extract all (%d) tracks specified from XMIDI. Only (%d)", info.tracks, count);

			int i = 0;

			for (i = 0; i < info.tracks; i++)
				DeleteEventList(events[i]);

			free(events);
			free(timing);

			return (0);
		}

		return (1);

	}			// Definately a Midi
	else if (!memcmp(buf, "MThd", 4)) {
		// Simple read length of header
		len = read4high(source);

		if (len < 6) {
			printf("Not a valid MIDI\n");
			return (0);
		}

		info.type = read2high(source);

		info.tracks = read2high(source);

		events = malloc(sizeof(struct midi_event) *info.tracks);
		timing = malloc(sizeof(signed short)*info.tracks);
		fixed = malloc(sizeof(bool)*info.tracks);
		timing[0] = read2high(source);
		for (i = 0; i < info.tracks; i++) {
			timing[i] = timing[0];
			events[i] = NULL;
			fixed[i] = false;
		}

		count = ExtractTracksFromMid(source);

		if (count != info.tracks) {
			printf("Error: unable to extract all (%s) tracks specified from MIDI. Only (%s)", info.tracks, count);

			for (i = 0; i < info.tracks; i++)
				DeleteEventList(events[i]);

			free(events);
			free(timing);

			return (0);

		}

		return (1);

	}// A RIFF Midi, just pass the source back to this function at the start of the midi file
	else if (!memcmp(buf, "RIFF", 4)) {
		// Read len
		len = read4(source);

		// Read 4 bytes of type
		copy(buf, 4, source);

		// Not an RMID
		if (memcmp(buf, "RMID", 4)) {
			printf("Invalid RMID\n");
			return (0);
		}

		// Is a RMID

		for (i = 4; i < len; i++) {
			// Read 4 bytes of type
			copy(buf, 4, source);

			chunk_len = read4(source);

			i += 8;

			if (memcmp(buf, "data", 4)) {
				// Must allign
				skip((chunk_len + 1) & ~1, source);
				i += (chunk_len + 1) & ~1;
				continue;
			}

			return (ExtractTracks(source));

		}

		printf("Failed to find midi data in RIFF Midi\n");
		return (0);
	}

	return (0);
}

