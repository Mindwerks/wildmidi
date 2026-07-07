/*
 * sf2.c -- SoundFont2 rendering via TinySoundFont
 *
 * Copyright (C) WildMIDI Developers 2026
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

#ifndef WILDMIDI_SF2

typedef char tsf_char20[20]; /* no empty source. */

#else

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TSF_POW pow
#define TSF_LOG log
#define TSF_TAN tan
#define TSF_LOG10 log10

#ifdef HAVE_POWF
#define TSF_POWF powf
#else
#define TSF_POWF(x,y) (float)pow((x),(y))
#endif
#ifdef HAVE_EXPF
#define TSF_EXPF expf
#else
#define TSF_EXPF(x) (float)exp((x))
#endif
#ifdef HAVE_SQRTF
#define TSF_SQRTF sqrtf
#else
#define TSF_SQRTF(x) (float)sqrt((x))
#endif

#define TSF_STATIC /* keep tsf symbols out of the library's export table */
#define TSF_IMPLEMENTATION
#include "tsf/tsf.h"

#include "common.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "lock.h"
#include "sf2.h"

static tsf *WM_sf2 = NULL;
static int WM_sf2_lock = 0;

int _WM_SF2_Magic(const uint8_t *data, uint32_t size) {
    return (size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "sfbk", 4));
}

int _WM_SF2_Load(const uint8_t *data, uint32_t size) {
    tsf *f;
    if (size > (uint32_t)INT_MAX) { /* tsf_load_memory takes int; refuse to wrap negative */
        return (-1);
    }
    f = tsf_load_memory(data, (int)size);
    if (f == NULL) {
        return (-1);
    }
    _WM_Lock(&WM_sf2_lock);
    if (WM_sf2) {
        tsf_close(WM_sf2); /* a later soundfont line replaces an earlier one */
    }
    WM_sf2 = f;
    _WM_Unlock(&WM_sf2_lock);
    return (0);
}

void _WM_SF2_Unload(void) {
    _WM_Lock(&WM_sf2_lock);
    if (WM_sf2) {
        tsf_close(WM_sf2);
        WM_sf2 = NULL;
    }
    _WM_Unlock(&WM_sf2_lock);
}

int _WM_SF2_Active(void) {
    return (WM_sf2 != NULL);
}

static void WM_SF2_InitChannels(tsf *f) {
    int ch;
    for (ch = 0; ch < 16; ch++) {
        tsf_channel_set_bank_preset(f, ch, (ch == 9) ? 128 : 0, 0);
    }
}

void *_WM_SF2_NewSynth(uint16_t rate) {
    tsf *f;
    _WM_Lock(&WM_sf2_lock);
    f = WM_sf2 ? tsf_copy(WM_sf2) : NULL;
    _WM_Unlock(&WM_sf2_lock);
    if (f == NULL) {
        return NULL;
    }
    tsf_set_output(f, TSF_STEREO_INTERLEAVED, rate, 0.0f);
    WM_SF2_InitChannels(f);
    return f;
}

void _WM_SF2_FreeSynth(void *synth) {
    if (synth) {
        tsf_close((tsf *)synth);
    }
}

void _WM_SF2_Reset(void *synth) {
    tsf *f = (tsf *)synth;
    int ch;
    tsf_reset(f);
    for (ch = 0; ch < 16; ch++) {
        tsf_channel_midi_control(f, ch, 121, 0); /* reset controllers */
    }
    WM_SF2_InitChannels(f);
}

int _WM_SF2_ActiveVoices(void *synth) {
    return tsf_active_voice_count((tsf *)synth);
}

void _WM_SF2_Event(void *synth, struct _mdi *mdi, struct _event *event) {
    tsf *f = (tsf *)synth;
    uint8_t ch = event->event_data.channel;
    uint32_t val = event->event_data.data.value;

    switch (event->evtype) {
    case ev_note_on:
        if ((val & 0xFF) == 0) { /* velocity 0 == note off */
            tsf_channel_note_off(f, ch, (val >> 8) & 0x7F);
        } else {
            tsf_channel_note_on(f, ch, (val >> 8) & 0x7F, (float)(val & 0x7F) / 127.0f);
        }
        break;
    case ev_note_off:
        tsf_channel_note_off(f, ch, (val >> 8) & 0x7F);
        break;
    case ev_patch:
        tsf_channel_set_presetnumber(f, ch, val & 0x7F, mdi->channel[ch].isdrum);
        break;
    case ev_pitch:
        tsf_channel_set_pitchwheel(f, ch, val & 0x3FFF);
        break;
    /* controllers: tsf_channel_midi_control() implements the full CC map,
       so we just translate the event type back to its controller number */
    case ev_control_bank_select:
        if (!mdi->channel[ch].isdrum) /* drum kits select via patch, not bank */
            tsf_channel_midi_control(f, ch, 0, val & 0x7F);
        break;
    case ev_control_data_entry_course:
        tsf_channel_midi_control(f, ch, 6, val & 0x7F);
        break;
    case ev_control_channel_volume:
        tsf_channel_midi_control(f, ch, 7, val & 0x7F);
        break;
    case ev_control_channel_balance:
        tsf_channel_midi_control(f, ch, 8, val & 0x7F);
        break;
    case ev_control_channel_pan:
        tsf_channel_midi_control(f, ch, 10, val & 0x7F);
        break;
    case ev_control_channel_expression:
        tsf_channel_midi_control(f, ch, 11, val & 0x7F);
        break;
    case ev_control_data_entry_fine:
        tsf_channel_midi_control(f, ch, 38, val & 0x7F);
        break;
    case ev_control_channel_hold:
        tsf_channel_midi_control(f, ch, 64, val & 0x7F);
        break;
    case ev_control_non_registered_param_fine:
        tsf_channel_midi_control(f, ch, 98, val & 0x7F);
        break;
    case ev_control_non_registered_param_course:
        tsf_channel_midi_control(f, ch, 99, val & 0x7F);
        break;
    case ev_control_registered_param_fine:
        tsf_channel_midi_control(f, ch, 100, val & 0x7F);
        break;
    case ev_control_registered_param_course:
        tsf_channel_midi_control(f, ch, 101, val & 0x7F);
        break;
    case ev_control_channel_sound_off:
        tsf_channel_midi_control(f, ch, 120, val & 0x7F);
        break;
    case ev_control_channel_controllers_off:
        tsf_channel_midi_control(f, ch, 121, val & 0x7F);
        break;
    case ev_control_channel_notes_off:
        tsf_channel_midi_control(f, ch, 123, val & 0x7F);
        break;
    case ev_control_dummy: /* unhandled CCs are stored as (controller << 8) | value */
        tsf_channel_midi_control(f, ch, (val >> 8) & 0x7F, val & 0x7F);
        break;
    case ev_sysex_roland_drum_track:
        tsf_channel_set_bank_preset(f, ch, val ? 128 : 0, 0);
        break;
    case ev_sysex_gm_reset:
    case ev_sysex_roland_reset:
    case ev_sysex_yamaha_reset:
        _WM_SF2_Reset(f);
        break;
    default: /* meta/timing events don't reach the synth */
        break;
    }
}

void _WM_SF2_Render(void *synth, int32_t *out, uint32_t frames) {
    tsf *f = (tsf *)synth;
    short buf[256 * 2];
    uint32_t n, i;

    while (frames) {
        n = (frames > 256) ? 256 : frames;
        tsf_render_short(f, buf, (int)n, 0);
        for (i = 0; i < n * 2; i++) {
            out[i] += buf[i];
        }
        out += n * 2;
        frames -= n;
    }
}
#endif /* WILDMIDI_SF2 */
