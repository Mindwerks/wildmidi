/*
	MUS2MIDI: DMX (DOOM) MUS to MIDI Library

	Copyright (C) 2014  Bret Curtis

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mus.h"
#include "wm_error.h"

#define DST_CHUNK 			8192
#define MIDI_MAXCHANNELS	16
#define MIDIHEADERSIZE		14
#define TEMPO				0x001aa309

uint8_t midimap[] =
{//	MIDI	Number	Description
	0,		//0		// prog change
	0,		//1		// bank sel
	0x01,	//2		// Modulation pot (frequency vibrato depth)
	0x07,	//3		// Volume: 0-silent, ~100-normal, 127-loud
	0x0A,	//4		// Pan (balance) pot: 0-left, 64-center (default), 127-right
	0x0B,	//5		// Expression pot
	0x5B,	//6		// Reverb depth
	0x5D,	//7		// Chorus depth
	0x40,	//8		// Sustain pedal
	0x43,	//9		// Soft pedal
	0x78,	//10	// All sounds off
	0x7B,	//11	// All notes off
	0x7E,	//12	// Mono (use numchannels + 1)
	0x7F,	//13	// Poly
	0x79,	//14	// reset all controllers
};

typedef struct MUSheader {
	char		ID[4];          // identifier "MUS" 0x1A
	uint16_t    scoreLen;
	uint16_t    scoreStart;
	uint16_t    channels;		// count of primary channels
	uint16_t    sec_channels;	// count of secondary channels
	uint16_t    instrCnt;
	uint16_t    dummy;
	/*  variable-length part starts here
	uint16_t	instruments[];
	*/
} MUSheader ;

typedef struct MidiHeaderChunk {
	char name[4];
	int32_t	length;
	int16_t	format;		// make 0
	int16_t	ntracks;	// make 1
	int16_t	division;	// 0xe250??
} MidiHeaderChunk;

typedef struct MidiTrackChunk {
	char name[4];
	int32_t	length;
} MidiTrackChunk;

struct mus_ctx {
	uint8_t *src, *src_ptr;
	uint32_t srcsize;
	uint32_t datastart;
	uint8_t *dst, *dst_ptr;
	uint32_t dstsize, dstrem;
};

static void resize_dst(struct mus_ctx *ctx) {
	uint32_t pos = ctx->dst_ptr - ctx->dst;
	ctx->dst = realloc(ctx->dst, ctx->dstsize + DST_CHUNK);
	ctx->dstsize += DST_CHUNK;
	ctx->dstrem += DST_CHUNK;
	ctx->dst_ptr = ctx->dst + pos;
}

static void write1(struct mus_ctx *ctx, uint32_t val)
{
	if (ctx->dstrem < 1)
		resize_dst(ctx);
	*ctx->dst_ptr++ = val & 0xff;
	ctx->dstrem--;
}

static void write2(struct mus_ctx *ctx, uint32_t val)
{
	if (ctx->dstrem < 2)
		resize_dst(ctx);
	*ctx->dst_ptr++ = (val>>8) & 0xff;
	*ctx->dst_ptr++ = val & 0xff;
	ctx->dstrem -= 2;
}

static void write4(struct mus_ctx *ctx, uint32_t val)
{
	if (ctx->dstrem < 4)
		resize_dst(ctx);
	*ctx->dst_ptr++ = (val>>24)&0xff;
	*ctx->dst_ptr++ = (val>>16)&0xff;
	*ctx->dst_ptr++ = (val>>8) & 0xff;
	*ctx->dst_ptr++ = val & 0xff;
	ctx->dstrem -= 4;
}

uint8_t *mus_getmididata(struct mus_ctx *ctx){
	return ctx->dst;
}

uint32_t mus_getmidisize(struct mus_ctx *ctx){
	return ctx->dstsize - ctx->dstrem;
}

void mus_free(struct mus_ctx *ctx){
	if (!ctx) return;
	free(ctx->dst);
	free(ctx);
}

struct mus_ctx *mus2midi(uint8_t *data, uint32_t size){
	struct mus_ctx *ctx;
	ctx = calloc(1, sizeof(struct mus_ctx));
	ctx->src = ctx->src_ptr = data;
	ctx->srcsize = size;

	ctx->dst = malloc(DST_CHUNK);
	ctx->dst_ptr = ctx->dst;
	ctx->dstsize = DST_CHUNK;
	ctx->dstrem = DST_CHUNK;

	MUSheader header;
	unsigned char* cur = data,* end;
	MidiHeaderChunk midiHeader;		// set to midi format 0
	MidiTrackChunk midiTrackHeader;	// 1 midi track
	uint8_t* midiTrackHeaderOut;	// position of header

	// Delta time for midi event
	int delta_time = 0;
	int temp;
	int channel_volume[MIDI_MAXCHANNELS] = {0};
	int bytes_written = 0;
	int channelMap[MIDI_MAXCHANNELS], currentChannel = 0;
	uint8_t last_status = 0;

	/* read the MUS header and set our location */
	memcpy(&header, ctx->src_ptr, sizeof(header));
	ctx->src_ptr += sizeof(header);

	// TODO: data is stored in little-endian, do we need to convert?

	// we only support 15 channels
	if (header.channels > MIDI_MAXCHANNELS - 1) return NULL;

	// Map channel 15 to 9(percussions)
	for (temp = 0; temp < MIDI_MAXCHANNELS; ++temp) {
		channelMap[temp] = -1;
		channel_volume[temp] = 0x40;
	}
	channelMap[15] = 9;

	// Get current position, and end of position
	cur = data + header.scoreStart;
	end = cur + header.scoreLen;

	/* Header is 14 bytes long and add the rest as well */
	write1(ctx, 'M');
	write1(ctx, 'T');
	write1(ctx, 'h');
	write1(ctx, 'd');
	write4(ctx, 6);			// length of header
	write2(ctx, 0);			// MIDI type (always 0)
	write2(ctx, 1);			// number of tracks
	write2(ctx, 0x0059);	// devision

	// Store this position, for later filling in the midiTrackHeader
	/*
	Midi_UpdateBytesWritten(&bytes_written, sizeof(midiTrackHeader), ctx->dstsize);
	midiTrackHeaderOut = ctx->dst;
	ctx->dst_ptr += sizeof(midiTrackHeader);
 	 */

	/* write tempo: microseconds per quarter note */
	write1(ctx, 0x00);	// delta time
	write1(ctx, 0xff);	// sys command
	write2(ctx, 0x5103); // command - set tempo
	write1(ctx, TEMPO & 0x000000ff);
	write1(ctx, (TEMPO & 0x0000ff00) >> 8);
	write1(ctx, (TEMPO & 0x00ff0000) >> 16);

	// Percussions channel starts out at full volume
	write1(ctx, 0x00);
	write1(ctx, 0xB9);
	write1(ctx, 0x07);
	write1(ctx, 127);

	// Write out track header
	/*
	WriteInt(midiTrackHeader.name, 'MTrk');
	WriteInt(&midiTrackHeader.length, ctx->dst_ptr - midiTrackHeaderOut - sizeof(midiTrackHeader));
	memcpy(midiTrackHeaderOut, &midiTrackHeader, sizeof(midiTrackHeader));
	*/

	// main loop

	// Store length written
	ctx->dstsize = bytes_written;
	/*{
		FILE* file = f o pen("d:\\test.midi", "wb");
		fwrite(midiTrackHeaderOut - sizeof(MidiHeaderChunk_t), bytes_written, 1, file);
		fclose(file);
	}*/

	return ctx;
}
