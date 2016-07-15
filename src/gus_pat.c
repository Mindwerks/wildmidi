/*
 * gus_pat.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2016
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

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gus_pat.h"
#include "common.h"
#include "wm_error.h"
#include "file_io.h"
#include "sample.h"

//#define DEBUG_GUSPAT
#ifdef DEBUG_GUSPAT
#define GUSPAT_FILENAME_DEBUG(dx) fprintf(stderr,"\r%s\n",dx)

#define GUSPAT_INT_DEBUG(dx,dy) fprintf(stderr,"\r%s: %i\n",dx,dy)
#define GUSPAT_FLOAT_DEBUG(dx,dy) fprintf(stderr,"\r%s: %f\n",dx,dy)
#define GUSPAT_START_DEBUG() fprintf(stderr,"\r")
#define GUSPAT_MODE_DEBUG(dx,dy,dz) if (dx & dy) fprintf(stderr,"%s",dz)
#define GUSPAT_END_DEBUG() fprintf(stderr,"\n")
#else
#define GUSPAT_FILENAME_DEBUG(dx)
#define GUSPAT_INT_DEBUG(dx,dy)
#define GUSPAT_FLOAT_DEBUG(dx,dy)
#define GUSPAT_START_DEBUG()
#define GUSPAT_MODE_DEBUG(dx,dy,dz)
#define GUSPAT_END_DEBUG()
#endif

/* sample data conversion functions
 * convert data to signed shorts
 */

/* 8bit signed */
static int convert_8s(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((gus_sample->data_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = (*read_data++) << 8;
        } while (read_data != read_end);
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, 0, "calloc failed", errno);

    return -1;
}

/* 8bit signed ping pong */
static int convert_8sp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->loop_start;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((new_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = (*read_data++) << 8;
        } while (read_data != read_end);

        *write_data = (*read_data++ << 8);
        write_data_a = write_data + dloop_length;
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + dloop_length;
        read_end = data + gus_sample->loop_end;
        do {
            *write_data = (*read_data++) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data != read_end);

        *write_data = (*read_data++ << 8);
        *write_data_b++ = *write_data;
        read_end = data + gus_sample->data_length;
        if (__builtin_expect((read_data != read_end), 1)) {
            do {
                *write_data_b++ = (*read_data++) << 8;
            } while (read_data != read_end);
        }
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit signed reverse */
static int convert_8sr(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;
    uint32_t tmp_loop = 0;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((gus_sample->data_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data + gus_sample->data_length - 1;
        do {
            *write_data-- = (*read_data++) << 8;
        } while (read_data != read_end);
        tmp_loop = gus_sample->loop_end;
        gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
        gus_sample->loop_start = gus_sample->data_length - tmp_loop;
        gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
                | ((gus_sample->loop_fraction & 0xf0) >> 4);
        gus_sample->modes ^= SAMPLE_REVERSE;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit signed reverse ping pong */
static int convert_8srp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data + gus_sample->data_length - 1;
    uint8_t *read_end = data + gus_sample->loop_end;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((new_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = (*read_data--) << 8;
        } while (read_data != read_end);

        *write_data = (*read_data-- << 8);
        write_data_a = write_data + dloop_length;
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + dloop_length;
        read_end = data + gus_sample->loop_start;
        do {
            *write_data = (*read_data--) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data != read_end);

        *write_data = (*read_data-- << 8);
        *write_data_b++ = *write_data;
        read_end = data - 1;
        do {
            *write_data_b++ = (*read_data--) << 8;
            write_data_b++;
        } while (read_data != read_end);
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit unsigned */
static int convert_8u(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((gus_sample->data_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = ((*read_data++) ^ 0x80) << 8;
        } while (read_data != read_end);
        gus_sample->modes ^= SAMPLE_UNSIGNED;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit unsigned ping pong */
static int convert_8up(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->loop_start;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((new_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = ((*read_data++) ^ 0x80) << 8;
        } while (read_data != read_end);

        *write_data = ((*read_data++) ^ 0x80) << 8;
        write_data_a = write_data + dloop_length;
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + dloop_length;
        read_end = data + gus_sample->loop_end;
        do {
            *write_data = ((*read_data++) ^ 0x80) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data != read_end);

        *write_data = ((*read_data++) ^ 0x80) << 8;
        *write_data_b++ = *write_data;
        read_end = data + gus_sample->data_length;
        if (__builtin_expect((read_data != read_end), 1)) {
            do {
                *write_data_b++ = ((*read_data++) ^ 0x80) << 8;
            } while (read_data != read_end);
        }
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_UNSIGNED;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit unsigned reverse */
static int convert_8ur(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;
    uint32_t tmp_loop = 0;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((gus_sample->data_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data + gus_sample->data_length - 1;
        do {
            *write_data-- = ((*read_data++) ^ 0x80) << 8;
        } while (read_data != read_end);
        tmp_loop = gus_sample->loop_end;
        gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
        gus_sample->loop_start = gus_sample->data_length - tmp_loop;
        gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
                | ((gus_sample->loop_fraction & 0xf0) >> 4);
        gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 8bit unsigned reverse ping pong */
static int convert_8urp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data + gus_sample->data_length - 1;
    uint8_t *read_end = data + gus_sample->loop_end;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc((new_length + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data++ = ((*read_data--) ^ 0x80) << 8;
        } while (read_data != read_end);

        *write_data = ((*read_data--) ^ 0x80) << 8;
        write_data_a = write_data + dloop_length;
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + dloop_length;
        read_end = data + gus_sample->loop_start;
        do {
            *write_data = ((*read_data--) ^ 0x80) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data != read_end);

        *write_data = ((*read_data--) ^ 0x80) << 8;
        *write_data_b++ = *write_data;
        read_end = data - 1;
        do {
            *write_data_b++ = ((*read_data--) ^ 0x80) << 8;
        } while (read_data != read_end);
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit signed */
static int convert_16s(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((gus_sample->data_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = *read_data++;
            *write_data++ |= (*read_data++) << 8;
        } while (read_data < read_end);

        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit signed ping pong */
static int convert_16sp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->loop_start;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((new_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = (*read_data++);
            *write_data++ |= (*read_data++) << 8;
        } while (read_data < read_end);

        *write_data = (*read_data++);
        *write_data |= (*read_data++) << 8;
        write_data_a = write_data + (dloop_length >> 1);
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + (dloop_length >> 1);
        read_end = data + gus_sample->loop_end;
        do {
            *write_data = (*read_data++);
            *write_data |= (*read_data++) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data < read_end);

        *write_data = *(read_data++);
        *write_data |= (*read_data++) << 8;
        *write_data_b++ = *write_data;
        read_end = data + gus_sample->data_length;
        if (__builtin_expect((read_data != read_end), 1)) {
            do {
                *write_data_b = *(read_data++);
                *write_data_b++ |= (*read_data++) << 8;
            } while (read_data < read_end);
        }
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG;
        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit signed reverse */
static int convert_16sr(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;
    uint32_t tmp_loop = 0;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((gus_sample->data_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
        do {
            *write_data = *read_data++;
            *write_data-- |= (*read_data++) << 8;
        } while (read_data < read_end);
        tmp_loop = gus_sample->loop_end;
        gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
        gus_sample->loop_start = gus_sample->data_length - tmp_loop;
        gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
                | ((gus_sample->loop_fraction & 0xf0) >> 4);
        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        gus_sample->modes ^= SAMPLE_REVERSE;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit signed reverse ping pong */
static int convert_16srp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data + gus_sample->data_length - 1;
    uint8_t *read_end = data + gus_sample->loop_end;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((new_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = (*read_data--) << 8;
            *write_data++ |= *read_data--;
        } while (read_data < read_end);

        *write_data = (*read_data-- << 8);
        *write_data |= *read_data--;
        write_data_a = write_data + (dloop_length >> 1);
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + (dloop_length >> 1);
        read_end = data + gus_sample->loop_start;
        do {
            *write_data = (*read_data--) << 8;
            *write_data |= *read_data--;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data < read_end);

        *write_data = ((*read_data--) << 8);
        *write_data |= *read_data--;
        *write_data_b++ = *write_data;
        read_end = data - 1;
        do {
            *write_data_b = (*read_data--) << 8;
            *write_data_b++ |= *read_data--;
        } while (read_data < read_end);
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit unsigned */
static int convert_16u(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((gus_sample->data_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = *read_data++;
            *write_data++ |= ((*read_data++) ^ 0x80) << 8;
        } while (read_data < read_end);
        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        gus_sample->modes ^= SAMPLE_UNSIGNED;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit unsigned ping pong */
static int convert_16up(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->loop_start;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((new_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = (*read_data++);
            *write_data++ |= ((*read_data++) ^ 0x80) << 8;
        } while (read_data < read_end);

        *write_data = (*read_data++);
        *write_data |= ((*read_data++) ^ 0x80) << 8;
        write_data_a = write_data + (dloop_length >> 1);
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + (dloop_length >> 1);
        read_end = data + gus_sample->loop_end;
        do {
            *write_data = (*read_data++);
            *write_data |= ((*read_data++) ^ 0x80) << 8;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data < read_end);

        *write_data = (*read_data++);
        *write_data |= ((*read_data++) ^ 0x80) << 8;
        *write_data_b++ = *write_data;
        read_end = data + gus_sample->data_length;
        if (__builtin_expect((read_data != read_end), 1)) {
            do {
                *write_data_b = (*read_data++);
                *write_data_b++ |= ((*read_data++) ^ 0x80) << 8;
            } while (read_data < read_end);
        }
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG;
        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit unsigned reverse */
static int convert_16ur(uint8_t *data, struct _sample *gus_sample) {
    uint8_t *read_data = data;
    uint8_t *read_end = data + gus_sample->data_length;
    int16_t *write_data = NULL;
    uint32_t tmp_loop = 0;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((gus_sample->data_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
        do {
            *write_data = *read_data++;
            *write_data-- |= ((*read_data++) ^ 0x80) << 8;
        } while (read_data < read_end);
        tmp_loop = gus_sample->loop_end;
        gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
        gus_sample->loop_start = gus_sample->data_length - tmp_loop;
        gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
                | ((gus_sample->loop_fraction & 0xf0) >> 4);
        gus_sample->loop_start >>= 1;
        gus_sample->loop_end >>= 1;
        gus_sample->data_length >>= 1;
        gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
        return 0;
    }
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* 16bit unsigned reverse ping pong */
static int convert_16urp(uint8_t *data, struct _sample *gus_sample) {
    uint32_t loop_length = gus_sample->loop_end
            - gus_sample->loop_start;
    uint32_t dloop_length = loop_length * 2;
    uint32_t new_length = gus_sample->data_length + dloop_length;
    uint8_t *read_data = data + gus_sample->data_length - 1;
    uint8_t *read_end = data + gus_sample->loop_end;
    int16_t *write_data = NULL;
    int16_t *write_data_a = NULL;
    int16_t *write_data_b = NULL;

    SAMPLE_CONVERT_DEBUG(__FUNCTION__);
    gus_sample->data = calloc(((new_length >> 1) + 2), sizeof(int16_t));
    if (__builtin_expect((gus_sample->data != NULL), 1)) {
        write_data = gus_sample->data;
        do {
            *write_data = ((*read_data--) ^ 0x80) << 8;
            *write_data++ |= *read_data--;
        } while (read_data < read_end);

        *write_data = ((*read_data--) ^ 0x80) << 8;
        *write_data |= *read_data--;
        write_data_a = write_data + (dloop_length >> 1);
        *write_data_a-- = *write_data;
        write_data++;
        write_data_b = write_data + (dloop_length >> 1);
        read_end = data + gus_sample->loop_start;
        do {
            *write_data = ((*read_data--) ^ 0x80) << 8;
            *write_data |= *read_data--;
            *write_data_a-- = *write_data;
            *write_data_b++ = *write_data;
            write_data++;
        } while (read_data < read_end);

        *write_data = ((*read_data--) ^ 0x80) << 8;
        *write_data |= *read_data--;
        *write_data_b++ = *write_data;
        read_end = data - 1;
        do {
            *write_data_b = ((*read_data--) ^ 0x80) << 8;
            *write_data_b++ |= *read_data--;
        } while (read_data < read_end);
        gus_sample->loop_start += loop_length;
        gus_sample->loop_end += dloop_length;
        gus_sample->data_length = new_length;
        gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
        return 0;
    }

    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
    return -1;
}

/* sample loading */

struct _sample * _WM_load_gus_pat(const char *filename, int fix_release) {
    uint8_t *gus_patch;
    uint32_t gus_size;
    uint32_t gus_ptr;
    uint8_t no_of_samples;
    struct _sample *gus_sample = NULL;
    struct _sample *first_gus_sample = NULL;
    uint32_t i = 0;

    int (*do_convert[])(uint8_t *data, struct _sample *gus_sample) = {
        convert_8s,
        convert_16s,
        convert_8u,
        convert_16u,
        convert_8sp,
        convert_16sp,
        convert_8up,
        convert_16up,
        convert_8sr,
        convert_16sr,
        convert_8ur,
        convert_16ur,
        convert_8srp,
        convert_16srp,
        convert_8urp,
        convert_16urp
    };
    uint32_t tmp_loop;

    UNUSED(fix_release);

    SAMPLE_CONVERT_DEBUG(__FUNCTION__); SAMPLE_CONVERT_DEBUG(filename);

    if ((gus_patch = (uint8_t *) _WM_BufferFile(filename, &gus_size)) == NULL) {
        return NULL;
    }
    if (gus_size < 239) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, filename, 0);
        free(gus_patch);
        return NULL;
    }
    if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22)
            && memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, filename, 0);
        free(gus_patch);
        return NULL;
    }
    if (gus_patch[82] > 1) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, filename, 0);
        free(gus_patch);
        return NULL;
    }
    if (gus_patch[151] > 1) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, filename, 0);
        free(gus_patch);
        return NULL;
    }

    GUSPAT_FILENAME_DEBUG(filename);
    GUSPAT_INT_DEBUG("voices",gus_patch[83]);

    no_of_samples = gus_patch[198];
    gus_ptr = 239;
    while (no_of_samples) {
        uint32_t tmp_cnt;
        if (first_gus_sample == NULL) {
            first_gus_sample = malloc(sizeof(struct _sample));
            gus_sample = first_gus_sample;
        } else {
            gus_sample->next = malloc(sizeof(struct _sample));
            gus_sample = gus_sample->next;
        }
        if (gus_sample == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, filename, 0);
            free(gus_patch);
            return NULL;
        }

        gus_sample->next = NULL;
        gus_sample->loop_fraction = gus_patch[gus_ptr + 7];
        gus_sample->data_length = (gus_patch[gus_ptr + 11] << 24)
                                | (gus_patch[gus_ptr + 10] << 16)
                                | (gus_patch[gus_ptr + 9]  <<  8)
                                |  gus_patch[gus_ptr + 8];
        gus_sample->loop_start  = (gus_patch[gus_ptr + 15] << 24)
                                | (gus_patch[gus_ptr + 14] << 16)
                                | (gus_patch[gus_ptr + 13] <<  8)
                                |  gus_patch[gus_ptr + 12];
        gus_sample->loop_end    = (gus_patch[gus_ptr + 19] << 24)
                                | (gus_patch[gus_ptr + 18] << 16)
                                | (gus_patch[gus_ptr + 17] <<  8)
                                |  gus_patch[gus_ptr + 16];
        gus_sample->rate        = (gus_patch[gus_ptr + 21] << 8)
                                |  gus_patch[gus_ptr + 20];
        gus_sample->freq_low    = (gus_patch[gus_ptr + 25] << 24)
                                | (gus_patch[gus_ptr + 24] << 16)
                                | (gus_patch[gus_ptr + 23] <<  8)
                                |  gus_patch[gus_ptr + 22];
        gus_sample->freq_high   = (gus_patch[gus_ptr + 29] << 24)
                                | (gus_patch[gus_ptr + 28] << 16)
                                | (gus_patch[gus_ptr + 27] <<  8)
                                |  gus_patch[gus_ptr + 26];
        gus_sample->freq_root   = (gus_patch[gus_ptr + 33] << 24)
                                | (gus_patch[gus_ptr + 32] << 16)
                                | (gus_patch[gus_ptr + 31] <<  8)
                                |  gus_patch[gus_ptr + 30];

        /* This is done this way instead of ((freq * 1024) / rate) to avoid 32bit overflow. */
        /* Result is 0.001% inacurate */
        gus_sample->inc_div = ((gus_sample->freq_root * 512) / gus_sample->rate) * 2;

#if 0
        /* We dont use this info at this time, kept in here for info */
        printf("\rTremolo Sweep: %i, Rate: %i, Depth %i\n",
                gus_patch[gus_ptr+49], gus_patch[gus_ptr+50], gus_patch[gus_ptr+51]);
        printf("\rVibrato Sweep: %i, Rate: %i, Depth %i\n",
                gus_patch[gus_ptr+52], gus_patch[gus_ptr+53], gus_patch[gus_ptr+54]);
#endif
        gus_sample->modes = gus_patch[gus_ptr + 55];
        GUSPAT_START_DEBUG(); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_16BIT, "16bit "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_UNSIGNED, "Unsigned "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_LOOP, "Loop "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_PINGPONG, "PingPong "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_REVERSE, "Reverse "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_SUSTAIN, "Sustain "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_ENVELOPE, "Envelope "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_CLAMPED, "Clamped "); GUSPAT_END_DEBUG();

        if (gus_sample->loop_start > gus_sample->loop_end) {
            tmp_loop = gus_sample->loop_end;
            gus_sample->loop_end = gus_sample->loop_start;
            gus_sample->loop_start = tmp_loop;
            gus_sample->loop_fraction =
                    ((gus_sample->loop_fraction & 0x0f) << 4)
                            | ((gus_sample->loop_fraction & 0xf0) >> 4);
        }

        /*
         FIXME: Experimental Hacky Fix

         This looks for "dodgy" release envelope settings that faulty editors
         may have set and attempts to corrects it.

         if (fix_release)
         Lets make this automatic ...
         */
        {
            /*
             After studying faulty gus_pats this way may work better
             Testing to determine if any further adjustments are required
             */
            if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 41]]) {
                uint8_t tmp_hack_rate = 0;

                if (env_time_table[gus_patch[gus_ptr + 41]] < env_time_table[gus_patch[gus_ptr + 42]]) {
                    // 1 2 3
                    tmp_hack_rate = gus_patch[gus_ptr + 40];
                    gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
                    gus_patch[gus_ptr + 42] = tmp_hack_rate;
                } else if (env_time_table[gus_patch[gus_ptr + 41]] == env_time_table[gus_patch[gus_ptr + 42]]) {
                    // 1 2 2
                    tmp_hack_rate = gus_patch[gus_ptr + 40];
                    gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
                    gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
                    gus_patch[gus_ptr + 42] = tmp_hack_rate;

                } else {
                    if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 42]]) {
                        // 1 3 2
                        tmp_hack_rate = gus_patch[gus_ptr + 40];
                        gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 41];
                        gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
                        gus_patch[gus_ptr + 42] = tmp_hack_rate;
                    } else {
                        // 2 3 1 or 1 2 1
                        tmp_hack_rate = gus_patch[gus_ptr + 40];
                        gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 41];
                        gus_patch[gus_ptr + 41] = tmp_hack_rate;
                    }
                }
            } else if (env_time_table[gus_patch[gus_ptr + 41]] < env_time_table[gus_patch[gus_ptr + 42]]) {
                uint8_t tmp_hack_rate = 0;

                if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 42]]) {
                    // 2 1 3
                    tmp_hack_rate = gus_patch[gus_ptr + 40];
                    gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
                    gus_patch[gus_ptr + 42] = gus_patch[gus_ptr + 41];
                    gus_patch[gus_ptr + 41] = tmp_hack_rate;
                } else {
                    // 3 1 2
                    tmp_hack_rate = gus_patch[gus_ptr + 41];
                    gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
                    gus_patch[gus_ptr + 42] = tmp_hack_rate;
                }
            }

#if 0
            if ((env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 41]]) && (env_time_table[gus_patch[gus_ptr + 41]] == env_time_table[gus_patch[gus_ptr + 42]])) {
                uint8_t tmp_hack_rate = 0;
                tmp_hack_rate = gus_patch[gus_ptr + 41];
                gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 40];
                gus_patch[gus_ptr + 42] = gus_patch[gus_ptr + 40];
                gus_patch[gus_ptr + 40] = tmp_hack_rate;
                tmp_hack_rate = gus_patch[gus_ptr + 47];
                gus_patch[gus_ptr + 47] = gus_patch[gus_ptr + 46];
                gus_patch[gus_ptr + 48] = gus_patch[gus_ptr + 46];
                gus_patch[gus_ptr + 46] = tmp_hack_rate;
            }
#endif
        }

        for (i = 0; i < 6; i++) {
            GUSPAT_INT_DEBUG("Envelope #",i);
            if (gus_sample->modes & SAMPLE_ENVELOPE) {
                uint8_t env_rate = gus_patch[gus_ptr + 37 + i];
                gus_sample->env_target[i] = 16448 * gus_patch[gus_ptr + 43 + i];
                GUSPAT_INT_DEBUG("Envelope Level",gus_patch[gus_ptr+43+i]); GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[env_rate]);
                gus_sample->env_rate[i] = (int32_t) (4194303.0
                        / ((float) _WM_SampleRate * env_time_table[env_rate]));
                GUSPAT_INT_DEBUG("Envelope Rate",gus_sample->env_rate[i]); GUSPAT_INT_DEBUG("GUSPAT Rate",env_rate);
                if (gus_sample->env_rate[i] == 0) {
                    _WM_DEBUG_MSG("%s: Warning: found invalid envelope(%u) rate setting in %s. Using %f instead.",
                            __FUNCTION__, i, filename, env_time_table[63]);
                    gus_sample->env_rate[i] = (int32_t) (4194303.0
                            / ((float) _WM_SampleRate * env_time_table[63]));
                    GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[63]);
                }
            } else {
                gus_sample->env_target[i] = 4194303;
                gus_sample->env_rate[i] = (int32_t) (4194303.0
                        / ((float) _WM_SampleRate * env_time_table[63]));
                GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[63]);
            }
        }

        gus_sample->env_target[6] = 0;
        gus_sample->env_rate[6] = (int32_t) (4194303.0
                / ((float) _WM_SampleRate * env_time_table[63]));

        gus_ptr += 96;
        tmp_cnt = gus_sample->data_length;

        if (do_convert[(((gus_sample->modes & 0x18) >> 1)
                | (gus_sample->modes & 0x03))](&gus_patch[gus_ptr], gus_sample)
                == -1) {
            free(gus_patch);
            return NULL;
        }

        /*
         Test and set decay expected decay time after a note off
         NOTE: This sets samples for full range decay
         */
        if (gus_sample->modes & SAMPLE_ENVELOPE) {
            float samples_f = 0.0;

            if (gus_sample->modes & SAMPLE_CLAMPED) {
                samples_f = (4194301.0 - (float)gus_sample->env_target[5]) / gus_sample->env_rate[5];
            } else {
                if (gus_sample->modes & SAMPLE_SUSTAIN) {
                    samples_f = (4194301.0 - (float)gus_sample->env_target[3]) / gus_sample->env_rate[3];
                    samples_f += (float)(gus_sample->env_target[3] - gus_sample->env_target[4]) / gus_sample->env_rate[4];
                } else {
                    samples_f = (4194301.0 - (float)gus_sample->env_target[4]) / gus_sample->env_rate[4];
                }
                samples_f += (float)(gus_sample->env_target[4] - gus_sample->env_target[5]) / gus_sample->env_rate[5];
            }
            samples_f += (float)gus_sample->env_target[5] / gus_sample->env_rate[6];

            gus_sample->note_off_decay = (uint32_t)samples_f;

        } else {
            gus_sample->note_off_decay = gus_sample->data_length * _WM_SampleRate / gus_sample->rate;
        }

        gus_ptr += tmp_cnt;
        gus_sample->loop_start = (gus_sample->loop_start << 10)
                | (((gus_sample->loop_fraction & 0x0f) << 10) / 16);
        gus_sample->loop_end = (gus_sample->loop_end << 10)
                | (((gus_sample->loop_fraction & 0xf0) << 6) / 16);
        gus_sample->loop_size = gus_sample->loop_end - gus_sample->loop_start;
        gus_sample->data_length = gus_sample->data_length << 10;
        no_of_samples--;
    }
    free(gus_patch);
    return first_gus_sample;
}
