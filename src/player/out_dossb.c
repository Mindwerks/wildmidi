/*
 * out_dossb.c -- DOS SoundBlaster output
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

#include "out_dossb.h"

#if (AUDIODRV_DOSSB == 1)

/* SoundBlaster/Pro/16/AWE32 driver for DOS -- adapted from
 * libMikMod,  written by Andrew Zabolotny <bit@eltech.ru>,
 * further fixes by O.Sezer <sezero@users.sourceforge.net>.
 * Timer callback functionality replaced by a push mechanism
 * to keep the wildmidi player changes to a minimum, for now.
 */

/* The last buffer byte filled with sound */
static unsigned int buff_tail = 0;

int write_sb_output(int8_t *data, unsigned int siz) {
    unsigned int dma_size, dma_pos;
    unsigned int cnt;

    sb_query_dma(&dma_size, &dma_pos);
    /* There isn't much sense in filling less than 256 bytes */
    dma_pos &= ~255;

    /* If nothing to mix, quit */
    if (buff_tail == dma_pos)
        return 0;

    /* If DMA pointer still didn't wrapped around ... */
    if (dma_pos > buff_tail) {
        if ((cnt = dma_pos - buff_tail) > siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        /* If we arrived right to the DMA buffer end, jump to the beginning */
        if (buff_tail >= dma_size)
            buff_tail = 0;
    } else {
        /* If wrapped around, fill first to the end of buffer */
        if ((cnt = dma_size - buff_tail) > siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        siz -= cnt;
        if (!siz) return cnt;

        /* Now fill from buffer beginning to current DMA pointer */
        if (dma_pos > siz) dma_pos = siz;
        data += cnt;
        cnt += dma_pos;

        memcpy(sb.dma_buff->linear, data, dma_pos);
        buff_tail = dma_pos;
    }
    return cnt;
}

int write_sb_s16stereo(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB16 sint16 stereo */
    int i;
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

static int write_sb_u8stereo(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB uint8 stereo */
    int16_t *src = (int16_t *) data;
    uint8_t *dst = (uint8_t *) data;
    int i = (siz /= 2);
    for (; i >= 0; --i) {
        *dst++ = (*src++ >> 8) + 128;
    }
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

int write_sb_u8mono(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB uint8 mono */
    int16_t *src = (int16_t *) data;
    uint8_t *dst = (uint8_t *) data;
    int i = (siz /= 4); int val;
    for (; i >= 0; --i) {
    /* do a cheap (left+right)/2 */
        val  = *src++;
        val += *src++;
        *dst++ = (val >> 9) + 128;
    }
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

void sb_silence_s16(void) {
    memset(sb.dma_buff->linear, 0, sb.dma_buff->size);
}

void sb_silence_u8(void) {
    memset(sb.dma_buff->linear, 0x80, sb.dma_buff->size);
}

void close_sb_output(void) {
    sb.timer_callback = NULL;
    sb_output(FALSE);
    sb_stop_dma();
    sb_close();
}

int open_sb_output(void) {
    if (!sb_open()) {
        fprintf(stderr, "Sound Blaster initialization failed.\n");
        return -1;
    }

    if (rate < 4000) rate = 4000;
    if (sb.caps & SBMODE_STEREO) {
        if (rate > sb.maxfreq_stereo)
            rate = sb.maxfreq_stereo;
    } else {
        if (rate > sb.maxfreq_mono)
            rate = sb.maxfreq_mono;
    }

    /* Enable speaker output */
    sb_output(TRUE);

    /* Set our routine to be called during SB IRQs */
    buff_tail = 0;
    sb.timer_callback = NULL;/* see above  */

    /* Start cyclic DMA transfer */
    if (!sb_start_dma(((sb.caps & SBMODE_16BITS) ? SBMODE_16BITS | SBMODE_SIGNED : 0) |
                            (sb.caps & SBMODE_STEREO), rate)) {
        sb_output(FALSE);
        sb_close();
        fprintf(stderr, "Sound Blaster: DMA start failed.\n");
        return -1;
    }

    if (sb.caps & SBMODE_16BITS) { /* can do stereo, too */
        send_output = write_sb_s16stereo;
        pause_output = sb_silence_s16;
        resume_output = resume_output_nop;
        printf("Sound Blaster 16 or compatible (16 bit, stereo, %u Hz)\n", rate);
    } else if (sb.caps & SBMODE_STEREO) {
        send_output = write_sb_u8stereo;
        pause_output = sb_silence_u8;
        resume_output = resume_output_nop;
        printf("Sound Blaster Pro or compatible (8 bit, stereo, %u Hz)\n", rate);
    } else {
        send_output = write_sb_u8mono;
        pause_output = sb_silence_u8;
        resume_output = resume_output_nop;
        printf("Sound Blaster %c or compatible (8 bit, mono, %u Hz)\n",
               (sb.dspver < SBVER_20)? '1' : '2', rate);
    }
    close_output = close_sb_output;

    return 0;
}




#endif // AUDIODRV_DOSSB == 1
