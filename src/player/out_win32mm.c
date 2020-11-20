/*
 * out_win32mm.c -- Windows MM output
 *
 * Copyright (C) WildMidi Developers 2020
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include "out_win32mm.h"
#include "wildplay.h"

#if AUDIODRV_WIN32_MM == 1

#include "wildplay.h"

extern unsigned int rate;

static HWAVEOUT hWaveOut = NULL;
static CRITICAL_SECTION waveCriticalSection;

static WAVEHDR *mm_blocks = NULL;
#define MM_BLOCK_SIZE 16384
#define MM_BLOCK_COUNT 3

static DWORD mm_free_blocks = MM_BLOCK_COUNT;
static DWORD mm_current_block = 0;

#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef DWORD DWORD_PTR;
#endif

static void CALLBACK mmOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    /* unused params */
    UNUSED(hWaveOut);
    UNUSED(dwParam1);
    UNUSED(dwParam2);

    if(uMsg != WOM_DONE)
        return;
    /* increment mm_free_blocks */
    EnterCriticalSection(&waveCriticalSection);
    (*(DWORD *)dwInstance)++;
    LeaveCriticalSection(&waveCriticalSection);
}

int open_mm_output(const char * output) {
    WAVEFORMATEX wfx;
    char *mm_buffer;
    int i;

    UNUSED(output);

    InitializeCriticalSection(&waveCriticalSection);

    if((mm_buffer = (char *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((MM_BLOCK_SIZE + sizeof(WAVEHDR)) * MM_BLOCK_COUNT))) == NULL) {
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

    if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)mmOutProc, (DWORD_PTR)&mm_free_blocks, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        fprintf(stderr, "unable to open WAVE_MAPPER device\r\n");
        HeapFree(GetProcessHeap(), 0, mm_blocks);
        hWaveOut = NULL;
        mm_blocks = NULL;
        return -1;
    }

    return (0);
}

int write_mm_output(int8_t *output_data, int output_size) {
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

void close_mm_output(void) {
    int i;

    if (!hWaveOut) return;

    printf("Shutting down sound output\r\n");
    for (i = 0; i < MM_BLOCK_COUNT; i++) {
        while (waveOutUnprepareHeader(hWaveOut, &mm_blocks[i], sizeof(WAVEHDR))
                == WAVERR_STILLPLAYING) {
            Sleep(10);
        }
    }

    waveOutClose (hWaveOut);
    HeapFree(GetProcessHeap(), 0, mm_blocks);
    hWaveOut = NULL;
    mm_blocks = NULL;
}

#endif
