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
int _WM_sf2_lock = 0;

/* A per-mdi synth is a tsf plus the note offs we are holding back.  tsf
 * releases a voice from wherever its amplitude envelope has got to, so a note
 * switched off while still in its attack never becomes audible at all.  XMI
 * scores really do carry zero-length notes - the descending triplet at 38s in
 * SUNNYDAY.XMI is three of them - and the GUS mixer keeps those by deferring
 * the release until the first envelope stage has finished (see the env == 0
 * branch of _WM_do_note_off).  Park the note off here and re-issue it from
 * the render loop once the voice has left its attack. */
struct wm_sf2_synth {
    tsf *f;
    uint8_t held_off[16][128]; /* deferred offs per channel/key, not a flag:
                                  a key retriggered while held has a voice per
                                  note on, and each one needs its own off */
    int held_count;
    uint32_t silent_frames; /* consecutive rendered frames that stayed inaudible */
};

/* How long the render has to stay inaudible before the tail counts as over.
 * It just has to be longer than a waveform's own zero crossings: 2048 frames
 * is 46ms even at 44.1kHz. */
#define SF2_SILENCE_FRAMES 2048

/* What counts as inaudible, in 16bit output counts.  A release that has decayed
 * this far is 60dB below full scale and another 20dB below anything else the
 * score is doing, so waiting for it to reach the last bit only buys seconds of
 * dead air: GeneralUser GS runs ~4s past the last note of SUNNYDAY.XMI at 1
 * count, ~2s at 32.  Only consulted once the event list is exhausted, so it
 * cannot cut a quiet passage short mid-score. */
#define SF2_SILENCE_LEVEL 32

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
    _WM_Lock(&_WM_sf2_lock);
    if (WM_sf2) {
        tsf_close(WM_sf2); /* a later soundfont line replaces an earlier one */
    }
    WM_sf2 = f;
    _WM_Unlock(&_WM_sf2_lock);
    return (0);
}

void _WM_SF2_Unload(void) {
    _WM_Lock(&_WM_sf2_lock);
    if (WM_sf2) {
        tsf_close(WM_sf2);
        WM_sf2 = NULL;
    }
    _WM_Unlock(&_WM_sf2_lock);
}

int _WM_SF2_Active(void) {
    return (WM_sf2 != NULL);
}

/* Channel volume, using wildmidi's own curves rather than tsf's cubic
 * default, so WM_MO_LOG_VOLUME does the same thing here as it does for the
 * GUS mixer.  The linear curve is _WM_lin_volume[v]/1024 == v/127; the log
 * curve is the MIDI2 table dBm_volume[v] == 40*log10(v/127), whose gain
 * 10^(dBm/20) is just (v/127) squared. */
static void WM_SF2_ChannelVolume(tsf *f, struct _mdi *mdi, uint8_t ch,
                                 int volume, int expression) {
    float gain = (float)((volume * expression) / 127) / 127.0f;
    if (mdi->extra_info.mixer_options & WM_MO_LOG_VOLUME) {
        gain *= gain;
    }
    tsf_channel_set_volume(f, ch, gain);
}

static void WM_SF2_InitChannels(tsf *f) {
    int ch;
    for (ch = 0; ch < 16; ch++) {
        tsf_channel_set_bank_preset(f, ch, (ch == 9) ? 128 : 0, 0);
    }
}

/* (Re)apply every channel's volume from the mdi's own state.  Needed after a
 * reset and whenever WM_MO_LOG_VOLUME is toggled mid-playback. */
void _WM_SF2_AdjustChannelVolumes(struct _mdi *mdi) {
    uint8_t ch;
    if (mdi->sf2_synth == NULL) return;
    for (ch = 0; ch < 16; ch++) {
        WM_SF2_ChannelVolume(((struct wm_sf2_synth *)mdi->sf2_synth)->f, mdi, ch,
                             mdi->channel[ch].volume, mdi->channel[ch].expression);
    }
}

void *_WM_SF2_NewSynth(uint16_t rate) {
    struct wm_sf2_synth *s;
    tsf *f;
    _WM_Lock(&_WM_sf2_lock);
    f = WM_sf2 ? tsf_copy(WM_sf2) : NULL;
    _WM_Unlock(&_WM_sf2_lock);
    if (f == NULL) {
        return NULL;
    }
    s = (struct wm_sf2_synth *) calloc(1, sizeof(struct wm_sf2_synth));
    if (s == NULL) {
        tsf_close(f);
        return NULL;
    }
    s->f = f;
    tsf_set_output(f, TSF_STEREO_INTERLEAVED, rate, 0.0f);
    WM_SF2_InitChannels(f);
    return s;
}

void _WM_SF2_FreeSynth(void *synth) {
    if (synth) {
        tsf_close(((struct wm_sf2_synth *)synth)->f);
        free(synth);
    }
}

void _WM_SF2_Reset(struct _mdi *mdi) {
    struct wm_sf2_synth *s = (struct wm_sf2_synth *)mdi->sf2_synth;
    tsf *f;
    int ch;
    if (s == NULL) return;
    f = s->f;
    memset(s->held_off, 0, sizeof(s->held_off));
    s->held_count = 0;
    s->silent_frames = 0;
    tsf_reset(f);
    for (ch = 0; ch < 16; ch++) {
        tsf_channel_midi_control(f, ch, 121, 0); /* reset controllers */
    }
    WM_SF2_InitChannels(f);
    /* Seed the gains from _WM_do_sysex_gm_reset()'s own defaults rather than
       from mdi->channel[], which makes this independent of when the caller
       resets the mdi: WM_GetOutput_SF2()'s loop path resets it just after,
       FastSeek/SongSeek just before, and the GM/GS/XG sysex path runs before
       do_event() has applied the reset at all.  All four land on the same
       volume 100 / expression 127 either way. */
    for (ch = 0; ch < 16; ch++) {
        WM_SF2_ChannelVolume(f, mdi, (uint8_t)ch, 100, 127);
    }
}

void _WM_SF2_ReleaseAll(void *synth) {
    struct wm_sf2_synth *s = (struct wm_sf2_synth *)synth;
    memset(s->held_off, 0, sizeof(s->held_off)); /* superseded by the release */
    s->held_count = 0;
    tsf_note_off_all(s->f);
}

/* Would a note off now silence the voice it lands on?  tsf_channel_note_off()
 * releases the sounding voice with the lowest playIndex - the oldest note on
 * the key - so that is the only envelope that matters here.  Asking whether
 * *any* matching voice is in attack would hold an older voice that is long
 * past its own attack for the whole of a newer overlapping note's. */
static int WM_SF2_InAttack(tsf *f, int ch, int key) {
    struct tsf_voice *v = f->voices, *vEnd = v ? v + f->voiceNum : TSF_NULL;
    struct tsf_voice *oldest = TSF_NULL;
    for (; v != vEnd; v++) {
        /* the same filter tsf_channel_note_off() applies when it picks a voice */
        if (v->playingPreset == -1 || v->playingChannel != ch || v->playingKey != key
            || v->ampenv.segment >= TSF_SEGMENT_RELEASE || v->heldSustain) continue;
        if (!oldest || v->playIndex < oldest->playIndex) oldest = v;
    }
    /* nothing to release: let the off through, tsf makes it a no-op */
    return (oldest != TSF_NULL && oldest->ampenv.segment <= TSF_SEGMENT_ATTACK);
}

static void WM_SF2_NoteOff(struct wm_sf2_synth *s, int ch, int key) {
    if (WM_SF2_InAttack(s->f, ch, key)) {
        if (s->held_off[ch][key] < 0xFF) { /* a stuck key cannot wrap the count */
            s->held_off[ch][key]++;
            s->held_count++;
        }
        return;
    }
    tsf_channel_note_off(s->f, ch, key);
}

/* Re-issue the note offs whose voices have now left their attack.  tsf picks
 * the oldest voice for the key, which is the one the held off belongs to; if
 * the voice is gone the call is a no-op and the entry just clears.  Each off
 * uncovers the next voice down, which may still be in its own attack, so the
 * check has to be repeated rather than draining the whole count at once. */
static void WM_SF2_FlushHeldOffs(struct wm_sf2_synth *s) {
    int ch, key;
    for (ch = 0; ch < 16 && s->held_count; ch++) {
        for (key = 0; key < 128 && s->held_count; key++) {
            while (s->held_off[ch][key] && !WM_SF2_InAttack(s->f, ch, key)) {
                s->held_off[ch][key]--;
                s->held_count--;
                tsf_channel_note_off(s->f, ch, key);
            }
        }
    }
}

/* tsf holds a voice open for its whole nominal release time, which on a
 * soundfont with long releases runs on for seconds after the envelope has
 * decayed out of 16bit range - dead air on the end of the render.  What has
 * actually come out of the mixer settles that better than any envelope
 * threshold can, so a run of silent frames ends the tail. */
int _WM_SF2_ActiveVoices(void *synth) {
    struct wm_sf2_synth *s = (struct wm_sf2_synth *)synth;
    if (s->silent_frames >= SF2_SILENCE_FRAMES) return 0;
    return tsf_active_voice_count(s->f);
}

void _WM_SF2_Event(void *synth, struct _mdi *mdi, struct _event *event) {
    struct wm_sf2_synth *s = (struct wm_sf2_synth *)synth;
    tsf *f = s->f;
    uint8_t ch = event->event_data.channel & 0x0F; /* held_off[] is indexed by it */
    uint32_t val = event->event_data.data.value;

    switch (event->evtype) {
    case ev_note_on:
        if ((val & 0xFF) == 0) { /* velocity 0 == note off */
            WM_SF2_NoteOff(s, ch, (val >> 8) & 0x7F);
        } else {
            uint8_t key = (val >> 8) & 0x7F;
            if (s->held_off[ch][key]) { /* retrigger: let the oldest voice go
                                           first, since waiting for the new
                                           note's attack would hold it on */
                s->held_off[ch][key]--;
                s->held_count--;
                tsf_channel_note_off(f, ch, key);
            }
            tsf_channel_note_on(f, ch, key, (float)(val & 0x7F) / 127.0f);
        }
        break;
    case ev_note_off:
        WM_SF2_NoteOff(s, ch, (val >> 8) & 0x7F);
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
    case ev_control_channel_modulation:
        tsf_channel_midi_control(f, ch, 1, val & 0x7F);
        break;
    case ev_control_data_entry_course:
        tsf_channel_midi_control(f, ch, 6, val & 0x7F);
        break;
    case ev_control_channel_volume:
        /* do_event() has not run yet, so pass the new value explicitly */
        WM_SF2_ChannelVolume(f, mdi, ch, val & 0x7F, mdi->channel[ch].expression);
        break;
    case ev_control_channel_balance:
        tsf_channel_midi_control(f, ch, 8, val & 0x7F);
        break;
    case ev_control_channel_pan:
        tsf_channel_midi_control(f, ch, 10, val & 0x7F);
        break;
    case ev_control_channel_expression:
        WM_SF2_ChannelVolume(f, mdi, ch, mdi->channel[ch].volume, val & 0x7F);
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
        /* CC121 puts tsf's own volume back to unity; restore ours.  Like
           _WM_do_control_channel_controllers_off(), CC7 survives, CC11 does not. */
        WM_SF2_ChannelVolume(f, mdi, ch, mdi->channel[ch].volume, 127);
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
        _WM_SF2_Reset(mdi);
        break;
    default: /* meta/timing events don't reach the synth */
        break;
    }
}

/* Headroom.  A soundfont renders a single note at full velocity close to full
 * scale, so a busy score summed at unity gain clips hard.  VOL_DIVISOR in
 * internal_midi.c uses 4.0 for the GUS mixer; soundfont material has a higher
 * crest factor than that covers.  Rendering GeneralUser GS's own nine demo
 * scores, the loudest (Jump!) needs 4.78 to stay inside 16 bits and the rest
 * need 1.43 to 3.67, so 5.0 clears the set.  It is also fluidsynth's default
 * gain of 0.2, and it puts SUNNYDAY.XMI within 0.1dB of the eawpats render in
 * RMS - the GUS and SF2 paths should not change loudness under the listener.
 * Denser material than those demos will reach the clamp; turn it down with
 * WildMidi_MasterVolume(). */
#define SF2_VOL_DIVISOR 5.0f

void _WM_SF2_Render(void *synth, int32_t *out, uint32_t frames) {
    struct wm_sf2_synth *s = (struct wm_sf2_synth *)synth;
    tsf *f = s->f;
    float buf[256 * 2];
    /* Render float, not short: tsf_render_short() clamps to int16 itself, so
       scaling its output afterwards would only make the clipping quieter. */
    const float gain = (32767.0f * (float)_WM_MasterVolume / 1024.0f) / SF2_VOL_DIVISOR;
    uint32_t n, i;

    while (frames) {
        int32_t heard = 0;
        n = (frames > 256) ? 256 : frames;
        if (s->held_count) WM_SF2_FlushHeldOffs(s);
        tsf_render_float(f, buf, (int)n, 0);
        for (i = 0; i < n * 2; i++) {
            int32_t v = (int32_t)(buf[i] * gain);
            if (v > SF2_SILENCE_LEVEL || v < -SF2_SILENCE_LEVEL) heard = 1;
            out[i] += v;
        }
        s->silent_frames = heard ? 0 : (s->silent_frames + n);
        out += n * 2;
        frames -= n;
    }
}
#endif /* WILDMIDI_SF2 */
