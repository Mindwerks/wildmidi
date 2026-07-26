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
#include <stdio.h>
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
#define MAFM_VIB_RATE_HZ 5      /* CC1 vibrato LFO rate */
#define MAFM_VIB_BLOCK   64     /* frames between vibrato LFO updates */

/* One entry in the file's custom voice bank. */
struct mafm_bank_entry {
    int bank;                        /* SMAF bank (from the voice key) */
    int pc;                          /* program number */
    int is_pcm;                      /* PCM (sampled) voice; play the wave */
    int wave_id;                     /* pcm.wave_id if is_pcm */
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
    float   chan_expression[16];     /* CC 0x0B; multiplied with volume */
    int     chan_pitch[16];          /* 14-bit pitch wheel, centred 0x2000 */
    uint8_t chan_pan[16];            /* 0..127 pan CC, 64 = centre; 0xff = unset */
    uint8_t chan_modulation[16];     /* CC 1 mod wheel, drives a 5Hz pitch LFO */
    double  vib_phase;               /* shared vibrato LFO phase, 0..1 */
    double  lim_env;                 /* soft peak-limiter state, in mix units */

    struct mafm_voice voices[MAFM_POLYPHONY];
};

/* ------------------------------------------------------------------------- */
/* Parse the file's Mtsu voice-exclusives into the bank.                     */

/* Walk one Mtsu chunk body, decoding each voice-exclusive.  MA-1/2 handyphone
 * files prefix each SysEx with the meta-event byte ("ff f0 <len> 43 ... f7");
 * MA-3/5/6 files store bare "f0 <len> 43 ... f7" instead.  Accept both. */
static void mafm_parse_mtsu(struct mafm_synth *s, const uint8_t *body,
                            uint32_t n) {
    uint32_t p = 0;
    while (p + 2 <= n) {
        uint32_t hdr;
        if (body[p] == 0xff && p + 3 <= n && body[p + 1] == 0xf0)
            hdr = 3;                                /* ff f0 <len> */
        else if (body[p] == 0xf0)
            hdr = 2;                                /* f0 <len> */
        else { p++; continue; }
        {
            uint8_t len = body[p + hdr - 1];
            const uint8_t *payload = body + p + hdr;
            uint32_t plen = len;
            struct mafm_parsed_voice pv;
            if ((uint32_t)p + hdr + len > n) break;
            if (plen > 0 && payload[plen - 1] == 0xf7) plen--;
            _WM_MAFM_ParseVoiceExclusive(payload, plen, &pv);
            if (pv.valid && s->bank_count < MAFM_MAX_BANK) {
                struct mafm_bank_entry *e = &s->bank[s->bank_count++];
                e->bank = pv.key.bank_lsb;
                e->pc = pv.key.pc;
                e->is_pcm = pv.is_pcm;
                e->wave_id = pv.pcm.wave_id;
                e->patch = pv.patch;
            }
            p += hdr + len;
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
    w->pcm = (int16_t *) calloc((size_t)alen, 2 * sizeof(int16_t));
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
    struct mafm_synth *tmp;
    int has;
    if (size < 8 || memcmp(smaf, "MMMD", 4) != 0) return 0;
    tmp = (struct mafm_synth *) calloc(1, sizeof(*tmp));
    if (!tmp) return 0;
    mafm_build_bank(tmp, smaf, size);
    has = tmp->bank_count > 0;
    free(tmp);
    return has;
}

/* Resolve a channel's (bank, program) to a bank patch, or fall back to the
 * GM/drum approximation. */
/* Returns >=0 with the bank index on match (caller inspects s->bank[i].is_pcm
 * to route PCM playback vs FM playback), -1 for a fallback to a built-in
 * approximation (which is filled into *out). */
static int mafm_select_patch(struct mafm_synth *s, int ch, int is_drum,
                             int note, struct mafm_voice_patch *out) {
    int i;
    int bank = s->chan_bank[ch];
    int pc = s->chan_program[ch];
    for (i = 0; i < s->bank_count; i++) {
        if (s->bank[i].pc == pc &&
            (s->bank[i].bank == bank || s->bank_count <= 4)) {
            /* bank match, or a tiny bank where the pc alone disambiguates */
            *out = s->bank[i].patch;
            return i;
        }
    }
    if (is_drum) _WM_MAFM_DrumApprox(note, out);
    else         _WM_MAFM_GmApprox(pc, out);
    return -1;
}

static double mafm_note_hz(int midi_note, int pitch14) {
    /* pitch14 centred at 0x2000; +/- 2 semitones default range */
    double bend = ((double)(pitch14 - 0x2000) / 8192.0) * 2.0;
    return 440.0 * pow(2.0, ((double)midi_note - 69.0 + bend) / 12.0);
}

/* Find a free (or steal the quietest) voice slot. */
static struct mafm_voice *mafm_alloc_voice(struct mafm_synth *s) {
    static int rr = 0;               /* round-robin slot pointer for stealing */
    int i, steal;
    for (i = 0; i < MAFM_POLYPHONY; i++)
        if (!_WM_MAFM_VoiceActive(&s->voices[i])) return &s->voices[i];
    /* All busy: steal round-robin (matches the reference).  Better than
     * always slot 0 because a dense burst spreads the churn across slots
     * instead of thrashing one voice into silence permanently. */
    steal = rr;
    rr = (rr + 1) % MAFM_POLYPHONY;
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
        s->chan_expression[i] = 1.0f;
        s->chan_pitch[i] = 0x2000;
        s->chan_pan[i] = 0xff;       /* sentinel: use patch pan_default */
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
        s->chan_expression[i] = 1.0f;
        s->chan_pitch[i] = 0x2000;
        s->chan_pan[i] = 0xff;       /* sentinel: use patch pan_default */
        s->chan_modulation[i] = 0;
    }
    s->vib_phase = 0.0;
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
    int bank_idx = mafm_select_patch(s, ch, is_drum, note, &patch);
    int sounding_note;
    float vel01, vel_curve;
    /* Matched voice is PCM (a sampled instrument, not FM).  We don't yet play
     * PCM voice-exclusives (see docs/SMAF_FM.md, "Still open" list); play
     * silence instead of falling back to the generic FM approximation, which
     * would be an obviously-wrong voice for the file's own sound design. */
    if (bank_idx >= 0 && s->bank[bank_idx].is_pcm) return;
    /* Apply the patch's own basic-octave transpose (BO field): a voice can
     * declare it plays an octave up/down from the incoming MIDI note. */
    sounding_note = note + patch.note_shift;
    if (sounding_note < 0)   sounding_note = 0;
    if (sounding_note > 127) sounding_note = 127;
    v = mafm_alloc_voice(s);
    /* Volume x expression, matching the reference mixer.  A file that keeps
     * volume at 100/127 and rides expression for dynamics needs both to
     * combine, otherwise the swells never reach the voice. */
    _WM_MAFM_VoiceSetVolume(v, s->chan_volume[ch] * s->chan_expression[ch]);
    /* Squared velocity curve.  A linear map made every mid-velocity note
     * nearly full-scale and constantly pushed the limiter; squaring keeps the
     * musical dynamic range and matches how the chip's own velocity table
     * feels (reference does the same). */
    vel01 = vel ? (float) vel / 127.0f : (100.0f / 127.0f);
    vel_curve = vel01 * vel01;
    _WM_MAFM_VoiceNoteOn(v, &patch,
                         mafm_note_hz(sounding_note, s->chan_pitch[ch]),
                         vel_curve);
    v->channel = ch;
    v->note = note;              /* raw key for note-off matching */
}

static void mafm_note_off(struct mafm_synth *s, int ch, int note) {
    int i;
    /* Release only ONE matching voice, matching the reference.  If the score
     * hits the same (channel, note) twice with overlap, killing every match
     * would cut the previous voice's release tail short and make legato
     * passages sound staccato. */
    for (i = 0; i < MAFM_POLYPHONY; i++) {
        struct mafm_voice *v = &s->voices[i];
        if (_WM_MAFM_VoiceActive(v) && v->channel == ch && v->note == note) {
            _WM_MAFM_VoiceNoteOff(v);
            return;
        }
    }
}

/* Restore a channel's sounding voices to their unmodulated pitch.  Needed
 * whenever vibrato stops, because the render loop then stops retuning them
 * and would otherwise leave the last LFO offset frozen in. */
static void mafm_clear_vibrato(struct mafm_synth *s, uint8_t ch) {
    int i;
    for (i = 0; i < MAFM_POLYPHONY; i++) {
        struct mafm_voice *v = &s->voices[i];
        if (_WM_MAFM_VoiceActive(v) && v->channel == ch)
            _WM_MAFM_VoiceSetPitch(v, mafm_note_hz(v->note, s->chan_pitch[ch]));
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
    case ev_control_channel_modulation:
        s->chan_modulation[ch] = val & 0x7F;
        if (s->chan_modulation[ch] == 0)
            mafm_clear_vibrato(s, ch);
        break;
    case ev_control_channel_controllers_off:
        s->chan_modulation[ch] = 0;
        mafm_clear_vibrato(s, ch);
        break;
    case ev_sysex_gm_reset:
    case ev_sysex_roland_reset:
    case ev_sysex_yamaha_reset: {
        /* A GM/GS/XG reset clears the wheel on every channel. */
        int i;
        for (i = 0; i < 16; i++) {
            s->chan_modulation[i] = 0;
            mafm_clear_vibrato(s, (uint8_t)i);
        }
    } break;
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
    case ev_control_channel_expression: {
        /* Update ALL currently-sounding voices on this channel so volume
         * swells / expression rides reach in-flight notes.  Without this a
         * long note that started at low volume stays low forever, missing
         * the crescendo the score encodes as CC 7/11 rises. */
        int j;
        float v_gain;
        if (event->evtype == ev_control_channel_volume)
            s->chan_volume[ch] = (float)(val & 0x7F) / 127.0f;
        else
            s->chan_expression[ch] = (float)(val & 0x7F) / 127.0f;
        v_gain = s->chan_volume[ch] * s->chan_expression[ch];
        for (j = 0; j < MAFM_POLYPHONY; j++) {
            struct mafm_voice *vp = &s->voices[j];
            if (_WM_MAFM_VoiceActive(vp) && vp->channel == ch)
                _WM_MAFM_VoiceSetVolume(vp, v_gain);
        }
    } break;
    case ev_control_channel_pan:
        s->chan_pan[ch] = (uint8_t)(val & 0x7F);
        break;
    default:
        break;
    }
}

/* Constant-power-ish pan law matching the reference mixer: pan is centred at
 * 0, negative = left, +1 = full right.  L gain = (1 - pan), R gain = (1 + pan),
 * so a centred voice hits both sides equally and a hard-panned voice moves all
 * of its energy to one side (total energy stays constant across the field). */
static void mafm_voice_pan_gains(const struct mafm_synth *s,
                                 const struct mafm_voice *v,
                                 float *gl, float *gr) {
    float pan;
    uint8_t chan_pan = s->chan_pan[v->channel];
    if (chan_pan == 0xff) {
        pan = v->patch.pan_default;                  /* -1..+1 */
    } else {
        pan = ((float)chan_pan - 64.0f) / 64.0f;     /* CC 0..127 -> -1..+1 */
    }
    if      (pan < -1.0f) pan = -1.0f;
    else if (pan >  1.0f) pan =  1.0f;
    *gl = 1.0f - pan;
    *gr = 1.0f + pan;
}

void _WM_MAFM_Render(void *synth, int32_t *out, uint32_t frames) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    /* Soft peak limiter: a dozen voices summing at full-scale would exceed
     * int16 and get hard-clipped by wildmidi_lib's output stage (which
     * sounds harsh).  A gain-rider with instant attack and slow release
     * keeps tuttis clean without squashing sparse passages.  Threshold is
     * below the 32767 cap to leave headroom for reverb / master volume. */
    const double LIM_THRESHOLD = 30000.0;
    const double LIM_RELEASE   = 0.9999;
    uint32_t f, i;
    /* Cache per-voice pan gains once per Render call: pan is a mix of the
     * channel's pan CC and the voice's patch pan_default, both of which are
     * set at note-on time and don't change during the callback window. */
    float pan_l[MAFM_POLYPHONY], pan_r[MAFM_POLYPHONY];
    for (i = 0; i < MAFM_POLYPHONY; i++) {
        struct mafm_voice *v = &s->voices[i];
        if (_WM_MAFM_VoiceActive(v)) mafm_voice_pan_gains(s, v, &pan_l[i], &pan_r[i]);
        else { pan_l[i] = 1.0f; pan_r[i] = 1.0f; }
    }
    for (f = 0; f < frames; f++) {
        double l = 0.0, r = 0.0, pcm, peak, gain;

        /* CC1 vibrato: advance a shared 5Hz triangle LFO and retune any voice
         * whose channel has the mod wheel up.  Depth matches the other
         * backends (50 cents at full wheel, SF2.01 8.4.4).  Updated once per
         * MAFM_VIB_BLOCK frames to keep the inner mixing loop cheap. */
        if ((s->cursor % MAFM_VIB_BLOCK) == 0) {
            double tri;
            s->vib_phase += (double)MAFM_VIB_RATE_HZ * MAFM_VIB_BLOCK / s->rate;
            s->vib_phase -= floor(s->vib_phase);
            /* unit triangle in [-1, 1] */
            tri = (s->vib_phase < 0.5)
                    ? (4.0 * s->vib_phase - 1.0)
                    : (3.0 - 4.0 * s->vib_phase);
            for (i = 0; i < MAFM_POLYPHONY; i++) {
                struct mafm_voice *v = &s->voices[i];
                if (!_WM_MAFM_VoiceActive(v)) continue;
                if (s->chan_modulation[v->channel]) {
                    double cents = 50.0 * s->chan_modulation[v->channel] / 127.0
                                   * tri;
                    _WM_MAFM_VoiceSetPitch(v,
                        mafm_note_hz(v->note, s->chan_pitch[v->channel])
                            * pow(2.0, cents / 1200.0));
                }
            }
        }

        for (i = 0; i < MAFM_POLYPHONY; i++) {
            struct mafm_voice *v = &s->voices[i];
            if (_WM_MAFM_VoiceActive(v)) {
                double x = _WM_MAFM_VoiceTick(v);
                l += x * pan_l[i];
                r += x * pan_r[i];
            }
        }
        /* ATR sampled drums/phrases play alongside the FM voices, on the
         * synth's own sample clock (advanced once per output frame).  PCM
         * triggers have no per-voice channel; keep them centred. */
        pcm = mafm_pcm_tick(s);
        /* Match the reference mixer: 0.32 headroom * int16 range = ~10485 per
         * voice at centre pan (per channel).  With the (1-pan)/(1+pan) pan
         * law above, each channel receives voice_sum * 0.32 at centre, up to
         * voice_sum * 0.64 when hard-panned toward it -- constant total
         * energy across the pan field.  The soft limiter below catches
         * dense-passage overshoot without squashing sparse notes. */
        l = l * 10500.0 + pcm * 20000.0;
        r = r * 10500.0 + pcm * 20000.0;
        peak = fabs(l) > fabs(r) ? fabs(l) : fabs(r);
        if (peak > s->lim_env) s->lim_env = peak;
        else s->lim_env *= LIM_RELEASE;
        if (s->lim_env > LIM_THRESHOLD) {
            gain = LIM_THRESHOLD / s->lim_env;
            l *= gain;
            r *= gain;
        }
        out[f * 2]     += (int32_t) l;
        out[f * 2 + 1] += (int32_t) r;
    }
}

#endif /* WILDMIDI_MAFM */
