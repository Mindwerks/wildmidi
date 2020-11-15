/*
 * out_wave.h -- WAVE output
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

#include "out_wave.h"

#if (AUDIODRV_WAVE == 1)

extern char wav_file[1024];
extern unsigned int rate;
extern int audio_fd;

uint32_t wav_size;

// FIXME move this to platform depended files
#define wmidi_geterrno() errno
#define WM_IS_BADF(_fd) ((_fd) < 0)
#define WM_BADF (-1)
extern int wmidi_fileexists(const char *path);
extern int wmidi_open_write(const char *path);
extern ssize_t wmidi_write(int fd, const void *buf, size_t size);
extern int wmidi_close(int fd);
extern off_t wmidi_seekset(int fd, off_t ofs);

int open_wav_output(void) {
    uint8_t wav_hdr[] = {
        0x52, 0x49,
        0x46, 0x46, /* "RIFF"  */
        0x00, 0x00,
        0x00, 0x00, /* riffsize: pcm size + 36 (filled when closing.) */
        0x57, 0x41,
        0x56, 0x45, /* "WAVE"  */
        0x66, 0x6D,
        0x74, 0x20, /* "fmt "  */
        0x10, 0x00,
        0x00, 0x00, /* length of this RIFF block: 16  */
        0x01, 0x00, /* wave format == 1 (WAVE_FORMAT_PCM)  */
        0x02, 0x00, /* channels == 2  */
        0x00, 0x00,
        0x00, 0x00, /* sample rate (filled below)  */
        0x00, 0x00,
        0x00, 0x00, /* bytes_per_sec: rate * channels * format bytes  */
        0x04, 0x00, /* block alignment: channels * format bytes == 4  */
        0x10, 0x00, /* format bits == 16  */
        0x64, 0x61,
        0x74, 0x61, /* "data"  */
        0x00, 0x00,
        0x00, 0x00 /* datasize: the pcm size (filled when closing.)  */
    };

    if (wav_file[0] == '\0') {
        return (-1);
    }

    audio_fd = wmidi_open_write(wav_file);
    if (WM_IS_BADF(audio_fd)) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n",
                strerror(wmidi_geterrno()));
        return (-1);
    } else {
        uint32_t bytes_per_sec;

        wav_hdr[24] = (rate) & 0xFF;
        wav_hdr[25] = (rate >> 8) & 0xFF;

        bytes_per_sec = rate * 4;
        wav_hdr[28] = (bytes_per_sec) & 0xFF;
        wav_hdr[29] = (bytes_per_sec >> 8) & 0xFF;
        wav_hdr[30] = (bytes_per_sec >> 16) & 0xFF;
        wav_hdr[31] = (bytes_per_sec >> 24) & 0xFF;
    }

    if (wmidi_write(audio_fd, wav_hdr, 44) < 0) {
        fprintf(stderr, "ERROR: failed writing wav header (%s)\r\n",
                strerror(wmidi_geterrno()));
        wmidi_close(audio_fd);
        audio_fd = WM_BADF;
        return (-1);
    }

    wav_size = 0;

    return (0);
}

int write_wav_output(int8_t *output_data, int output_size) {
#ifdef WORDS_BIGENDIAN
    /* libWildMidi outputs host-endian, *.wav must have little-endian. */
    uint16_t *swp = (uint16_t *)output_data;
    int i = (output_size / 2) - 1;
    for (; i >= 0; --i) {
        swp[i] = (swp[i] << 8) | (swp[i] >> 8);
    }
#endif
    if (wmidi_write(audio_fd, output_data, output_size) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n",
              strerror(wmidi_geterrno()));
        wmidi_close(audio_fd);
        audio_fd = WM_BADF;
        return (-1);
    }

    wav_size += output_size;
    return (0);
}

void close_wav_output(void) {
    uint8_t wav_count[4];
    if (WM_IS_BADF(audio_fd))
        return;

    printf("Finishing and closing wav output\r");
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    wmidi_seekset(audio_fd, 40);
    if (wmidi_write(audio_fd, wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n",
                strerror(wmidi_geterrno()));
        goto end;
    }

    wav_size += 36;
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    wmidi_seekset(audio_fd, 4);
    if (wmidi_write(audio_fd, wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n",
                strerror(wmidi_geterrno()));
        goto end;
    }

end:
    printf("\n");
    if (!WM_IS_BADF(audio_fd)) {
        wmidi_close(audio_fd);
    }
    audio_fd = WM_BADF;
}

#endif // (AUDIODRV_WAVE == 1)
