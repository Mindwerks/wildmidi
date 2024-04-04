/*
 * out_netbsd.c -- Audio output for NetBSD, based on XMP:
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

#include "config.h"

#ifdef AUDIODRV_NETBSD

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "wildplay.h"

static int audio_fd = -1;

static const int bsize = 32768;
static const int gain = 128;

static int open_netbsd_output(const char *pcmname, unsigned int *rate)
{
    audio_info_t ainfo;

    if (!pcmname || !*pcmname) {
        pcmname = "/dev/audio";
    }

    if ((audio_fd = open(pcmname, O_WRONLY)) < 0) {
        fprintf(stderr, "ERROR: Unable to open %s (%s)\r\n", pcmname, strerror(errno));
        return -1;
    }

    /* empty buffers before change config */
    ioctl(audio_fd, AUDIO_DRAIN, 0); /* drain everything out */
    ioctl(audio_fd, AUDIO_FLUSH);    /* flush audio */

    /* get audio parameters. */
    if (ioctl(audio_fd, AUDIO_GETINFO, &ainfo) < 0) {
        goto err0;
    }

    AUDIO_INITINFO(&ainfo);

    #if 0
    if (gain < AUDIO_MIN_GAIN)
        gain = AUDIO_MIN_GAIN;
    if (gain > AUDIO_MAX_GAIN)
        gain = AUDIO_MAX_GAIN;
    #endif

    ainfo.mode = AUMODE_PLAY_ALL;
    ainfo.play.sample_rate = *rate;
    ainfo.play.channels = 2;
    ainfo.play.precision = 16;
    ainfo.play.encoding = AUDIO_ENCODING_SLINEAR;
    ainfo.play.gain = gain;
    ainfo.play.buffer_size = bsize;
    ainfo.blocksize = 0;

    if (ioctl(audio_fd, AUDIO_SETINFO, &ainfo) < 0) {
        fprintf(stderr, "netbsdaudio: AUDIO_SETINFO failed!\r\n");
        goto err1;
    }
    if (ioctl(audio_fd, AUDIO_GETINFO, &ainfo) < 0) {
        err0:
        fprintf(stderr, "netbsdaudio: AUDIO_GETINFO failed!\r\n");
        goto err1;
    }
    if (ainfo.play.sample_rate != *rate) {
        if (ainfo.play.sample_rate > 65535) { /* WildMidi_Init() accepts uint16_t as rate */
            fprintf(stderr, "netbsdaudio: an unsupported sample rate (%uHz) was set\r\n", ainfo.play.sample_rate);
            goto err1;
        }
        fprintf(stderr, "netbsdaudio: sample rate set to %uHz instead of %u\r\n", ainfo.play.sample_rate, *rate);
    }
    *rate = ainfo.play.sample_rate;

    return 0;

  err1:
    close(audio_fd);
    return -1;
}

static int write_netbsd_output(void *buf, int len)
{
    int j;

    while (len) {
        if ((j = write(audio_fd, buf, len)) > 0) {
            len -= j;
            buf = (char *)buf + j;
        } else{
            fprintf(stderr, "\nnetbsdaudio: write failure to dsp: %s.\r\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

static void close_netbsd_output(void)
{
    if (audio_fd < 0)
        return;
    printf("Shutting down sound output\r\n");
    close(audio_fd);
    audio_fd = -1;
}

static void pause_netbsd_output(void) {}
static void resume_netbsd_output(void) {}

audiodrv_info audiodrv_netbsd = {
    "netbsd",
    "NetBSD PCM audio",
    open_netbsd_output,
    write_netbsd_output,
    close_netbsd_output,
    pause_netbsd_output,
    resume_netbsd_output
};

#endif /* AUDIODRV_NETBSD */
