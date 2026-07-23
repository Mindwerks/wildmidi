/*
 * mafm.c -- Yamaha MA-series FM rendering for SMAF files.
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
 *
 * The WildMIDI-side wrapper around the FM synth core in src/mafm/.  It parses a
 * SMAF file's custom instrument bank (Mtsu voice-exclusives) and drives the FM
 * core as a per-mdi synth instance, mirroring sf2.c.  See docs/SMAF_FM.md.
 */

#include "config.h"

#ifndef WILDMIDI_MAFM

typedef char mafm_char20[20]; /* no empty source. */

#else

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "mafm.h"
#include "mafm/ma_fm_core.h"
#include "mafm/smaf_voice.h"
#include "mafm/yamaha_adpcm.h"

/* big-endian 32-bit read */
#define MAFM_BE32(p) (((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | \
                      ((uint32_t)(p)[2] << 8)  |  (uint32_t)(p)[3])

#define MAFM_MAX_BANK   64      /* custom voices we keep from one file */
#define MAFM_POLYPHONY  32      /* simultaneously sounding FM voices */
#define MAFM_MAX_WAVES  32      /* decoded ADPCM waves kept from one file */
#define MAFM_PCM_POOL   16      /* simultaneously sounding sampled voices */
#define MAFM_MAX_TRIGS  1024    /* scheduled ATR wave hits */

/* One entry in the file's custom voice bank. */
struct mafm_bank_entry {
    int bank;                        /* SMAF bank (from the voice key) */
    int pc;                          /* program number */
    struct mafm_voice_patch patch;
};

/* A decoded ADPCM wave (Awa) from the ATR audio track. */
struct mafm_wave {
    int16_t *pcm;
    uint32_t len;                    /* samples */
    int fs;                          /* native sample rate (Hz) */
};

/* A sampled voice playing back a wave. */
struct mafm_pcm_voice {
    const int16_t *pcm;
    uint32_t len;
    double pos;                      /* fractional read position (samples) */
    double step;                     /* native_fs / output_rate */
    float  gain;
    int    active;
};

/* One scheduled ATR wave trigger. */
struct mafm_trigger {
    uint32_t at_sample;              /* absolute output-sample time */
    int      wave;                   /* wave number */
};

struct mafm_synth {
    double rate;

    struct mafm_bank_entry bank[MAFM_MAX_BANK];
    int bank_count;

    /* ADPCM wave bank + scheduled ATR triggers */
    struct mafm_wave waves[MAFM_MAX_WAVES];
    int wave_count;
    struct mafm_trigger trigs[MAFM_MAX_TRIGS];
    int trig_count;
    int trig_next;                   /* index of the next pending trigger */
    uint32_t cursor;                 /* output samples rendered so far */
    struct mafm_pcm_voice pcm[MAFM_PCM_POOL];

    /* per-MIDI-channel selection state (16 channels) */
    uint8_t chan_bank[16];
    uint8_t chan_program[16];
    float   chan_volume[16];
    int     chan_pitch[16];          /* 14-bit pitch wheel, centred 0x2000 */

    struct mafm_voice voices[MAFM_POLYPHONY];
};

/* ------------------------------------------------------------------------- */
/* Parse the file's Mtsu voice-exclusives into the bank.                     */

/* Walk one Mtsu chunk body, decoding each "ff f0 <len> 43 ... f7" exclusive. */
static void mafm_parse_mtsu(struct mafm_synth *s, const uint8_t *body,
                            uint32_t n) {
    uint32_t p = 0;
    while (p + 3 <= n) {
        if (body[p] == 0xff && body[p + 1] == 0xf0) {
            uint8_t len = body[p + 2];
            const uint8_t *payload = body + p + 3;
            uint32_t plen = len;
            struct mafm_parsed_voice pv;
            if ((uint32_t)p + 3 + len > n) break;
            if (plen > 0 && payload[plen - 1] == 0xf7) plen--;
            _WM_MAFM_ParseVoiceExclusive(payload, plen, &pv);
            if (pv.valid && !pv.is_pcm && s->bank_count < MAFM_MAX_BANK) {
                struct mafm_bank_entry *e = &s->bank[s->bank_count++];
                e->bank = pv.key.bank_lsb;
                e->pc = pv.key.pc;
                e->patch = pv.patch;
            }
            p += 3 + len;
        } else {
            p++;
        }
    }
}

/* Scan the whole SMAF container for Mtsu chunks (inside MTR* score tracks). */
static void mafm_build_bank(struct mafm_synth *s, const uint8_t *in,
                            uint32_t insize) {
    uint32_t pos = 8, end = insize;
    if (insize >= 8) {
        uint32_t declared = MAFM_BE32(in + 4);
        if ((uint64_t)8 + declared <= insize) end = 8 + declared;
    }
    while (pos + 8 <= end) {
        const uint8_t *c = in + pos;
        uint32_t sz = MAFM_BE32(c + 4);
        uint32_t body = pos + 8;
        if ((uint64_t)body + sz > insize) sz = insize - body;

        if (memcmp(c, "MTR", 3) == 0) {
            /* header width depends on the format byte; scan the inner chunks */
            uint8_t fmt = (sz >= 1) ? c[8] : 0xff;
            uint8_t chlen = (fmt == 0x00) ? 2 : (fmt == 0x03) ? 32 : 16;
            uint32_t hdr = 4 + chlen;
            uint32_t q = body + hdr, tend = body + sz;
            while (q + 8 <= tend) {
                const uint8_t *sc = in + q;
                uint32_t ssz = MAFM_BE32(sc + 4);
                uint32_t sbody = q + 8;
                if ((uint64_t)sbody + ssz > insize) ssz = insize - sbody;
                if (memcmp(sc, "Mtsu", 4) == 0)
                    mafm_parse_mtsu(s, in + sbody, ssz);
                q = sbody + ssz;
                if (ssz == 0) q++;
            }
        }
        pos = body + sz;
        if (sz == 0) pos++;
    }
}

/* ------------------------------------------------------------------------- */
/* ADPCM wave bank + ATR wave-trigger schedule.                              */

/* rate class (fmt2 low nibble) -> Hz */
static int mafm_wave_rate(uint8_t fmt2) {
    switch (fmt2 & 0x0f) {
    case 0: return 4000;
    case 1: return 8000;
    case 2: return 11025;
    case 3: return 22050;
    case 4: return 44100;
    default: return 8000;
    }
}

/* Decode one Awa wave body ([formatByte][fmt2][adpcm...]) into the bank. */
static void mafm_add_wave(struct mafm_synth *s, int number,
                          const uint8_t *body, uint32_t sz) {
    struct mafm_wave *w;
    const uint8_t *adpcm;
    uint32_t alen;
    if (s->wave_count >= MAFM_MAX_WAVES || number < 0 || number >= MAFM_MAX_WAVES)
        return;
    if (sz < 2) return;
    adpcm = body + 2;
    alen = sz - 2;
    w = &s->waves[number];
    if (w->pcm) return;              /* already have this wave number */
    w->pcm = (int16_t *) malloc((size_t)alen * 2 * sizeof(int16_t));
    if (!w->pcm) return;
    w->len = _WM_MAFM_AdpcmDecodeAll(adpcm, alen, 0 /* low-nibble-first */, w->pcm);
    w->fs = mafm_wave_rate(body[1]);
    if (number + 1 > s->wave_count) s->wave_count = number + 1;
}

/* HandyPhone VLQ (1-or-2 byte), as in smaf2mid.c / the ATR sequence. */
static uint32_t mafm_hps_vlq(const uint8_t *seq, uint32_t *pp, uint32_t end) {
    uint32_t p = *pp;
    uint8_t b;
    uint32_t val;
    if (p >= end) { *pp = end; return 0; }
    b = seq[p++];
    if (b & 0x80) {
        if (p >= end) { *pp = end; return 0; }
        val = (((uint32_t)(b & 0x7f) + 1) << 7) | seq[p++];
    } else {
        val = b;
    }
    *pp = p;
    return val;
}

/* Decode an ATR Atsq sequence: like a HandyPhone stream, but a note event's
 * lead byte carries the wave number to trigger.  Schedule each hit on the
 * synth's output-sample clock.  ms/tick comes from the ATR timebase. */
static void mafm_decode_atsq(struct mafm_synth *s, const uint8_t *seq,
                             uint32_t seqlen, uint32_t ms_dur) {
    uint32_t p = 0, cur_ms = 0;
    while (p < seqlen && s->trig_count < MAFM_MAX_TRIGS) {
        uint32_t dur;
        uint8_t e1;
        dur = mafm_hps_vlq(seq, &p, seqlen);
        cur_ms += dur * ms_dur;
        if (p >= seqlen) break;
        e1 = seq[p++];
        if (e1 == 0xff) {                       /* meta / exclusive */
            uint8_t e2;
            if (p >= seqlen) break;
            e2 = seq[p++];
            if (e2 == 0x00) continue;
            if (e2 == 0xf0) {                   /* exclusive: len + payload */
                uint32_t len = mafm_hps_vlq(seq, &p, seqlen);
                if ((uint64_t)p + len > seqlen) break;
                p += len;
            } else {                            /* meta: len byte + payload */
                uint8_t len;
                if (p >= seqlen) break;
                len = seq[p++];
                if ((uint32_t)p + len > seqlen) break;
                p += len;
            }
        } else if (e1 == 0x00) {                /* control event */
            uint8_t e2;
            if (p >= seqlen) break;
            e2 = seq[p++];
            if (e2 == 0x00) {                   /* 00 00 xx */
                if (p >= seqlen) break;
                if (seq[p++] == 0x00) break;    /* end of sequence */
            } else if (((e2 >> 4) & 3) == 3) {  /* long control: skip value */
                if (p >= seqlen) break;
                p++;
            }
        } else {                                /* note: e1 = wave number + gate */
            uint8_t wave = e1 & 0x3f;
            uint32_t gate = mafm_hps_vlq(seq, &p, seqlen);
            (void) gate;
            if (s->trig_count < MAFM_MAX_TRIGS) {
                struct mafm_trigger *t = &s->trigs[s->trig_count++];
                t->at_sample = (uint32_t)((double)cur_ms * s->rate / 1000.0);
                t->wave = wave;
            }
        }
    }
}

/* Scan the container's ATR audio track(s) for the Awa waves + Atsq triggers. */
static void mafm_build_waves(struct mafm_synth *s, const uint8_t *in,
                             uint32_t insize) {
    uint32_t pos = 8, end = insize;
    if (insize >= 8) {
        uint32_t declared = MAFM_BE32(in + 4);
        if ((uint64_t)8 + declared <= insize) end = 8 + declared;
    }
    while (pos + 8 <= end) {
        const uint8_t *c = in + pos;
        uint32_t sz = MAFM_BE32(c + 4);
        uint32_t body = pos + 8;
        if ((uint64_t)body + sz > insize) sz = insize - body;

        if (memcmp(c, "ATR", 3) == 0 && sz >= 6) {
            /* ATR header: format_type, sequence_type, timebase_dur,
             * timebase_gate, wave_type ... whose width varies.  Rather than
             * assume it, scan for the known sub-chunk ids (AspI / Atsq / Awa*)
             * and process each; advance a byte at a time when the id is
             * unrecognised. */
            uint32_t q = body, tend = body + sz;
            /* The ATR Atsq shares the score's tick base; the sample corpus uses
             * 4 ms/tick (timebase code 0x02).  The ATR header's own timebase
             * field placement varies by wave-type, so default to 4 ms rather
             * than risk misreading it (a wrong value halves/doubles every hit). */
            uint32_t ms_dur = 4;
            while (q + 8 <= tend) {
                const uint8_t *sc = in + q;
                uint32_t ssz = MAFM_BE32(sc + 4);
                uint32_t sbody = q + 8;
                int matched = 0;
                if ((uint64_t)sbody + ssz <= insize && sbody + ssz <= tend) {
                    if (memcmp(sc, "Awa", 3) == 0) {
                        mafm_add_wave(s, sc[3], in + sbody, ssz);
                        matched = 1;
                    } else if (memcmp(sc, "Atsq", 4) == 0) {
                        mafm_decode_atsq(s, in + sbody, ssz, ms_dur);
                        matched = 1;
                    } else if (memcmp(sc, "AspI", 4) == 0) {
                        matched = 1;    /* setup info: skip by size */
                    }
                }
                if (matched) {
                    q = sbody + ssz;
                    if (ssz == 0) q++;
                } else {
                    q++;                /* header byte: step past it */
                }
            }
        }
        pos = body + sz;
        if (sz == 0) pos++;
    }
}

/* ------------------------------------------------------------------------- */

int _WM_MAFM_HasCustomVoices(const uint8_t *smaf, uint32_t size) {
    struct mafm_synth tmp;
    if (size < 8 || memcmp(smaf, "MMMD", 4) != 0) return 0;
    memset(&tmp, 0, sizeof(tmp));
    mafm_build_bank(&tmp, smaf, size);
    return tmp.bank_count > 0;
}

/* Resolve a channel's (bank, program) to a bank patch, or fall back to the
 * GM/drum approximation. */
static void mafm_select_patch(struct mafm_synth *s, int ch, int is_drum,
                              struct mafm_voice_patch *out) {
    int i;
    int bank = s->chan_bank[ch];
    int pc = s->chan_program[ch];
    for (i = 0; i < s->bank_count; i++) {
        if (s->bank[i].pc == pc &&
            (s->bank[i].bank == bank || s->bank_count <= 4)) {
            /* bank match, or a tiny bank where the pc alone disambiguates */
            *out = s->bank[i].patch;
            return;
        }
    }
    if (is_drum) _WM_MAFM_DrumApprox(pc, out);
    else         _WM_MAFM_GmApprox(pc, out);
}

static double mafm_note_hz(int midi_note, int pitch14) {
    /* pitch14 centred at 0x2000; +/- 2 semitones default range */
    double bend = ((double)(pitch14 - 0x2000) / 8192.0) * 2.0;
    return 440.0 * pow(2.0, ((double)midi_note - 69.0 + bend) / 12.0);
}

/* Find a free (or steal the quietest) voice slot. */
static struct mafm_voice *mafm_alloc_voice(struct mafm_synth *s) {
    int i, steal = 0;
    for (i = 0; i < MAFM_POLYPHONY; i++)
        if (!_WM_MAFM_VoiceActive(&s->voices[i])) return &s->voices[i];
    /* all busy: steal slot 0 (simple; ringtones rarely exceed the pool) */
    return &s->voices[steal];
}

void *_WM_MAFM_NewSynth(const uint8_t *smaf, uint32_t size, uint16_t rate) {
    struct mafm_synth *s;
    int i;
    s = (struct mafm_synth *) calloc(1, sizeof(struct mafm_synth));
    if (!s) return NULL;
    s->rate = rate ? (double) rate : 44100.0;
    for (i = 0; i < 16; i++) {
        s->chan_bank[i] = 0;
        s->chan_program[i] = 0;
        s->chan_volume[i] = 1.0f;
        s->chan_pitch[i] = 0x2000;
    }
    for (i = 0; i < MAFM_POLYPHONY; i++)
        _WM_MAFM_VoiceInit(&s->voices[i], s->rate);
    mafm_build_bank(s, smaf, size);
    mafm_build_waves(s, smaf, size);
    return s;
}

void _WM_MAFM_FreeSynth(void *synth) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    int i;
    if (!s) return;
    for (i = 0; i < MAFM_MAX_WAVES; i++)
        free(s->waves[i].pcm);
    free(s);
}

void _WM_MAFM_Reset(void *synth) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    int i;
    for (i = 0; i < MAFM_POLYPHONY; i++)
        _WM_MAFM_VoiceInit(&s->voices[i], s->rate);
    for (i = 0; i < MAFM_PCM_POOL; i++)
        s->pcm[i].active = 0;
    s->cursor = 0;
    s->trig_next = 0;
    for (i = 0; i < 16; i++) {
        s->chan_bank[i] = 0;
        s->chan_program[i] = 0;
        s->chan_volume[i] = 1.0f;
        s->chan_pitch[i] = 0x2000;
    }
}

int _WM_MAFM_ActiveVoices(void *synth) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    int i, n = 0;
    for (i = 0; i < MAFM_POLYPHONY; i++)
        if (_WM_MAFM_VoiceActive(&s->voices[i])) n++;
    for (i = 0; i < MAFM_PCM_POOL; i++)
        if (s->pcm[i].active) n++;
    /* Note: pending triggers are NOT counted as active.  They fire while the
     * song's event list is still playing; using them to hold the synth alive
     * past the end-of-track could spin forever if the cursor never reaches a
     * late trigger.  A currently-sounding sample (above) is what rings out. */
    return n;
}

/* Start a sampled wave playing on a free PCM slot (drums are one-shot). */
static void mafm_start_pcm(struct mafm_synth *s, int wave) {
    struct mafm_wave *w;
    struct mafm_pcm_voice *pv = NULL;
    int i;
    if (wave < 0 || wave >= s->wave_count) return;
    w = &s->waves[wave];
    if (!w->pcm || w->len == 0) return;
    for (i = 0; i < MAFM_PCM_POOL; i++)
        if (!s->pcm[i].active) { pv = &s->pcm[i]; break; }
    if (!pv) pv = &s->pcm[0];       /* steal slot 0 if the pool is full */
    pv->pcm = w->pcm;
    pv->len = w->len;
    pv->pos = 0.0;
    pv->step = (double) w->fs / s->rate;
    pv->gain = 1.0f;
    pv->active = 1;
}

/* Advance the ATR trigger schedule and render one PCM sample (summed mono). */
static double mafm_pcm_tick(struct mafm_synth *s) {
    double mix = 0.0;
    int i;
    /* fire any triggers whose time has arrived */
    while (s->trig_next < s->trig_count &&
           s->trigs[s->trig_next].at_sample <= s->cursor) {
        mafm_start_pcm(s, s->trigs[s->trig_next].wave);
        s->trig_next++;
    }
    for (i = 0; i < MAFM_PCM_POOL; i++) {
        struct mafm_pcm_voice *pv = &s->pcm[i];
        uint32_t idx;
        if (!pv->active) continue;
        idx = (uint32_t) pv->pos;
        if (idx >= pv->len) { pv->active = 0; continue; }
        mix += (double) pv->pcm[idx] / 32768.0 * pv->gain;
        pv->pos += pv->step;
    }
    s->cursor++;
    return mix;
}

static void mafm_note_on(struct mafm_synth *s, int ch, int note, int vel) {
    struct mafm_voice_patch patch;
    struct mafm_voice *v;
    int is_drum = (ch == 9);
    mafm_select_patch(s, ch, is_drum, &patch);
    v = mafm_alloc_voice(s);
    _WM_MAFM_VoiceSetVolume(v, s->chan_volume[ch]);
    _WM_MAFM_VoiceNoteOn(v, &patch,
                         mafm_note_hz(note, s->chan_pitch[ch]),
                         (float) vel / 127.0f);
    v->channel = ch;
    v->note = note;
}

static void mafm_note_off(struct mafm_synth *s, int ch, int note) {
    int i;
    for (i = 0; i < MAFM_POLYPHONY; i++) {
        struct mafm_voice *v = &s->voices[i];
        if (_WM_MAFM_VoiceActive(v) && v->channel == ch && v->note == note)
            _WM_MAFM_VoiceNoteOff(v);
    }
}

void _WM_MAFM_Event(void *synth, struct _mdi *mdi, struct _event *event) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    uint8_t ch = event->event_data.channel;
    uint32_t val = event->event_data.data.value;
    (void) mdi;
    if (ch > 15) return;

    switch (event->evtype) {
    case ev_note_on:
        if ((val & 0xFF) == 0)
            mafm_note_off(s, ch, (val >> 8) & 0x7F);
        else
            mafm_note_on(s, ch, (val >> 8) & 0x7F, val & 0x7F);
        break;
    case ev_note_off:
        mafm_note_off(s, ch, (val >> 8) & 0x7F);
        break;
    case ev_patch:
        s->chan_program[ch] = val & 0x7F;
        break;
    case ev_control_bank_select:
        s->chan_bank[ch] = val & 0x7F;
        break;
    case ev_pitch: {
        int i;
        s->chan_pitch[ch] = val & 0x3FFF;
        for (i = 0; i < MAFM_POLYPHONY; i++) {
            struct mafm_voice *v = &s->voices[i];
            if (_WM_MAFM_VoiceActive(v) && v->channel == ch)
                _WM_MAFM_VoiceSetPitch(v, mafm_note_hz(v->note, s->chan_pitch[ch]));
        }
    } break;
    case ev_control_channel_volume:
        s->chan_volume[ch] = (float)(val & 0x7F) / 127.0f;
        break;
    default:
        break;
    }
}

void _WM_MAFM_Render(void *synth, int32_t *out, uint32_t frames) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    uint32_t f, i;
    for (f = 0; f < frames; f++) {
        double fm = 0.0, pcm;
        int32_t v;
        for (i = 0; i < MAFM_POLYPHONY; i++)
            if (_WM_MAFM_VoiceActive(&s->voices[i]))
                fm += _WM_MAFM_VoiceTick(&s->voices[i]);
        /* ATR sampled drums/phrases play alongside the FM voices, on the
         * synth's own sample clock (advanced once per output frame). */
        pcm = mafm_pcm_tick(s);
        v = (int32_t)(fm * 8000.0 + pcm * 20000.0);  /* samples carry more level */
        out[f * 2]     += v;                 /* mono -> both channels */
        out[f * 2 + 1] += v;
    }
}

#endif /* WILDMIDI_MAFM */
