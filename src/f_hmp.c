/*
 * hmp.c -- Midi Wavetable Processing library
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "wm_error.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "reverb.h"
#include "f_hmp.h"

/*
 Turns hmp file data into an event stream
 */
struct _mdi *
_WM_ParseNewHmp(uint8_t *hmp_data, uint32_t hmp_size) {
    uint8_t is_hmp2 = 0;
    uint32_t zero_cnt = 0;
    uint32_t i = 0;
    uint32_t hmp_file_length = 0;
    uint32_t hmp_chunks = 0;
    uint32_t hmp_divisions = 0;
    uint32_t hmp_unknown = 0;
    uint32_t hmp_bpm = 0;
    uint32_t hmp_song_time = 0;
    struct _mdi *hmp_mdi;
    uint8_t **hmp_chunk;
    uint32_t *chunk_length;
    uint32_t *chunk_ofs;
    uint32_t *chunk_delta;
    uint8_t *chunk_end;
    uint32_t chunk_num = 0;
    uint32_t hmp_track = 0;
//  uint32_t j = 0;
    uint32_t smallest_delta = 0;
    uint32_t subtract_delta = 0;
//  uint32_t chunks_finished = 0;
    uint32_t end_of_chunks = 0;
    uint32_t var_len_shift = 0;

    float tempo_f = 500000.0;
    float samples_per_delta_f = 0.0;

//  uint8_t hmp_event = 0;
//  uint8_t hmp_channel = 0;

    uint32_t sample_count = 0;
    float sample_count_f = 0;
    float sample_remainder = 0;

    if (memcmp(hmp_data, "HMIMIDIP", 8)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_HMP, NULL, 0);
        return NULL;
    }
    hmp_data += 8;
    hmp_size -= 8;

    if (!memcmp(hmp_data, "013195", 6)) {
        hmp_data += 6;
        hmp_size -= 6;
        is_hmp2 = 1;
    }

    // should be a bunch of \0's
    if (is_hmp2) {
        zero_cnt = 18;
    } else {
        zero_cnt = 24;
    }
    for (i = 0; i < zero_cnt; i++) {
        if (hmp_data[i] != 0) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_HMP, NULL, 0);
            return NULL;
        }
    }
    hmp_data += zero_cnt;
    hmp_size -= zero_cnt;

    hmp_file_length = *hmp_data++;
    hmp_file_length += (*hmp_data++ << 8);
    hmp_file_length += (*hmp_data++ << 16);
    hmp_file_length += (*hmp_data++ << 24);
    hmp_size -= 4;

    UNUSED(hmp_file_length);

    // Next 12 bytes are normally \0 so skipping over them
    hmp_data += 12;
    hmp_size -= 12;

    hmp_chunks = *hmp_data++;
    hmp_chunks += (*hmp_data++ << 8);
    hmp_chunks += (*hmp_data++ << 16);
    hmp_chunks += (*hmp_data++ << 24);
    hmp_size -= 4;

    // Still decyphering what this is
    hmp_unknown = *hmp_data++;
    hmp_unknown += (*hmp_data++ << 8);
    hmp_unknown += (*hmp_data++ << 16);
    hmp_unknown += (*hmp_data++ << 24);
    hmp_size -= 4;

    UNUSED(hmp_unknown);

    // Defaulting: experimenting has found this to be the ideal value
    hmp_divisions = 60;

    // Beats per minute
    hmp_bpm = *hmp_data++;
    hmp_bpm += (*hmp_data++ << 8);
    hmp_bpm += (*hmp_data++ << 16);
    hmp_bpm += (*hmp_data++ << 24);
    hmp_size -= 4;

    /* Slow but needed for accuracy */
    if ((_WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
        tempo_f = (float) (60000000 / hmp_bpm) + 0.5f;
    } else {
        tempo_f = (float) (60000000 / hmp_bpm);
    }

    samples_per_delta_f = _WM_GetSamplesPerTick(hmp_divisions, tempo_f);

    //DEBUG
    //fprintf(stderr, "DEBUG: Samples Per Delta Tick: %f\r\n",samples_per_delta_f);

    // FIXME: This value is incorrect
    hmp_song_time = *hmp_data++;
    hmp_song_time += (*hmp_data++ << 8);
    hmp_song_time += (*hmp_data++ << 16);
    hmp_song_time += (*hmp_data++ << 24);
    hmp_size -= 4;

    // DEBUG
    //fprintf(stderr,"DEBUG: ??DIVISIONS??: %u, BPM: %u, ??SONG TIME??: %u:%.2u\r\n",hmp_divisions, hmp_bpm, (hmp_song_time / 60), (hmp_song_time % 60));

    UNUSED(hmp_song_time);

    if (is_hmp2) {
        hmp_data += 840;
        hmp_size -= 840;
    } else {
        hmp_data += 712;
        hmp_size -= 712;
    }

    hmp_mdi = _WM_initMDI();

    _WM_midi_setup_divisions(hmp_mdi, hmp_divisions);
    _WM_midi_setup_tempo(hmp_mdi, (uint32_t)tempo_f);

    hmp_chunk = malloc(sizeof(uint8_t *) * hmp_chunks);
    chunk_length = malloc(sizeof(uint32_t) * hmp_chunks);
    chunk_delta = malloc(sizeof(uint32_t) * hmp_chunks);
    chunk_ofs = malloc(sizeof(uint32_t) * hmp_chunks);
    chunk_end = malloc(sizeof(uint8_t) * hmp_chunks);

    smallest_delta = 0xffffffff;
    // store chunk info for use, and check chunk lengths
    for (i = 0; i < hmp_chunks; i++) {
        hmp_chunk[i] = hmp_data;
        chunk_ofs[i] = 0;

        chunk_num = *hmp_data++;
        chunk_num += (*hmp_data++ << 8);
        chunk_num += (*hmp_data++ << 16);
        chunk_num += (*hmp_data++ << 24);
        chunk_ofs[i] += 4;

        UNUSED(chunk_num);

        chunk_length[i] = *hmp_data++;
        chunk_length[i] += (*hmp_data++ << 8);
        chunk_length[i] += (*hmp_data++ << 16);
        chunk_length[i] += (*hmp_data++ << 24);
        chunk_ofs[i] += 4;

        if (chunk_length[i] > hmp_size) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_HMP, "file too short", 0);
            goto _hmp_end;
        }
        hmp_size -= chunk_length[i];

        hmp_track = *hmp_data++;
        hmp_track += (*hmp_data++ << 8);
        hmp_track += (*hmp_data++ << 16);
        hmp_track += (*hmp_data++ << 24);
        chunk_ofs[i] += 4;

        UNUSED(hmp_track);

        // Start of Midi Data
        chunk_delta[i] = 0;
        var_len_shift = 0;
        if (*hmp_data < 0x80) {
            do {
                chunk_delta[i] = chunk_delta[i] | ((*hmp_data++ & 0x7F) << var_len_shift);
                var_len_shift += 7;
                chunk_ofs[i]++;
            } while (*hmp_data < 0x80);
        }
        chunk_delta[i] = chunk_delta[i] | ((*hmp_data++ & 0x7F) << var_len_shift);
        chunk_ofs[i]++;

        if (chunk_delta[i] < smallest_delta) {
            smallest_delta = chunk_delta[i];
        }

        // goto start of next chunk
        hmp_data = hmp_chunk[i] + chunk_length[i];
        hmp_chunk[i] += chunk_ofs[i]++;
        chunk_end[i] = 0;
    }

    subtract_delta = smallest_delta;
    sample_count_f = (((float) smallest_delta * samples_per_delta_f) + sample_remainder);

    sample_count = (uint32_t) sample_count_f;
    sample_remainder = sample_count_f - (float) sample_count;

    hmp_mdi->events[hmp_mdi->event_count - 1].samples_to_next += sample_count;
    hmp_mdi->extra_info.approx_total_samples += sample_count;

    while (end_of_chunks < hmp_chunks) {
        smallest_delta = 0;

        // DEBUG
        // fprintf(stderr,"DEBUG: Delta Ticks: %u\r\n",subtract_delta);

        for (i = 0; i < hmp_chunks; i++) {
            if (chunk_end[i])
                continue;

            if (chunk_delta[i]) {
                chunk_delta[i] -= subtract_delta;
                if (chunk_delta[i]) {
                    if ((!smallest_delta)
                        || (smallest_delta > chunk_delta[i])) {
                        smallest_delta = chunk_delta[i];
                    }
                    continue;
                }
            }
            do {
                if (((hmp_chunk[i][0] & 0xf0) == 0xb0 ) && ((hmp_chunk[i][1] == 110) || (hmp_chunk[i][1] == 111)) && (hmp_chunk[i][2] > 0x7f)) {
                    // Reserved for loop markers
                    // TODO: still deciding what to do about these
                    hmp_chunk[i] += 3;
                } else {
                    uint32_t setup_ret = 0;

                    if ((setup_ret = _WM_SetupMidiEvent(hmp_mdi, hmp_chunk[i], 0)) == 0) {
                        goto _hmp_end;
                    }

                    if ((hmp_chunk[i][0] == 0xff) && (hmp_chunk[i][1] == 0x2f) && (hmp_chunk[i][2] == 0x00)) {
                        /* End of Chunk */
                        end_of_chunks++;
                        chunk_end[i] = 1;
                        hmp_chunk[i] += 3;
                        goto NEXT_CHUNK;
                    } else if ((hmp_chunk[i][0] == 0xff) && (hmp_chunk[i][1] == 0x51) && (hmp_chunk[i][2] == 0x03)) {
                        /* Tempo */
                        tempo_f = (float)((hmp_chunk[i][3] << 16) + (hmp_chunk[i][4] << 8)+ hmp_chunk[i][5]);
                        if (tempo_f == 0.0)
                            tempo_f = 500000.0;

                        // DEBUG
                        fprintf(stderr,"DEBUG: Tempo change %f\r\n", tempo_f);
                    }
                    hmp_chunk[i] += setup_ret;
                }
                var_len_shift = 0;
                chunk_delta[i] = 0;
                if (*hmp_chunk[i] < 0x80) {
                    do {
                        chunk_delta[i] = chunk_delta[i] + ((*hmp_chunk[i] & 0x7F) << var_len_shift);
                        var_len_shift += 7;
                        hmp_chunk[i]++;
                    } while (*hmp_chunk[i] < 0x80);
                }
                chunk_delta[i] = chunk_delta[i] + ((*hmp_chunk[i] & 0x7F) << var_len_shift);
                hmp_chunk[i]++;
            } while (!chunk_delta[i]);

            if ((!smallest_delta) || (smallest_delta > chunk_delta[i])) {
                smallest_delta = chunk_delta[i];
            }
        NEXT_CHUNK: continue;
        }

        subtract_delta = smallest_delta;
        sample_count_f= (((float) smallest_delta * samples_per_delta_f) + sample_remainder);

        sample_count = (uint32_t) sample_count_f;
        sample_remainder = sample_count_f - (float) sample_count;

        hmp_mdi->events[hmp_mdi->event_count - 1].samples_to_next += sample_count;
        hmp_mdi->extra_info.approx_total_samples += sample_count;

        // DEBUG
        // fprintf(stderr,"DEBUG: Sample Count %u\r\n",sample_count);
    }

    if ((hmp_mdi->reverb = _WM_init_reverb(_WM_SampleRate, _WM_reverb_room_width, _WM_reverb_room_length, _WM_reverb_listen_posx, _WM_reverb_listen_posy)) == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to init reverb", 0);
        goto _hmp_end;
    }

    hmp_mdi->extra_info.current_sample = 0;
    hmp_mdi->current_event = &hmp_mdi->events[0];
    hmp_mdi->samples_to_mix = 0;
    hmp_mdi->note = NULL;

    _WM_ResetToStart(hmp_mdi);

_hmp_end:
    free(hmp_chunk);
    free(chunk_length);
    free(chunk_delta);
    free(chunk_ofs);
    free(chunk_end);
    if (hmp_mdi->reverb) return (hmp_mdi);
    _WM_freeMDI(hmp_mdi);
    return NULL;
}
