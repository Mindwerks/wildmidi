/*
	common.h

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

#ifndef __COMMON_H
#define __COMMON_H

#define SAMPLE_16BIT 0x01
#define SAMPLE_UNSIGNED 0x02
#define SAMPLE_LOOP 0x04
#define SAMPLE_PINGPONG 0x08
#define SAMPLE_REVERSE 0x10
#define SAMPLE_SUSTAIN 0x20
#define SAMPLE_ENVELOPE 0x40
#define SAMPLE_CLAMPED 0x80

#ifdef DEBUG_SAMPLES
#define SAMPLE_CONVERT_DEBUG(dx) printf("\r%s\n",dx)
#else
#define SAMPLE_CONVERT_DEBUG(dx)
#endif

extern uint16_t _WM_SampleRate;

struct _sample {
	uint32_t data_length;
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t loop_size;
	uint8_t  loop_fraction;
	uint16_t rate;
	uint32_t freq_low;
	uint32_t freq_high;
	uint32_t freq_root;
	uint8_t  modes;
	int32_t env_rate[7];
	int32_t env_target[7];
	uint32_t inc_div;
	int16_t *data;
	struct _sample *next;
};

struct _env {
	float time;
	float level;
	uint8_t set;
};

struct _patch {
	uint16_t patchid;
	uint8_t loaded;
	char *filename;
	int16_t amp;
	uint8_t keep;
	uint8_t remove;
	struct _env env[6];
	uint8_t  note;
	uint32_t inuse_count;
	struct _sample *first_sample;
	struct _patch *next;
};

/* Set our global defines here */
#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#endif /* __COMMON_H */
