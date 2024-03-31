/*
 * out_oss.c -- OSS output
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

#include "config.h"

#ifdef AUDIODRV_OSS

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/soundcard.h>

#include "wildplay.h"

#if !defined(AFMT_S16_NE)
#ifdef WORDS_BIGENDIAN
#define AFMT_S16_NE AFMT_S16_BE
#else
#define AFMT_S16_NE AFMT_S16_LE
#endif
#endif

#define DEFAULT_FRAGSIZE 14
#define DEFAULT_NUMFRAGS 16

static int audio_fd;

static void close_oss_output(void);

static void pause_oss_output(void) {
    ioctl(audio_fd, SNDCTL_DSP_POST, 0);
}
static void resume_oss_output(void) {}

static int open_oss_output(const char *pcmname, unsigned int *rate) {
    const unsigned int r = *rate;
    int tmp;

    if (!pcmname || !*pcmname) {
        pcmname = "/dev/dsp";
    }
    if ((audio_fd = open(pcmname, O_WRONLY)) < 0) {
        fprintf(stderr, "ERROR: Unable to open %s (%s)\r\n", pcmname, strerror(errno));
        return (-1);
    }
    if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) < 0) {
        fprintf(stderr, "ERROR: Unable to reset dsp\r\n");
        goto fail;
    }

    tmp = AFMT_S16_NE;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set 16bit sound format\r\n");
        goto fail;
    }

    tmp = 2;
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set stereo\r\n");
        goto fail;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, rate) < 0) {
        fprintf(stderr, "ERROR: Unable to set %uHz sample rate\r\n", r);
        goto fail;
    }
    if (r != *rate) {
        fprintf(stderr, "OSS: sample rate set to %uHz instead of %u\r\n", *rate, r);
    }

    tmp = (DEFAULT_NUMFRAGS<<16)|DEFAULT_FRAGSIZE;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set fragment size\r\n");
        goto fail;
    }

    return (0);

fail:
    close_oss_output();
    return (-1);
}

static int write_oss_output(void *data, int output_size) {
    const unsigned char *output_data = (unsigned char *)data;
    int res = 0;
    while (output_size > 0) {
        res = write(audio_fd, output_data, output_size);
        if (res > 0) {
            output_size -= res;
            output_data += res;
        } else {
            fprintf(stderr, "\nOSS: write failure to dsp: %s.\r\n", strerror(errno));
            return (-1);
        }
    }
    return (0);
}

static void close_oss_output(void) {
    if (audio_fd < 0)
        return;
    printf("Shutting down sound output\r\n");
    ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
    close(audio_fd);
    audio_fd = -1;
}

audiodrv_info audiodrv_oss = {
    "oss",
    "Open Sound System (OSS) output",
    open_oss_output,
    write_oss_output,
    close_oss_output,
    pause_oss_output,
    resume_oss_output
};

#endif /* AUDIODRV_OSS */
