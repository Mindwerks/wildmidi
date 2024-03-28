/*
 * out_coreaudio.c -- CoreAudio output for Mac OS X, based on XMP:
 * Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Copyright (C) WildMidi Developers 2024
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

#include "out_coreaudio.h"
#include "wildplay.h"

#if (AUDIODRV_COREAUDIO == 1)

extern unsigned int rate;
extern int msleep(unsigned long msec);

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreServices/CoreServices.h>

static AudioUnit au;

#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1060) || \
    (!defined(AUDIO_UNIT_VERSION) || ((AUDIO_UNIT_VERSION - 0) < 1060))
#define AudioComponent Component
#define AudioComponentDescription ComponentDescription
#define AudioComponentFindNext FindNextComponent
#define AudioComponentInstanceNew OpenAComponent
#define AudioComponentInstanceDispose CloseComponent
#endif

/*
 * CoreAudio helpers by Timothy J. Wood from mplayer/libao
 * The player fills a ring buffer, OSX retrieves data from the buffer
 */

static int paused;
static unsigned char *buffer;
static int buffer_len;
static int buf_write_pos;
static int buf_read_pos;
static int num_chunks;
static int chunk_size;
static int packet_size;


/* return minimum number of free bytes in buffer, value may change between
 * two immediately following calls, and the real number of free bytes
 * might actually be larger!  */
static int buf_free(void)
{
    int free = buf_read_pos - buf_write_pos - chunk_size;
    if (free < 0) {
        free += buffer_len;
    }
    return free;
}

/* return minimum number of buffered bytes, value may change between
 * two immediately following calls, and the real number of buffered bytes
 * might actually be larger! */
static int buf_used(void)
{
    int used = buf_write_pos - buf_read_pos;
    if (used < 0) {
        used += buffer_len;
    }
    return used;
}

/* add data to ringbuffer */
static int write_buffer(unsigned char *data, int len)
{
    int first_len = buffer_len - buf_write_pos;
    int free = buf_free();

    if (len > free) {
        len = free;
    }
    if (first_len > len) {
        first_len = len;
    }

    /* till end of buffer */
    memcpy(buffer + buf_write_pos, data, first_len);
    if (len > first_len) {
    /* wrap around remaining part from beginning of buffer */
        memcpy(buffer, data + first_len, len - first_len);
    }
    buf_write_pos = (buf_write_pos + len) % buffer_len;

    return len;
}

/* remove data from ringbuffer */
static int read_buffer(unsigned char *data, int len)
{
    int first_len = buffer_len - buf_read_pos;
    int buffered = buf_used();

    if (len > buffered) {
        len = buffered;
    }
    if (first_len > len) {
        first_len = len;
    }

    /* till end of buffer */
    memcpy(data, buffer + buf_read_pos, first_len);
    if (len > first_len) {
    /* wrap around remaining part from beginning of buffer */
        memcpy(data + first_len, buffer, len - first_len);
    }
    buf_read_pos = (buf_read_pos + len) % buffer_len;

    return len;
}

static OSStatus render_proc(void *inRefCon,
                            AudioUnitRenderActionFlags *inActionFlags,
                            const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                            UInt32 inNumFrames, AudioBufferList *ioData)
{
    int amt = buf_used();
    int req = inNumFrames * packet_size;

    if (amt > req) {
        amt = req;
    }

    read_buffer((unsigned char *)ioData->mBuffers[0].mData, amt);
    ioData->mBuffers[0].mDataByteSize = amt;

    WMPLAY_UNUSED(inRefCon);
    WMPLAY_UNUSED(inActionFlags);
    WMPLAY_UNUSED(inTimeStamp);
    WMPLAY_UNUSED(inBusNumber);

    return noErr;
}

/*
 * end of CoreAudio helpers
 */


int open_coreaudio_output(const char * output)
{
    AudioStreamBasicDescription ad;
    AudioComponent comp;
    AudioComponentDescription cd;
    AURenderCallbackStruct rc;
    OSStatus status;
    UInt32 size, max_frames;
    int latency = 250;

    WMPLAY_UNUSED(output);

    if (latency < 20) {
        latency = 20;
    }

    /* Setup for signed 16 bit output: */
    ad.mSampleRate = rate;
    ad.mFormatID = kAudioFormatLinearPCM;
    ad.mFormatFlags = kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;
    ad.mChannelsPerFrame = 2;
    ad.mBitsPerChannel = 16;
    ad.mBytesPerFrame = 2 * ad.mChannelsPerFrame;
    ad.mBytesPerPacket = ad.mBytesPerFrame;
    ad.mFramesPerPacket = 1;

    packet_size = ad.mFramesPerPacket * ad.mChannelsPerFrame * (ad.mBitsPerChannel / 8);

    cd.componentType = kAudioUnitType_Output;
    cd.componentSubType = kAudioUnitSubType_DefaultOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    if ((comp = AudioComponentFindNext(NULL, &cd)) == NULL) {
        goto err;
    }

    if ((status = AudioComponentInstanceNew(comp, &au))) {
        goto err1;
    }

    if ((status = AudioUnitInitialize(au))) {
        goto err1;
    }

    status = AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0, &ad, sizeof(ad));
    if (status) {
        goto err1;
    }

    size = sizeof(UInt32);
    status = AudioUnitGetProperty(au, kAudioDevicePropertyBufferSize,
                                  kAudioUnitScope_Input, 0, &max_frames, &size);
    if (status) {
        goto err1;
    }

    chunk_size = max_frames;
    num_chunks = (rate * ad.mBytesPerFrame * latency / 1000 + chunk_size - 1) / chunk_size;
    buffer_len = (num_chunks + 1) * chunk_size;
    if ((buffer = calloc(num_chunks + 1, chunk_size)) == NULL) {
        goto err;
    }

    rc.inputProc = render_proc;
    rc.inputProcRefCon = 0;

    buf_read_pos = 0;
    buf_write_pos = 0;
    paused = 1;

    status = AudioUnitSetProperty(au,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0, &rc, sizeof(rc));
    if (status) {
        goto err2;
    }

    return 0;

  err2:
    free(buffer);
  err1:
    fprintf(stderr, "initialization error: %d\n", (int)status);
  err:
    return -1;
}


int write_coreaudio_output(int8_t *buf, int len)
{
    int j = 0;

    /* block until we have enough free space in the buffer */
    while (buf_free() < len) {
        msleep(100);
    }

    while (len) {
        j = write_buffer((unsigned char *)buf, len);
        if (j > 0) {
            len -= j;
            buf += j;
        } else
            break;
    }

    if (paused) {
        AudioOutputUnitStart(au);
        paused = 0;
    }

    return 0;
}


void close_coreaudio_output(void)
{
    AudioOutputUnitStop(au);
    AudioUnitUninitialize(au);
    AudioComponentInstanceDispose(au);
    free(buffer);
}

void pause_coreaudio_output(void)
{
    AudioOutputUnitStop(au);
}

void resume_coreaudio_output(void)
{
    AudioOutputUnitStart(au);
}

#endif /* AUDIODRV_COREAUDIO == 1 */
