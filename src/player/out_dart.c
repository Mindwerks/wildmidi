/*
 * out_dart.c -- OS/2 DART output
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

#include "out_dart.h"

#if (AUDIODRV_OS2DART == 1)

/* based on Dart code originally written by Kevin Langman for XMP */

#define BUFFERCOUNT 4

static MCI_MIX_BUFFER MixBuffers[BUFFERCOUNT];
static MCI_MIXSETUP_PARMS MixSetupParms;
static MCI_BUFFER_PARMS BufferParms;
static MCI_GENERIC_PARMS GenericParms;

static ULONG DeviceID = 0;
static ULONG bsize = 16;
static short next = 2;
static short ready = 1;

static HMTX dart_mutex;

/* Buffer update thread (created and called by DART) */
static LONG APIENTRY OS2_Dart_UpdateBuffers
    (ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags) {

    (void) pBuffer;/* unused param */

    if ((ulFlags == MIX_WRITE_COMPLETE) ||
        ((ulFlags == (MIX_WRITE_COMPLETE | MIX_STREAM_ERROR)) &&
         (ulStatus == ERROR_DEVICE_UNDERRUN))) {
        DosRequestMutexSem(dart_mutex, SEM_INDEFINITE_WAIT);
        ready++;
        DosReleaseMutexSem(dart_mutex);
    }
    return (TRUE);
}

int open_dart_output(void) {
    int i;
    MCI_AMP_OPEN_PARMS AmpOpenParms;

    if (DosCreateMutexSem(NULL, &dart_mutex, 0, 0) != NO_ERROR) {
        fprintf(stderr, "Failed creating a MutexSem.\r\n");
        return (-1);
    }

    /* compute a size for circa 1/4" of playback. */
    bsize = rate >> 2;
    bsize <<= 1; /* stereo */
    bsize <<= 1; /* 16 bit */
    for (i = 15; i >= 12; i--) {
        if (bsize & (1 << i))
            break;
    }
    bsize = (1 << i);
    /* make sure buffer is not greater than 64 Kb: DART can't handle it. */
    if (bsize > 65536)
        bsize = 65536;

    MixBuffers[0].pBuffer = NULL; /* marker */
    memset(&GenericParms, 0, sizeof(MCI_GENERIC_PARMS));

    /* open AMP device */
    memset(&AmpOpenParms, 0, sizeof(MCI_AMP_OPEN_PARMS));
    AmpOpenParms.usDeviceID = 0;

    AmpOpenParms.pszDeviceType =
        (PSZ) MAKEULONG(MCI_DEVTYPE_AUDIO_AMPMIX, 0); /* 0: default waveaudio device */

    if(mciSendCommand(0, MCI_OPEN, MCI_WAIT|MCI_OPEN_TYPE_ID|MCI_OPEN_SHAREABLE,
                       (PVOID) &AmpOpenParms, 0) != MCIERR_SUCCESS) {
        fprintf(stderr, "Failed opening DART audio device\r\n");
        return (-1);
    }

    DeviceID = AmpOpenParms.usDeviceID;

    /* setup playback parameters */
    memset(&MixSetupParms, 0, sizeof(MCI_MIXSETUP_PARMS));

    MixSetupParms.ulBitsPerSample = 16;
    MixSetupParms.ulFormatTag = MCI_WAVE_FORMAT_PCM;
    MixSetupParms.ulSamplesPerSec = rate;
    MixSetupParms.ulChannels = 2;
    MixSetupParms.ulFormatMode = MCI_PLAY;
    MixSetupParms.ulDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    MixSetupParms.pmixEvent = OS2_Dart_UpdateBuffers;

    if (mciSendCommand(DeviceID, MCI_MIXSETUP,
                       MCI_WAIT | MCI_MIXSETUP_INIT,
                       (PVOID) & MixSetupParms, 0) != MCIERR_SUCCESS) {

        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) & GenericParms, 0);
        fprintf(stderr, "Failed DART mixer setup\r\n");
        return (-1);
    }

    /*bsize = MixSetupParms.ulBufferSize;*/
    /*printf("Dart Buffer Size = %lu\n", bsize);*/

    BufferParms.ulNumBuffers = BUFFERCOUNT;
    BufferParms.ulBufferSize = bsize;
    BufferParms.pBufList = MixBuffers;

    if (mciSendCommand(DeviceID, MCI_BUFFER,
                       MCI_WAIT | MCI_ALLOCATE_MEMORY,
                       (PVOID) & BufferParms, 0) != MCIERR_SUCCESS) {
        fprintf(stderr, "DART Memory allocation error\r\n");
        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) & GenericParms, 0);
        return (-1);
    }

    for (i = 0; i < BUFFERCOUNT; i++) {
        MixBuffers[i].ulBufferLength = bsize;
    }

    /* Start Playback */
    memset(MixBuffers[0].pBuffer, /*32767 */ 0, bsize);
    memset(MixBuffers[1].pBuffer, /*32767 */ 0, bsize);
    MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle, MixBuffers, 2);

    send_output = write_dart_output;
    close_output = close_dart_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;

    return (0);
}

int write_dart_output(int8_t *output_data, int output_size) {
    static int idx = 0;

    if (idx + output_size > bsize) {
        do {
            DosRequestMutexSem(dart_mutex, SEM_INDEFINITE_WAIT);
            if (ready != 0) {
                DosReleaseMutexSem(dart_mutex);
                break;
            }
            DosReleaseMutexSem(dart_mutex);
            DosSleep(20);
        } while (TRUE);

        MixBuffers[next].ulBufferLength = idx;
        MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle, &(MixBuffers[next]), 1);
        ready--;
        next++;
        idx = 0;
        if (next == BUFFERCOUNT) {
            next = 0;
        }
    }
    memcpy(&((char *)MixBuffers[next].pBuffer)[idx], output_data, output_size);
    idx += output_size;
    return (0);
}

void close_dart_output(void) {
    printf("Shutting down sound output\r\n");
    if (MixBuffers[0].pBuffer) {
        mciSendCommand(DeviceID, MCI_BUFFER,
                       MCI_WAIT | MCI_DEALLOCATE_MEMORY, &BufferParms, 0);
        MixBuffers[0].pBuffer = NULL;
    }
    if (DeviceID) {
        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) &GenericParms, 0);
        DeviceID = 0;
    }
}

#endif // AUDIODRV_OS2DART == 1
