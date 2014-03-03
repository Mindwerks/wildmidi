/*
 wildmidi.c
 Midi Player using the WildMidi Midi Processing Library

 Copyright (C) Chris Ison  2001-2011
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

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(_WIN32) && !defined(__DJGPP__)
# if (defined __gnu_hurd__)
# define __USE_XOPEN 1
# endif
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
static int msleep(unsigned long millisec);
#endif

#if defined(__DJGPP__)
#include "getopt_long.h"
#include <conio.h>
#define getopt dj_getopt /* hack */
#include <unistd.h>
#undef getopt
#define msleep(s) usleep((s)*1000)
#include <io.h>
#endif

#if (defined _WIN32) || (defined __CYGWIN__)
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#define msleep(s) Sleep((s))
#undef strdup
#define strdup _strdup
#include <io.h>
#include "getopt_long.h"
#else
# ifdef AUDIODRV_ALSA
#  include <alsa/asoundlib.h>
# elif defined AUDIODRV_OSS
#   if defined HAVE_SYS_SOUNDCARD_H
#   include <sys/soundcard.h>
#   elif defined HAVE_LINUX_SOUNDCARD_H
#   include <linux/soundcard.h>
#   elif defined HAVE_MACHINE_SOUNDCARD_H
#   include <machine/soundcard.h>
#   endif
# elif defined AUDIODRV_OPENAL
#   include <al.h>
#   include <alc.h>
# endif
#endif

#ifndef FNONBLOCK
#define FNONBLOCK O_NONBLOCK
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#include "wildmidi_lib.h"
#include "filenames.h"

struct _midi_test {
	unsigned char *data;
	unsigned long int size;
};

/* scale test from 0 to 127
 * test a
 * offset 18-21 (0x12-0x15) - track size
 * offset 25 (0x1A) = bank number
 * offset 28 (0x1D) = patch number
 */
static unsigned char midi_test_c_scale[] = {
	0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, /* 0x00    */
	0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x4d, 0x54, /* 0x08    */
	0x72, 0x6b, 0x00, 0x00, 0x02, 0x63, 0x00, 0xb0, /* 0x10    */
	0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x90, 0x00,	/* 0x18  C */
	0x64, 0x08, 0x80, 0x00, 0x00, 0x08, 0x90, 0x02,	/* 0x20  D */
	0x64, 0x08, 0x80, 0x02, 0x00, 0x08, 0x90, 0x04,	/* 0x28  E */
	0x64, 0x08, 0x80, 0x04, 0x00, 0x08, 0x90, 0x05,	/* 0x30  F */
	0x64, 0x08, 0x80, 0x05, 0x00, 0x08, 0x90, 0x07,	/* 0x38  G */
	0x64, 0x08, 0x80, 0x07, 0x00, 0x08, 0x90, 0x09,	/* 0x40  A */
	0x64, 0x08, 0x80, 0x09, 0x00, 0x08, 0x90, 0x0b,	/* 0x48  B */
	0x64, 0x08, 0x80, 0x0b, 0x00, 0x08, 0x90, 0x0c,	/* 0x50  C */
	0x64, 0x08, 0x80, 0x0c, 0x00, 0x08, 0x90, 0x0e,	/* 0x58  D */
	0x64, 0x08, 0x80, 0x0e, 0x00, 0x08, 0x90, 0x10,	/* 0x60  E */
	0x64, 0x08, 0x80, 0x10, 0x00, 0x08, 0x90, 0x11,	/* 0x68  F */
	0x64, 0x08, 0x80, 0x11, 0x00, 0x08, 0x90, 0x13,	/* 0x70  G */
	0x64, 0x08, 0x80, 0x13, 0x00, 0x08, 0x90, 0x15,	/* 0x78  A */
	0x64, 0x08, 0x80, 0x15, 0x00, 0x08, 0x90, 0x17,	/* 0x80  B */
	0x64, 0x08, 0x80, 0x17, 0x00, 0x08, 0x90, 0x18,	/* 0x88  C */
	0x64, 0x08, 0x80, 0x18, 0x00, 0x08, 0x90, 0x1a,	/* 0x90  D */
	0x64, 0x08, 0x80, 0x1a, 0x00, 0x08, 0x90, 0x1c,	/* 0x98  E */
	0x64, 0x08, 0x80, 0x1c, 0x00, 0x08, 0x90, 0x1d,	/* 0xA0  F */
	0x64, 0x08, 0x80, 0x1d, 0x00, 0x08, 0x90, 0x1f,	/* 0xA8  G */
	0x64, 0x08, 0x80, 0x1f, 0x00, 0x08, 0x90, 0x21,	/* 0xB0  A */
	0x64, 0x08, 0x80, 0x21, 0x00, 0x08, 0x90, 0x23,	/* 0xB8  B */
	0x64, 0x08, 0x80, 0x23, 0x00, 0x08, 0x90, 0x24,	/* 0xC0  C */
	0x64, 0x08, 0x80, 0x24, 0x00, 0x08, 0x90, 0x26,	/* 0xC8  D */
	0x64, 0x08, 0x80, 0x26, 0x00, 0x08, 0x90, 0x28,	/* 0xD0  E */
	0x64, 0x08, 0x80, 0x28, 0x00, 0x08, 0x90, 0x29,	/* 0xD8  F */
	0x64, 0x08, 0x80, 0x29, 0x00, 0x08, 0x90, 0x2b,	/* 0xE0  G */
	0x64, 0x08, 0x80, 0x2b, 0x00, 0x08, 0x90, 0x2d,	/* 0xE8  A */
	0x64, 0x08, 0x80, 0x2d, 0x00, 0x08, 0x90, 0x2f,	/* 0xF0  B */
	0x64, 0x08, 0x80, 0x2f, 0x00, 0x08, 0x90, 0x30,	/* 0xF8  C */
	0x64, 0x08, 0x80, 0x30, 0x00, 0x08, 0x90, 0x32,	/* 0x100 D */
	0x64, 0x08, 0x80, 0x32, 0x00, 0x08, 0x90, 0x34,	/* 0x108 E */
	0x64, 0x08, 0x80, 0x34, 0x00, 0x08, 0x90, 0x35,	/* 0x110 F */
	0x64, 0x08, 0x80, 0x35, 0x00, 0x08, 0x90, 0x37,	/* 0x118 G */
	0x64, 0x08, 0x80, 0x37, 0x00, 0x08, 0x90, 0x39,	/* 0x120 A */
	0x64, 0x08, 0x80, 0x39, 0x00, 0x08, 0x90, 0x3b,	/* 0X128 B */
	0x64, 0x08, 0x80, 0x3b, 0x00, 0x08, 0x90, 0x3c,	/* 0x130 C */
	0x64, 0x08, 0x80, 0x3c, 0x00, 0x08, 0x90, 0x3e,	/* 0x138 D */
	0x64, 0x08, 0x80, 0x3e, 0x00, 0x08, 0x90, 0x40,	/* 0X140 E */
	0x64, 0x08, 0x80, 0x40, 0x00, 0x08, 0x90, 0x41,	/* 0x148 F */
	0x64, 0x08, 0x80, 0x41, 0x00, 0x08, 0x90, 0x43,	/* 0x150 G */
	0x64, 0x08, 0x80, 0x43, 0x00, 0x08, 0x90, 0x45,	/* 0x158 A */
	0x64, 0x08, 0x80, 0x45, 0x00, 0x08, 0x90, 0x47,	/* 0x160 B */
	0x64, 0x08, 0x80, 0x47, 0x00, 0x08, 0x90, 0x48,	/* 0x168 C */
	0x64, 0x08, 0x80, 0x48, 0x00, 0x08, 0x90, 0x4a,	/* 0x170 D */
	0x64, 0x08, 0x80, 0x4a, 0x00, 0x08, 0x90, 0x4c,	/* 0x178 E */
	0x64, 0x08, 0x80, 0x4c, 0x00, 0x08, 0x90, 0x4d,	/* 0x180 F */
	0x64, 0x08, 0x80, 0x4d, 0x00, 0x08, 0x90, 0x4f,	/* 0x188 G */
	0x64, 0x08, 0x80, 0x4f, 0x00, 0x08, 0x90, 0x51,	/* 0x190 A */
	0x64, 0x08, 0x80, 0x51, 0x00, 0x08, 0x90, 0x53,	/* 0x198 B */
	0x64, 0x08, 0x80, 0x53, 0x00, 0x08, 0x90, 0x54,	/* 0x1A0 C */
	0x64, 0x08, 0x80, 0x54, 0x00, 0x08, 0x90, 0x56,	/* 0x1A8 D */
	0x64, 0x08, 0x80, 0x56, 0x00, 0x08, 0x90, 0x58,	/* 0x1B0 E */
	0x64, 0x08, 0x80, 0x58, 0x00, 0x08, 0x90, 0x59,	/* 0x1B8 F */
	0x64, 0x08, 0x80, 0x59, 0x00, 0x08, 0x90, 0x5b,	/* 0x1C0 G */
	0x64, 0x08, 0x80, 0x5b, 0x00, 0x08, 0x90, 0x5d,	/* 0x1C8 A */
	0x64, 0x08, 0x80, 0x5d, 0x00, 0x08, 0x90, 0x5f,	/* 0x1D0 B */
	0x64, 0x08, 0x80, 0x5f, 0x00, 0x08, 0x90, 0x60,	/* 0x1D8 C */
	0x64, 0x08, 0x80, 0x60, 0x00, 0x08, 0x90, 0x62,	/* 0x1E0 D */
	0x64, 0x08, 0x80, 0x62, 0x00, 0x08, 0x90, 0x64,	/* 0x1E8 E */
	0x64, 0x08, 0x80, 0x64, 0x00, 0x08, 0x90, 0x65,	/* 0x1F0 F */
	0x64, 0x08, 0x80, 0x65, 0x00, 0x08, 0x90, 0x67,	/* 0x1F8 G */
	0x64, 0x08, 0x80, 0x67, 0x00, 0x08, 0x90, 0x69,	/* 0x200 A */
	0x64, 0x08, 0x80, 0x69, 0x00, 0x08, 0x90, 0x6b,	/* 0x208 B */
	0x64, 0x08, 0x80, 0x6b, 0x00, 0x08, 0x90, 0x6c,	/* 0x210 C */
	0x64, 0x08, 0x80, 0x6c, 0x00, 0x08, 0x90, 0x6e,	/* 0x218 D */
	0x64, 0x08, 0x80, 0x6e, 0x00, 0x08, 0x90, 0x70,	/* 0x220 E */
	0x64, 0x08, 0x80, 0x70, 0x00, 0x08, 0x90, 0x71,	/* 0x228 F */
	0x64, 0x08, 0x80, 0x71, 0x00, 0x08, 0x90, 0x73,	/* 0x230 G */
	0x64, 0x08, 0x80, 0x73, 0x00, 0x08, 0x90, 0x75,	/* 0x238 A */
	0x64, 0x08, 0x80, 0x75, 0x00, 0x08, 0x90, 0x77,	/* 0x240 B */
	0x64, 0x08, 0x80, 0x77, 0x00, 0x08, 0x90, 0x78,	/* 0x248 C */
	0x64, 0x08, 0x80, 0x78, 0x00, 0x08, 0x90, 0x7a,	/* 0x250 D */
	0x64, 0x08, 0x80, 0x7a, 0x00, 0x08, 0x90, 0x7c,	/* 0x258 E */
	0x64, 0x08, 0x80, 0x7c, 0x00, 0x08, 0x90, 0x7d,	/* 0x260 F */
	0x64, 0x08, 0x80, 0x7d, 0x00, 0x08, 0x90, 0x7f,	/* 0x268 G */
	0x64, 0x08, 0x80, 0x7f, 0x00, 0x08, 0xff, 0x2f, /* 0x270   */
	0x00 /* 0x278   */
};

static struct _midi_test midi_test[] = {
	{ midi_test_c_scale, 663 },
	{ NULL, 0 }
};

static int midi_test_max = 1;

/*
 ==============================
 Audio Output Functions

 We have two 'drivers': first is the wav file writer which is
 always available. the second, if it is really compiled in,
 is the system audio output driver. only _one of the two_ can
 be active, not both.
 ==============================
 */

static unsigned int rate = 32072;
static char *pcmname = NULL;

static int (*send_output)(char * output_data, int output_size);
static void (*close_output)(void);
static void (*pause_output)(void);
static int audio_fd = -1;

static void pause_output_nop(void) {
}

/*
 Wav Output Functions
 */

static char wav_file[1024] = "\0";
static unsigned long int wav_size;

static int write_wav_output(char * output_data, int output_size);
static void close_wav_output(void);

static int open_wav_output(void) {

	unsigned char wav_hdr[] = { 0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,
			0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00,
			0x00, 0x01, 0x00, 0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1,
			0x02, 0x00, 0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x00,
			0x00, 0x00, 0x00 };

	if (wav_file[0] == '\0')
		return (-1);
#if defined(_WIN32) || defined(__DJGPP__)
	if ((audio_fd = open(wav_file, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0666)) < 0) {
#else
	if ((audio_fd = open(wav_file, (O_RDWR | O_CREAT | O_TRUNC),
			(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))) < 0) {
#endif
		fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(errno));
		return (-1);
	} else {
		unsigned long int bytes_per_sec;

		wav_hdr[24] = (rate) & 0xFF;
		wav_hdr[25] = (rate >> 8) & 0xFF;

		bytes_per_sec = rate * 4;
		wav_hdr[28] = (bytes_per_sec) & 0xFF;
		wav_hdr[29] = (bytes_per_sec >> 8) & 0xFF;
		wav_hdr[30] = (bytes_per_sec >> 16) & 0xFF;
		wav_hdr[31] = (bytes_per_sec >> 24) & 0xFF;
	}

	if (write(audio_fd, &wav_hdr, 44) < 0) {
		fprintf(stderr, "ERROR: failed writing wav header (%s)\r\n", strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return (-1);
	}

	wav_size = 0;
	send_output = write_wav_output;
	close_output = close_wav_output;
	pause_output = pause_output_nop;
	return (0);
}

static int write_wav_output(char * output_data, int output_size) {
#ifdef WORDS_BIGENDIAN
/* libWildMidi outputs host-endian, *.wav must have little-endian. */
	unsigned short *swp = (unsigned short *) output_data;
	int i = (output_size / 2) - 1;
	for (; i >= 0; --i) {
		swp[i] = (swp[i] << 8) | (swp[i] >> 8);
	}
#endif
	if (write(audio_fd, output_data, output_size) < 0) {
		fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
		close(audio_fd);
		audio_fd = -1;
		return (-1);
	}

	wav_size += output_size;
	return (0);
}

static void close_wav_output(void) {
	char wav_count[4];
	if (audio_fd < 0)
		return;

	printf("Finishing and closing wav output\r");
	wav_count[0] = (wav_size) & 0xFF;
	wav_count[1] = (wav_size >> 8) & 0xFF;
	wav_count[2] = (wav_size >> 16) & 0xFF;
	wav_count[3] = (wav_size >> 24) & 0xFF;
	lseek(audio_fd, 40, SEEK_SET);
	if (write(audio_fd, &wav_count, 4) < 0) {
		fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
		goto end;
	}

	wav_size += 36;
	wav_count[0] = (wav_size) & 0xFF;
	wav_count[1] = (wav_size >> 8) & 0xFF;
	wav_count[2] = (wav_size >> 16) & 0xFF;
	wav_count[3] = (wav_size >> 24) & 0xFF;
	lseek(audio_fd, 4, SEEK_SET);
	if (write(audio_fd, &wav_count, 4) < 0) {
		fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
		goto end;
	}

end:	printf("\n");
	if (audio_fd >= 0)
		close(audio_fd);
	audio_fd = -1;
}

#if (defined _WIN32) || (defined __CYGWIN__)

static HWAVEOUT hWaveOut = NULL;
static CRITICAL_SECTION waveCriticalSection;

#define open_audio_output open_mm_output
static int write_mm_output (char * output_data, int output_size);
static void close_mm_output (void);

static WAVEHDR *mm_blocks = NULL;
#define MM_BLOCK_SIZE 16384
#define MM_BLOCK_COUNT 3

static unsigned long int mm_free_blocks = MM_BLOCK_COUNT;
static unsigned long int mm_current_block = 0;

#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef DWORD DWORD_PTR;
#endif

static void CALLBACK mmOutProc (HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	int* freeBlockCounter = (int*)dwInstance;
	HWAVEOUT tmp_hWaveOut = hWaveOut;
	DWORD tmp_dwParam1 = dwParam1;
	DWORD tmp_dwParam2 = dwParam2;

	tmp_hWaveOut = hWaveOut;
	tmp_dwParam1 = dwParam2;
	tmp_dwParam2 = dwParam1;

	if(uMsg != WOM_DONE)
		return;
	EnterCriticalSection(&waveCriticalSection);
	(*freeBlockCounter)++;
	LeaveCriticalSection(&waveCriticalSection);
}

static int
open_mm_output (void) {
	WAVEFORMATEX wfx;
	char *mm_buffer;
	int i;

	InitializeCriticalSection(&waveCriticalSection);

	if((mm_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((MM_BLOCK_SIZE + sizeof(WAVEHDR)) * MM_BLOCK_COUNT))) == NULL) {
		fprintf(stderr, "Memory allocation error\r\n");
		return -1;
	}

	mm_blocks = (WAVEHDR*)mm_buffer;
	mm_buffer += sizeof(WAVEHDR) * MM_BLOCK_COUNT;

	for(i = 0; i < MM_BLOCK_COUNT; i++) {
		mm_blocks[i].dwBufferLength = MM_BLOCK_SIZE;
		mm_blocks[i].lpData = mm_buffer;
		mm_buffer += MM_BLOCK_SIZE;
	}

	wfx.nSamplesPerSec = rate;
	wfx.wBitsPerSample = 16;
	wfx.nChannels = 2;
	wfx.cbSize = 0;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

	if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)mmOutProc, (DWORD_PTR)&mm_free_blocks, CALLBACK_FUNCTION ) != MMSYSERR_NOERROR) {
		fprintf(stderr, "unable to open WAVE_MAPPER device\r\n");
		HeapFree(GetProcessHeap(), 0, mm_blocks);
		hWaveOut = NULL;
		mm_blocks = NULL;
		return -1;
	}

	send_output = write_mm_output;
	close_output = close_mm_output;
	pause_output = pause_output_nop;
	return (0);
}

static int
write_mm_output (char * output_data, int output_size) {
	WAVEHDR* current;
	int free_size = 0;
	int data_read = 0;
	current = &mm_blocks[mm_current_block];

	while (output_size) {
		if(current->dwFlags & WHDR_PREPARED)
		waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		free_size = MM_BLOCK_SIZE - current->dwUser;
		if (free_size > output_size)
			free_size = output_size;

		memcpy(current->lpData + current->dwUser, &output_data[data_read], free_size);
		current->dwUser += free_size;
		output_size -= free_size;
		data_read += free_size;

		if (current->dwUser < MM_BLOCK_SIZE) {
			return (0);
		}

		current->dwBufferLength = MM_BLOCK_SIZE;
		waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
		EnterCriticalSection(&waveCriticalSection);
		mm_free_blocks--;
		LeaveCriticalSection(&waveCriticalSection);
		while(!mm_free_blocks)
			Sleep(10);
		mm_current_block++;
		mm_current_block %= MM_BLOCK_COUNT;
		current = &mm_blocks[mm_current_block];
		current->dwUser = 0;
	}
	return (0);
}

static void
close_mm_output (void) {
	WAVEHDR* current;
	int i, j;

	if (!hWaveOut) return;

	printf("Shutting down sound output\r\n");

	current = &mm_blocks[mm_current_block];
	i = MM_BLOCK_SIZE - current->dwUser;

	for (j = i; i; i--)
		write_mm_output (0, 0);

	waveOutClose (hWaveOut);
	HeapFree(GetProcessHeap(), 0, mm_blocks);
	hWaveOut = NULL;
	mm_blocks = NULL;
}

#else
#ifdef AUDIODRV_ALSA

static int alsa_first_time = 1;
static snd_pcm_t *pcm = NULL;

#define open_audio_output open_alsa_output
static int write_alsa_output(char * output_data, int output_size);
static void close_alsa_output(void);

static int open_alsa_output(void) {
	snd_pcm_hw_params_t *hw;
	snd_pcm_sw_params_t *sw;
	int err;
	unsigned int alsa_buffer_time;
	unsigned int alsa_period_time;

	if (!pcmname) {
		pcmname = strdup("default");
	}

	if ((err = snd_pcm_open(&pcm, pcmname, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		fprintf(stderr, "Error: audio open error: %s\r\n", snd_strerror(err));
		return -1;
	}

	snd_pcm_hw_params_alloca(&hw);

	if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0) {
		fprintf(stderr, "ERROR: No configuration available for playback: %s\r\n",
				snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_access(pcm, hw,
			SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "Cannot set access mode: %s.\r\n", snd_strerror(err));
		goto fail;
	}

	if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16) < 0) {
		fprintf(stderr, "ALSA does not support 16bit signed audio for your soundcard\r\n");
		goto fail;
	}

	if (snd_pcm_hw_params_set_channels(pcm, hw, 2) < 0) {
		fprintf(stderr, "ALSA does not support stereo for your soundcard\r\n");
		goto fail;
	}

	if (snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0) < 0) {
		fprintf(stderr, "ALSA does not support %iHz for your soundcard\r\n", rate);
		goto fail;
	}

	alsa_buffer_time = 500000;
	alsa_period_time = 50000;

	if ((err = snd_pcm_hw_params_set_buffer_time_near(pcm, hw,
			&alsa_buffer_time, 0)) < 0) {
		fprintf(stderr, "Set buffer time failed: %s.\r\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_period_time_near(pcm, hw,
			&alsa_period_time, 0)) < 0) {
		fprintf(stderr, "Set period time failed: %s.\r\n", snd_strerror(err));
		goto fail;
	}

	if (snd_pcm_hw_params(pcm, hw) < 0) {
		fprintf(stderr, "Unable to install hw params\r\n");
		goto fail;
	}

	snd_pcm_sw_params_alloca(&sw);
	snd_pcm_sw_params_current(pcm, sw);
	if (snd_pcm_sw_params(pcm, sw) < 0) {
		fprintf(stderr, "Unable to install sw params\r\n");
		goto fail;
	}

	send_output = write_alsa_output;
	close_output = close_alsa_output;
	pause_output = pause_output_nop;
	if (pcmname != NULL) {
		free(pcmname);
	}
	return (0);

fail:	close_alsa_output();
	return -1;
}

static int write_alsa_output(char * output_data, int output_size) {
	int err;
	snd_pcm_uframes_t frames;

	while (output_size > 0) {
		frames = snd_pcm_bytes_to_frames(pcm, output_size);
		if ((err = snd_pcm_writei(pcm, output_data, frames)) < 0) {
			if (snd_pcm_state(pcm) == SND_PCM_STATE_XRUN) {
				if ((err = snd_pcm_prepare(pcm)) < 0)
					fprintf(stderr, "\nsnd_pcm_prepare() failed.\r\n");
				alsa_first_time = 1;
				continue;
			}
			return err;
		}

		output_size -= snd_pcm_frames_to_bytes(pcm, err);
		output_data += snd_pcm_frames_to_bytes(pcm, err);
		if (alsa_first_time) {
			alsa_first_time = 0;
			snd_pcm_start(pcm);
		}
	}
	return (0);
}

static void close_alsa_output(void) {
	if (!pcm) return;
	printf("Shutting down sound output\r\n");
	snd_pcm_close(pcm);
	pcm = NULL;
}

#elif defined AUDIODRV_OSS
/*
 OSS Output Functions
 --------------------
 uses mmap'd audio
 */

#if !defined(AFMT_S16_NE)
#ifdef WORDS_BIGENDIAN
#define AFMT_S16_NE AFMT_S16_BE
#else
#define AFMT_S16_NE AFMT_S16_LE
#endif
#endif

static char *buffer = NULL;
static unsigned long int max_buffer;
static int counter;
static struct audio_buf_info info;

#define open_audio_output open_oss_output
static int write_oss_output(char * output_data, int output_size);
static void close_oss_output(void);

static void pause_output_oss(void) {
	memset(buffer, 0, max_buffer);
}

static int open_oss_output(void) {
	int caps, rc, tmp;
	unsigned long int sz = sysconf(_SC_PAGESIZE);

	if (!pcmname) {
		pcmname = strdup("/dev/dsp");
	}

	if ((audio_fd = open(pcmname, O_RDWR)) < 0) {
		fprintf(stderr, "ERROR: Unable to open dsp (%s)\r\n", strerror(errno));
		return -1;
	}
	if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) < 0) {
		fprintf(stderr, "ERROR: Unable to reset dsp\r\n");
		goto fail;
	}
	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps) < 0) {
		fprintf(stderr, "ERROR: Unable to retrieve soundcard capabilities\r\n");
		goto fail;
	}
	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP)) {
		fprintf(stderr, "ERROR: Audio driver doesn't support mmap or trigger\r\n");
		goto fail;
	}

	rc = AFMT_S16_NE;
	if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc) < 0) {
		fprintf(stderr, "ERROR: Unable to set 16bit sound format\r\n");
		goto fail;
	}

	tmp = 2;
	if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &tmp) < 0) {
		fprintf(stderr, "ERROR: Unable to set stereo\r\n");
		goto fail;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate) < 0) {
		fprintf(stderr, "ERROR: Unable to set %iHz sample rate\r\n", rate);
		goto fail;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) < 0) {
		fprintf(stderr, "ERROR: Unable to retrieve buffer status\r\n");
		goto fail;
	}

	max_buffer = (info.fragstotal * info.fragsize + sz - 1) & ~(sz - 1);
	buffer = (char *) mmap(NULL, max_buffer, PROT_WRITE|PROT_READ,
					MAP_FILE|MAP_SHARED, audio_fd, 0);

	if (buffer == MAP_FAILED) {
		buffer = NULL;
		fprintf(stderr, "ERROR: couldn't mmap dsp (%s)\r\n", strerror(errno));
		goto fail;
	}

	tmp = 0;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) < 0) {
		fprintf(stderr, "ERROR: Could not toggle dsp\r\n");
		goto fail;
	}

	tmp = PCM_ENABLE_OUTPUT;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) < 0) {
		fprintf(stderr, "ERROR: Could not toggle dsp\r\n");
		goto fail;
	}
	send_output = write_oss_output;
	close_output = close_oss_output;
	pause_output = pause_output_oss;
	return (0);

fail:	close_oss_output();
	return -1;
}

static int write_oss_output(char * output_data, int output_size) {
	struct count_info count;
	int data_read = 0;
	int free_size = 0;
	while (output_size != 0) {
		while (1) {
			if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count) < 0) {
				fprintf(stderr, "\nERROR: Sound dead\r\n");
				return -1;
			}
			if ((count.ptr < counter) || (count.ptr >= (counter + 4))) {
				break;
			}
			msleep(5);
		}
		if (count.ptr < counter) {
			free_size = max_buffer - counter;
		} else {
			free_size = count.ptr - counter;
		}
		if (free_size > output_size)
		free_size = output_size;

		memcpy(&buffer[counter], &output_data[data_read], free_size);
		data_read += free_size;
		counter += free_size;
		if (counter >= (long)max_buffer)
			counter = 0;
		output_size -= free_size;
	}
	return (0);
}

static void close_oss_output(void) {
	if (!buffer && audio_fd < 0)
		return;
	printf("Shutting down sound output\r\n");
	/* unmap before closing audio_fd */
	if (buffer != NULL)
		munmap(buffer, max_buffer);
	buffer = NULL;
	if (audio_fd >= 0)
		close(audio_fd);
	audio_fd = -1;
}

#elif defined AUDIODRV_OPENAL

#define NUM_BUFFERS 16
#define PRIME 8

struct position {
	ALfloat x;
	ALfloat y;
	ALfloat z;
};

static ALCdevice *device;
static ALCcontext *context;
static ALuint sourceId = 0;
static ALuint buffers[NUM_BUFFERS];
static ALuint frames = 0;

#define open_audio_output open_openal_output

static void pause_output_openal(void) {
	alSourcePause(sourceId);
}

static int write_openal_output(char * output_data, int output_size) {
	ALint processed, state;

	if (frames <= PRIME) { /* prime the pump */
		alBufferData(buffers[frames], AL_FORMAT_STEREO16, output_data,
				output_size, rate);

		/* Now queue and start playback! */
		if (frames == PRIME) {
			alSourceQueueBuffers(sourceId, frames, buffers);
			alSourcePlay(sourceId);
		}
		frames++;
		return 0;
	}

	/* Get relevant source info */
	alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
	alGetSourcei(sourceId, AL_BUFFERS_PROCESSED, &processed);

	/* Unqueue and handle each processed buffer */
	while (processed > 0) {
		ALuint bufid;

		alSourceUnqueueBuffers(sourceId, 1, &bufid);
		processed--;

		/* Read the next chunk of data, refill the buffer, and queue it
		 * back on the source */
		if (output_data != NULL) {
			alBufferData(bufid, AL_FORMAT_STEREO16, output_data, output_size, rate);
			alSourceQueueBuffers(sourceId, 1, &bufid);
		}
		if (alGetError() != AL_NO_ERROR) {
			fprintf(stderr, "\nError buffering data\r\n");
			return (-1);
		}
	}

	/* Make sure the source hasn't underrun */
	if (state != AL_PLAYING) {
		ALint queued;

		/* If no buffers are queued, playback is finished */
		alGetSourcei(sourceId, AL_BUFFERS_QUEUED, &queued);
		if(queued == 0)
			return (-1);

		/*printf("STATE: %#08x - %d\n", state, queued);*/

		alSourcePlay(sourceId);
		if (alGetError() != AL_NO_ERROR) {
			fprintf(stderr, "\nError restarting playback\r\n");
			return (-1);
		}
	}

	/* block while playing back samples */
	while (state == AL_PLAYING && processed == 0) {
		msleep(1);
		alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
		alGetSourcei(sourceId, AL_BUFFERS_PROCESSED, &processed);
	}

	return (0);
}

static void close_openal_output(void) {
	if (!context) return;
	printf("Shutting down sound output\r\n");
	alSourceStop(sourceId);			/* stop playing */
	alSourcei(sourceId, AL_BUFFER, 0);	/* unload buffer from source */
	alDeleteBuffers(NUM_BUFFERS, buffers);
	alDeleteSources(1, &sourceId);
	alcDestroyContext(context);
	alcCloseDevice(device);
	context = NULL;
	device = NULL;
}

static int open_openal_output(void) {
	/* setup our audio devices and contexts */
	device = alcOpenDevice(NULL);
	if (!device) {
		fprintf(stderr, "OpenAL: Unable to open default device.\r\n");
		return (-1);
	}

	context = alcCreateContext(device, NULL);
	if (context == NULL || alcMakeContextCurrent(context) == ALC_FALSE) {
		if (context != NULL)
			alcDestroyContext(context);
		alcCloseDevice(device);
		context = NULL;
		device = NULL;
		fprintf(stderr, "OpenAL: Failed to create the default context.\r\n");
		return (-1);
	}

	/* setup our sources and buffers */
	alGenSources(1, &sourceId);
	alGenBuffers(NUM_BUFFERS, buffers);

	send_output = write_openal_output;
	close_output = close_openal_output;
	pause_output = pause_output_openal;
	return (0);
}

#else /* no audio output driver compiled in: */

#define open_audio_output open_noaudio_output
static int open_noaudio_output(void) {
	return -1;
}

#endif /* AUDIODRV_ALSA */
#endif /* _WIN32 || __CYGWIN__ */

static struct option const long_options[] = {
	{ "version", 0, 0, 'v' },
	{ "help", 0, 0, 'h' },
	{ "rate", 1, 0, 'r' },
	{ "master_volume", 1, 0, 'm' },
	{ "config_file", 1, 0, 'c' },
	{ "wavout", 1, 0, 'o' },
	{ "log_vol", 0, 0, 'l' },
	{ "reverb", 0, 0, 'b' },
	{ "test_midi", 0, 0, 't' },
	{ "test_bank", 1, 0, 'k' },
	{ "test_patch", 1, 0, 'p' },
	{ "enhanced_resample", 0, 0, 'e' },
	{ "auddev", 1, 0, 'd' },
	{ "wholetempo", 0, 0, 'w' },
	{ "roundtempo", 0, 0, 'n' },
	{ NULL, 0, NULL, 0 }
};

static void do_help(void) {
	printf("  -v    --version        Display version\n");
	printf("  -h    --help           This help.\n");
#ifndef _WIN32
	printf("  -d D  --device=D       Use device D for audio output instead\n");
	printf("                         of the default\n");
#endif
	printf("MIDI Options\n");
	printf("  -w    --wholetempo       round down tempo to whole number\n");
	printf("  -n    --roundtempo       round tempo to nearest whole number\n");
	printf("Software Wavetable Options\n");
	printf("  -o W  --wavout=W       Saves the output to W in wav format\n");
	printf("                         at 44100Hz 16 bit stereo\n");
	printf("  -l    --log_vol        Use log volume adjustments\n");
	printf("  -r N  --rate=N         output at N samples per second\n");
	printf("  -c P  --config_file=P  P is the path and filename to your wildmidi.cfg\n");
	printf("                         Defaults to %s\n", WILDMIDI_CFG);
	printf(" -m V  --master_volume=V Sets the master volumes, default is 100\n");
	printf("                         range is 0-127 with 127 being the loudest\n");
	printf(" -b    --reverb          Enable final output reverb engine\n");
}

static void do_version(void) {
	printf("\nWildMidi %s Open Source Midi Sequencer\n", PACKAGE_VERSION);
	printf("Copyright (C) WildMIDI Developers 2001-2014\n\n");
	printf("WildMidi comes with ABSOLUTELY NO WARRANTY\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under the terms and conditions of the GNU General Public License version 3.\n");
	printf("For more information see COPYING\n\n");
	printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
	printf("WildMIDI homepage at %s\n\n", PACKAGE_URL);
}

static void do_syntax(void) {
	printf("wildmidi [options] filename.mid\n\n");
}

int main(int argc, char **argv) {
	struct _WM_Info *wm_info;
	int i, res;
	int option_index = 0;
	unsigned long int mixer_options = 0;
	char *config_file = NULL;
	void *midi_ptr;
	unsigned char master_volume = 100;
	char *output_buffer;
	unsigned long int perc_play = 0;
	unsigned long int pro_mins = 0;
	unsigned long int pro_secs = 0;
	unsigned long int apr_mins = 0;
	unsigned long int apr_secs = 0;
	unsigned char modes[4];
	unsigned long int count_diff;
	unsigned char ch;
	unsigned char test_midi = 0;
	unsigned char test_count = 0;
	unsigned char *test_data;
	unsigned char test_bank = 0;
	unsigned char test_patch = 0;
	static char spinner[] = "|/-\\";
	static int spinpoint = 0;
	unsigned long int seek_to_sample = 0;
	int inpause = 0;
	long libraryver;

#if !defined(_WIN32) && !defined(__DJGPP__)
	int my_tty;
	struct termios _tty;
	tcflag_t _res_oflg = 0;
	tcflag_t _res_lflg = 0;

#define raw() (_tty.c_lflag &= ~(ICANON | ICRNL | ISIG), \
		_tty.c_oflag &= ~ONLCR, tcsetattr(my_tty, TCSANOW, &_tty))
#define savetty() ((void) tcgetattr(my_tty, &_tty), \
		_res_oflg = _tty.c_oflag, _res_lflg = _tty.c_lflag)
#define resetty() (_tty.c_oflag = _res_oflg, _tty.c_lflag = _res_lflg,\
		(void) tcsetattr(my_tty, TCSADRAIN, &_tty))
#endif /* !_WIN32, !__DJGPP__ */

	do_version();
	while (1) {
		i = getopt_long(argc, argv, "vho:lr:c:m:btk:p:ed:wn", long_options,
				&option_index);
		if (i == -1)
			break;
		switch (i) {
		case 'v': /* Version */
			return (0);
		case 'h': /* help */
			do_syntax();
			do_help();
			return (0);
		case 'r': /* Sample Rate */
			res = atoi(optarg);
			if (res < 0 || res > 65535) {
				fprintf(stderr, "Error: bad rate %i.\n", res);
				return (0);
			}
			rate = res;
			break;
		case 'b': /* Reverb */
			mixer_options ^= WM_MO_REVERB;
			break;
		case 'm': /* Master Volume */
			master_volume = (unsigned char) atoi(optarg);
			break;
		case 'o': /* Wav Output */
			if (!*optarg) {
				fprintf(stderr, "Error: empty wavfile name.\n");
				return (0);
			}
			strcpy(wav_file, optarg);
			break;
		case 'c': /* Config File */
			config_file = strdup(optarg);
			break;
		case 'd': /* Output device */
			if (!*optarg) {
				fprintf(stderr, "Error: empty device name.\n");
				return (0);
			}
			pcmname = strdup(optarg);
			break;
		case 'e': /* Enhanced Resampling */
			mixer_options |= WM_MO_ENHANCED_RESAMPLING;
			break;
		case 'l': /* log volume */
			mixer_options |= WM_MO_LOG_VOLUME;
			break;
		case 't': /* play test midis */
			test_midi = 1;
			break;
		case 'k': /* set test bank */
			test_bank = (unsigned char) atoi(optarg);
			break;
		case 'p': /* set test patch */
			test_patch = (unsigned char) atoi(optarg);
			break;
		case 'w': /* whole number tempo */
			mixer_options |= WM_MO_WHOLETEMPO;
			break;
		case 'n': /* whole number tempo */
			mixer_options |= WM_MO_ROUNDTEMPO;
			break;
		default:
			fprintf(stderr, "Error: Unknown option -%o\n", i);
			return (0);
		}
	}

	if (!config_file) {
		config_file = strdup(WILDMIDI_CFG);
	}
	if (optind < argc || test_midi) {
		printf("Initializing Sound System\n");

		if (wav_file[0] != '\0') {
			if (open_wav_output() == -1) {
				return (0);
			}
		} else {
			if (open_audio_output() == -1) {
				return (0);
			}
		}

		libraryver = WildMidi_GetVersion();
		printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
							(libraryver>>16) & 255,
							(libraryver>> 8) & 255,
							(libraryver    ) & 255);
		if (WildMidi_Init(config_file, rate, mixer_options) == -1) {
			return (0);
		}

		printf(" +  Volume up        e  Better resampling    n  Next Midi\n");
		printf(" -  Volume down      l  Log volume           q  Quit\n");
		printf(" ,  1sec Seek Back   r  Reverb               .  1sec Seek Forward\n");
		printf("                     p  Pause On/Off\n\n");

		output_buffer = malloc(16384);
		if (output_buffer == NULL) {
			fprintf(stderr, "Not enough memory, exiting\n");
			WildMidi_Shutdown();
			return (0);
		}

#if !defined(_WIN32) && !defined(__DJGPP__)
		my_tty = fileno(stdin);
		if (isatty(my_tty)) {
			savetty();
			raw();
			fcntl(0, F_SETFL, FNONBLOCK);
		}
#endif

		WildMidi_MasterVolume(master_volume);

		while ((optind < argc) || (test_midi)) {
			if (!test_midi) {
				const char *real_file = FIND_LAST_DIRSEP(argv[optind]);

				printf("Playing ");
				if (real_file != NULL) {
					printf("%s \r\n", (real_file + 1));
				} else {
					printf("%s \r\n", argv[optind]);
				}

				midi_ptr = WildMidi_Open(argv[optind]);
				if (midi_ptr == NULL) {
					printf("\r");
					optind++;
					continue;
				}
				wm_info = WildMidi_GetInfo(midi_ptr);

				optind++;
			} else {
				if (test_count == midi_test_max) {
					break;
				}
				test_data = malloc(midi_test[test_count].size);
				memcpy(test_data, midi_test[test_count].data,
						midi_test[test_count].size);
				test_data[25] = test_bank;
				test_data[28] = test_patch;
				midi_ptr = WildMidi_OpenBuffer(test_data, 633);
				wm_info = WildMidi_GetInfo(midi_ptr);
				test_count++;
				printf("Playing test midi no. %i\r\n", test_count);
			}

			apr_mins = wm_info->approx_total_samples / (rate * 60);
			apr_secs = (wm_info->approx_total_samples % (rate * 60)) / rate;

			if (midi_ptr == NULL) {
				fprintf(stderr, "Skipping %s\r\n", argv[optind]);
				optind++;
				continue;
			}
			mixer_options = wm_info->mixer_options;
			fprintf(stderr, "\r");

			while (1) {
				count_diff = wm_info->approx_total_samples
						- wm_info->current_sample;

				if (count_diff == 0)
					break;

				ch = 0;
#ifdef _WIN32
				if (_kbhit()) {
					ch = _getch();
					putch(ch);
				}
#elif defined(__DJGPP__)
				if (kbhit()) {
					ch = getch();
					putch(ch);
				}
#else
				if (read(my_tty, &ch, 1) != 1)
					ch = 0;
#endif
				if (ch) {
					switch (ch) {
					case 'l':
						WildMidi_SetOption(midi_ptr, WM_MO_LOG_VOLUME,
								((mixer_options & WM_MO_LOG_VOLUME)
										^ WM_MO_LOG_VOLUME));
						mixer_options ^= WM_MO_LOG_VOLUME;
						break;
					case 'r':
						WildMidi_SetOption(midi_ptr, WM_MO_REVERB,
								((mixer_options & WM_MO_REVERB) ^ WM_MO_REVERB));
						mixer_options ^= WM_MO_REVERB;
						break;
					case 'e':
						WildMidi_SetOption(midi_ptr, WM_MO_ENHANCED_RESAMPLING,
								((mixer_options & WM_MO_ENHANCED_RESAMPLING)
										^ WM_MO_ENHANCED_RESAMPLING));
						mixer_options ^= WM_MO_ENHANCED_RESAMPLING;
						break;
					case 'n':
						goto NEXTMIDI;
					case 'p':
						if (inpause) {
							inpause = 0;
							fprintf(stderr, "       \r");
						} else {
							inpause = 1;
							fprintf(stderr, "Paused \r");
							continue;
						}
						break;
					case 'q':
						printf("\r\n");
						goto end1;
					case '-':
						if (master_volume > 0) {
							master_volume--;
							WildMidi_MasterVolume(master_volume);
						}
						break;
					case '+':
						if (master_volume < 127) {
							master_volume++;
							WildMidi_MasterVolume(master_volume);
						}
						break;
					case ',': /* fast seek backwards */
						if (wm_info->current_sample < rate) {
							seek_to_sample = 0;
						} else {
							seek_to_sample = wm_info->current_sample - rate;
						}
						WildMidi_FastSeek(midi_ptr, &seek_to_sample);
						break;
					case '.': /* fast seek forwards */
						if ((wm_info->approx_total_samples
								- wm_info->current_sample) < rate) {
							seek_to_sample = wm_info->approx_total_samples;
						} else {
							seek_to_sample = wm_info->current_sample + rate;
						}
						WildMidi_FastSeek(midi_ptr, &seek_to_sample);
						break;
					default:
						break;
					}
				}

				if (inpause) {
					wm_info = WildMidi_GetInfo(midi_ptr);
					perc_play = (wm_info->current_sample * 100)
							/ wm_info->approx_total_samples;
					pro_mins = wm_info->current_sample / (rate * 60);
					pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
					modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
					modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
					modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
					modes[3] = '\0';
					fprintf(stderr,
						"        [Approx %2lum %2lus Total] [%s] [%3i] [%2lum %2lus Processed] [%2lu%%] 0  \r",
						apr_mins, apr_secs, modes, master_volume, pro_mins,
						pro_secs, perc_play);

					msleep(5);
					pause_output();
					continue;
				}

				res = WildMidi_GetOutput(midi_ptr, output_buffer,
							 (count_diff >= 4096)? 16384 : (count_diff * 4));
				if (res <= 0)
					break;

				wm_info = WildMidi_GetInfo(midi_ptr);
				perc_play = (wm_info->current_sample * 100)
						/ wm_info->approx_total_samples;
				pro_mins = wm_info->current_sample / (rate * 60);
				pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
				modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
				modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
				modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
				modes[3] = '\0';
				fprintf(stderr,
						"        [Approx %2lum %2lus Total] [%s] [%3i] [%2lum %2lus Processed] [%2lu%%] %c  \r",
						apr_mins, apr_secs, modes, master_volume, pro_mins,
						pro_secs, perc_play, spinner[spinpoint++ % 4]);

				if (send_output(output_buffer, res) < 0) {
				/* driver prints an error message already. */
					printf("\r");
					goto end2;
				}
			}
			NEXTMIDI: fprintf(stderr, "\r\n");
			if (WildMidi_Close(midi_ptr) == -1) {
				fprintf(stderr, "OOPS: failed closing midi handle!\r\n");
			}
			memset(output_buffer, 0, 16384);
			send_output(output_buffer, 16384);
		}
end1:		memset(output_buffer, 0, 16384);
		send_output(output_buffer, 16384);
		msleep(5);
end2:		close_output();
		free(output_buffer);
		if (WildMidi_Shutdown() == -1)
			fprintf(stderr, "OOPS: failure shutting down libWildMidi\r\n");
#if !defined(_WIN32) && !defined(__DJGPP__)
		if (isatty(my_tty))
			resetty();
#endif
	} else {
		fprintf(stderr, "ERROR: No midi file given\r\n");
		do_syntax();
		return (0);
	}

	printf("\r\n");
	return (0);
}

#if !defined(_WIN32) && !defined(__DJGPP__)
static int msleep(unsigned long milisec) {
	struct timespec req = { 0, 0 };
	time_t sec = (int) (milisec / 1000);
	milisec = milisec - (sec * 1000);
	req.tv_sec = sec;
	req.tv_nsec = milisec * 1000000L;
	while (nanosleep(&req, &req) == -1)
		continue;
	return (1);
}
#endif
