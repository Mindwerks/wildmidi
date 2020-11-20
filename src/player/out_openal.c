/*
 * out_openal.c -- OpenAL output
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

#include "out_openal.h"
#include "wildplay.h"

#if (AUDIODRV_OPENAL == 1)

#define NUM_BUFFERS 4

extern unsigned int rate;

static ALCdevice *device;
static ALCcontext *context;
static ALuint sourceId = 0;
static ALuint buffers[NUM_BUFFERS];
static ALuint frames = 0;

void pause_output_openal(void) {
    alSourcePause(sourceId);
}

int write_openal_output(int8_t *output_data, int output_size) {
    ALint processed, state;
    ALuint bufid;

    if (frames < NUM_BUFFERS) { /* initial state: fill the buffers */
        alBufferData(buffers[frames], AL_FORMAT_STEREO16, output_data,
                     output_size, rate);

        /* Now queue and start playback! */
        if (++frames == NUM_BUFFERS) {
            alSourceQueueBuffers(sourceId, frames, buffers);
            alSourcePlay(sourceId);
        }
        return 0;
    }

    /* Get relevant source info */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED) { /* resume it, then.. */
        alSourcePlay(sourceId);
        if (alGetError() != AL_NO_ERROR) {
            fprintf(stderr, "\nError restarting playback\r\n");
            return (-1);
        }
    }

    processed = 0;
    while (processed == 0) { /* Wait until we have a processed buffer */
        alGetSourcei(sourceId, AL_BUFFERS_PROCESSED, &processed);
    }

    /* Unqueue and handle each processed buffer */
    alSourceUnqueueBuffers(sourceId, 1, &bufid);

    /* Read the next chunk of data, refill the buffer, and queue it
     * back on the source */
    alBufferData(bufid, AL_FORMAT_STEREO16, output_data, output_size, rate);
    alSourceQueueBuffers(sourceId, 1, &bufid);
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr, "\nError buffering data\r\n");
        return (-1);
    }

    /* Make sure the source hasn't underrun */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    /*printf("STATE: %#08x - %d\n", state, queued);*/
    if (state != AL_PLAYING) {
        ALint queued;

        /* If no buffers are queued, playback is finished */
        alGetSourcei(sourceId, AL_BUFFERS_QUEUED, &queued);
        if (queued == 0) {
            fprintf(stderr, "\nNo buffers queued for playback\r\n");
            return (-1);
        }

        alSourcePlay(sourceId);
    }
    return (0);
}

void close_openal_output(void) {
    if (!context)
        return;
    printf("Shutting down sound output\r\n");
    alSourceStop(sourceId);         /* stop playing */
    alSourcei(sourceId, AL_BUFFER, 0);  /* unload buffer from source */
    alDeleteBuffers(NUM_BUFFERS, buffers);
    alDeleteSources(1, &sourceId);
    alcDestroyContext(context);
    alcCloseDevice(device);
    context = NULL;
    device = NULL;
    frames = 0;
}

int open_openal_output(const char * output) {
    UNUSED(output);

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

    return (0);
}

#endif // AUDIODRV_OPENAL == 1
