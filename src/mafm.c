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
#define MAFM_MAX_WAVES  128     /* decoded ADPCM waves; wave ids are 7-bit */
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
    int drum_note;                   /* fixed pitch for drums; 0 = melodic */
    struct mafm_pcm_params pcm;      /* env + loop; valid when is_pcm */
    struct mafm_voice_patch patch;
};

/* A decoded ADPCM wave (Awa) from the ATR audio track. */
struct mafm_wave {
    int16_t *pcm;
    uint32_t len;                    /* samples */
    int fs;                          /* native sample rate (Hz) */
};

/* PCM voice envelope phase, one-off ADSR modelled on the FM operator EG in
 * ma_fm_core.c.  IDLE = slot free; the others are the classic four stages. */
enum mafm_pcm_eg { MAFM_PCM_EG_IDLE = 0, MAFM_PCM_EG_ATTACK,
                   MAFM_PCM_EG_DECAY, MAFM_PCM_EG_SUSTAIN, MAFM_PCM_EG_RELEASE };

/* A sampled voice playing back a wave. */
struct mafm_pcm_voice {
    const int16_t *pcm;
    uint32_t len;                    /* decoded sample count */
    uint32_t loop_pt;                /* loop start, clamped to len */
    uint32_t end_pt;                 /* loop/end point, clamped to len */
    double pos;                      /* fractional read position (samples) */
    double step;                     /* native_fs / output_rate * pitch ratio */
    float  gain;                     /* volume * expression * velocity^2 */
    float  pan_l, pan_r;             /* per-slot L/R gains from chan_pan CC */
    int    channel;                  /* -1 for ATR one-shots (no owner ch) */
    int    note;
    int    active;
    int    do_loop;                  /* RM flag: loop between loop_pt and end_pt */
    /* Envelope, applied as an extra gain multiplier per sample. */
    double env_level;                /* current amp, 0..1 */
    double atk_step, dec_step, sus_step, rel_step;
    double sus_level;
    uint8_t env_phase;               /* enum mafm_pcm_eg */
    uint8_t sustaining;              /* SR!=0 means sustain phase bleeds too */
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

    /* Two wave banks with different jobs, deliberately NOT merged.  Awa/Mwa
     * waves are the score track's streaming audio: median 13446 samples and up
     * to 706352 (69 s), i.e. whole phrases or whole songs.  Mtsu setup-record
     * waves are instrument samples for sampled voices: median 2128 samples,
     * never over 15872 (2 s).  55 corpus files ship both and about half reuse
     * the same numbers for different waves, so one array cannot hold them. */
    struct mafm_wave waves[MAFM_MAX_WAVES];        /* Awa / Mwa (streaming) */
    struct mafm_wave setup_waves[MAFM_MAX_WAVES];  /* Mtsu records (voices)  */
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

/* Read a MIDI variable-length quantity at *pos.  On success advances *pos past
 * the quantity and returns it; returns MAFM_VLQ_BAD (leaving *pos alone) if it
 * runs off the end or does not terminate within four bytes. */
#define MAFM_VLQ_BAD 0xffffffffu
static uint32_t mafm_vlq(const uint8_t *d, uint32_t n, uint32_t *pos) {
    uint32_t v = 0, i = 0;
    while (*pos + i < n && i < 4) {
        uint8_t b = d[*pos + i];
        v = (v << 7) | (uint32_t)(b & 0x7f);
        i++;
        if (!(b & 0x80)) { *pos += i; return v; }
    }
    return MAFM_VLQ_BAD;
}

/* Is this setup record a wave-delivery record?  Each MA generation spells it
 * differently but they all carry the same body. */
static int mafm_is_wave_record(const uint8_t *p, uint32_t n) {
    if (n < 8 || p[0] != 0x43 || p[1] != 0x79 || p[3] != 0x7f) return 0;
    return (p[2] == 0x08 && p[4] == 0x23) ||    /* MA-7 */
           (p[2] == 0x07 && p[4] == 0x03) ||    /* MA-5 */
           (p[2] == 0x06 && p[4] == 0x03);      /* MA-3, 7-bit packed */
}

/* MA-3 keeps its wave payload 7-bit clean, the way a SysEx body is supposed to
 * be, and _WM_MAFM_Unpack7() undoes it.  MA-5 and MA-7 just store the ADPCM
 * raw and run bytes up to 0xff straight through the setup chunk.
 *
 * Verified over the whole corpus: none of the 312 MA-3 records contains a byte
 * above 0x7f, while every one of the 599 MA-5/MA-7 records does.  Unpacking
 * first is what makes MA-3 decode - 96.8% of records land near zero DC, versus
 * nothing at all when the payload is fed to the decoder as-is. */

/* Decode one wave-delivery record into the wave bank:
 *
 *     43 79 08 7F 23 [waveId] [00] [adpcm ...]      (MA-7)
 *     43 79 07 7F 03 [waveId] [00] [adpcm ...]      (MA-5)
 *
 * The payload is the same 4-bit Yamaha ADPCM as Awa/Mwa, packed low nibble
 * first.  That order is what the corpus supports: decoded low-nibble-first,
 * 100% of the MA-7 records and 95.9% of the MA-5 records land near zero DC
 * with almost no clipping, while high-nibble-first pegs samples at full scale
 * and drifts thousands off centre.
 *
 * Unlike Awa/Mwa the record carries no sample rate, so the wave is stored with
 * fs 0 and mafm_start_pcm_full() takes the rate from the voice record that
 * references it.  Byte [6] is 0 in every known record; its meaning is not
 * established, so it is skipped rather than interpreted. */
static void mafm_add_wave_setup(struct mafm_synth *s, const uint8_t *p,
                                uint32_t n) {
    struct mafm_wave *w;
    const uint8_t *adpcm;
    uint8_t *unpacked = NULL;
    int number;
    uint32_t alen;
    number = p[5] & 0x7f;
    if (number >= MAFM_MAX_WAVES) return;
    w = &s->setup_waves[number];
    if (w->pcm) return;                         /* already have this number   */
    alen = n - 7;
    adpcm = p + 7;
    if (p[2] == 0x06) {                         /* MA-3 ships it 7-bit packed */
        unpacked = (uint8_t *) malloc(alen);
        if (!unpacked) return;
        alen = _WM_MAFM_Unpack7(adpcm, alen, unpacked);
        adpcm = unpacked;
    }
    if (alen == 0) { free(unpacked); return; }
    w->pcm = (int16_t *) calloc((size_t)alen, 2 * sizeof(int16_t));
    if (!w->pcm) { free(unpacked); return; }
    w->len = _WM_MAFM_AdpcmDecodeAll(adpcm, alen, 0 /* low-nibble-first */,
                                     w->pcm);
    w->fs = 0;                                  /* rate comes from the voice  */
    free(unpacked);
    if (number + 1 > s->wave_count) s->wave_count = number + 1;
}

/* Walk one Mtsu chunk body, decoding each setup record.  MA-1/2 handyphone
 * files prefix each SysEx with the meta-event byte ("ff f0 <len> 43 ... f7");
 * MA-3/5/6 files store bare "f0 <len> 43 ... f7" instead.  Accept both.
 *
 * <len> is a variable-length quantity, not a single byte: MA-7 wave records
 * run to well over 127 bytes, and reading one byte walks the parser into their
 * ADPCM payload.  The two forms agree for every length below 128, so this is
 * the same behaviour as before for voice-only files. */
static void mafm_parse_mtsu(struct mafm_synth *s, const uint8_t *body,
                            uint32_t n) {
    uint32_t p = 0;
    while (p + 2 <= n) {
        uint32_t hdr, len, q;
        if (body[p] == 0xff && p + 3 <= n && body[p + 1] == 0xf0)
            hdr = 2;                                /* ff f0 <len> */
        else if (body[p] == 0xf0)
            hdr = 1;                                /* f0 <len> */
        else { p++; continue; }
        q = p + hdr;
        len = mafm_vlq(body, n, &q);
        if (len == MAFM_VLQ_BAD || (uint64_t)q + len > n) break;
        {
            const uint8_t *payload = body + q;
            uint32_t plen = len;
            struct mafm_parsed_voice pv;
            if (plen > 0 && payload[plen - 1] == 0xf7) plen--;
            if (mafm_is_wave_record(payload, plen)) {
                mafm_add_wave_setup(s, payload, plen);
            } else {
                _WM_MAFM_ParseVoiceExclusive(payload, plen, &pv);
                if (pv.valid && s->bank_count < MAFM_MAX_BANK) {
                    struct mafm_bank_entry *e = &s->bank[s->bank_count++];
                    e->bank = pv.key.bank_lsb;
                    e->pc = pv.key.pc;
                    e->is_pcm = pv.is_pcm;
                    e->wave_id = pv.pcm.wave_id;
                    e->drum_note = pv.key.drum_note;
                    e->pcm = pv.pcm;
                    e->patch = pv.patch;
                }
            }
            p = q + len;
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

/* Claim wave slot `number` in the Awa/Mwa bank.  Returns the slot, or NULL if
 * it is already taken.  Setup-record waves live in their own bank, so the two
 * sources no longer contend for a number. */
static struct mafm_wave *mafm_claim_wave(struct mafm_synth *s, int number) {
    struct mafm_wave *w;
    /* Bound on the slot itself, not on wave_count: wave_count is the highest
     * number seen plus one, and a setup record may legitimately push it high
     * (Blossom ships wave 0x64) while lower slots are still free. */
    if (number < 0 || number >= MAFM_MAX_WAVES)
        return NULL;
    w = &s->waves[number];
    if (w->pcm) return NULL;                    /* already have this number  */
    return w;
}

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

/* Decode one Awa (ATR) wave body ([formatByte][fmt2][adpcm...]) into the bank.
 * fmt2 holds an ATR-style 4-bit rate class - mafm_wave_rate() maps it to Hz. */
static void mafm_add_wave(struct mafm_synth *s, int number,
                          const uint8_t *body, uint32_t sz) {
    struct mafm_wave *w;
    const uint8_t *adpcm;
    uint32_t alen;
    if (sz < 2) return;
    adpcm = body + 2;
    alen = sz - 2;
    w = mafm_claim_wave(s, number);
    if (!w) return;                  /* already have this wave number */
    w->pcm = (int16_t *) calloc((size_t)alen, 2 * sizeof(int16_t));
    if (!w->pcm) return;
    w->len = _WM_MAFM_AdpcmDecodeAll(adpcm, alen, 0 /* low-nibble-first */, w->pcm);
    w->fs = mafm_wave_rate(body[1]);
    if (number + 1 > s->wave_count) s->wave_count = number + 1;
}

/* Decode one Mwa (Mtsp / score track) wave body into the bank.  Same Yamaha
 * ADPCM as Awa, but with a 3-byte header carrying an explicit u16 BE sample
 * rate instead of Awa's 4-bit class code:
 *
 *     [formatByte][rateHi][rateLo][adpcm...]
 *
 * The corpus rates that occur are 8000, 11025, 12000, 15000, 16000, 22050,
 * 22000, 24000; refuse anything outside a sane range so a stray body cannot
 * pin the resampler at 0 or run wildly fast. */
static void mafm_add_wave_mwa(struct mafm_synth *s, int number,
                              const uint8_t *body, uint32_t sz) {
    struct mafm_wave *w;
    const uint8_t *adpcm;
    uint32_t alen;
    int fs;
    if (sz < 3) return;
    fs = ((int)body[1] << 8) | body[2];
    if (fs < 4000 || fs > 48000) return;
    adpcm = body + 3;
    alen = sz - 3;
    w = mafm_claim_wave(s, number);
    if (!w) return;                  /* already have this wave number */
    w->pcm = (int16_t *) calloc((size_t)alen, 2 * sizeof(int16_t));
    if (!w->pcm) return;
    w->len = _WM_MAFM_AdpcmDecodeAll(adpcm, alen, 0 /* low-nibble-first */, w->pcm);
    w->fs = fs;
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

/* Scan every MTR score track for its Mtsp chunk, and pull each Mwa* wave out
 * of it into the bank.  Mtsp lives inside the score track alongside Mtsu and
 * Mtsq; the score's format byte controls the header width (mirrors the walker
 * in mafm_build_bank).  Called after mafm_build_waves() so Awa keeps priority
 * for a given wave number if both provide it. */
static void mafm_build_mtsp_waves(struct mafm_synth *s, const uint8_t *in,
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

        if (memcmp(c, "MTR", 3) == 0 && sz >= 1) {
            uint8_t fmt = c[8];
            uint8_t chlen = (fmt == 0x00) ? 2 : (fmt == 0x03) ? 32 : 16;
            uint32_t hdr = 4 + chlen;
            uint32_t q = body + hdr, tend = body + sz;
            while (q + 8 <= tend) {
                const uint8_t *sc = in + q;
                uint32_t ssz = MAFM_BE32(sc + 4);
                uint32_t sbody = q + 8;
                if ((uint64_t)sbody + ssz > insize) ssz = insize - sbody;
                if (memcmp(sc, "Mtsp", 4) == 0) {
                    /* Mtsp is a container: each inner chunk is Mwa<n>. */
                    uint32_t r = sbody, rend = sbody + ssz;
                    while (r + 8 <= rend) {
                        const uint8_t *ic = in + r;
                        uint32_t isz = MAFM_BE32(ic + 4);
                        uint32_t ibody = r + 8;
                        if ((uint64_t)ibody + isz > insize) isz = insize - ibody;
                        if (memcmp(ic, "Mwa", 3) == 0)
                            mafm_add_wave_mwa(s, ic[3], in + ibody, isz);
                        r = ibody + isz;
                        if (isz == 0) r++;
                    }
                }
                q = sbody + ssz;
                if (ssz == 0) q++;
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
    /* Also engage MAFM when the file has only Mtsp waves and no Mtsu voice
     * definitions - the streaming-audio case (see the special bank entry
     * synthesised at the bottom of _WM_MAFM_NewSynth).  Without this the plain
     * GM path handles those files and their single-note trigger plays
     * whatever GM patch bank 0x7d resolves to instead of the actual audio. */
    if (tmp->bank_count == 0) {
        mafm_build_mtsp_waves(tmp, smaf, size);
    }
    /* Also for audio-only files (CNTI + ATR, no MTR): ATR waves + Atsq
     * triggers ARE the file's content, so MAFM has to engage even though
     * no voice bank exists.  smaf2mid emits a placeholder score for these;
     * without engaging MAFM the placeholder plays as silence. */
    if (tmp->bank_count == 0 && tmp->wave_count == 0) {
        mafm_build_waves(tmp, smaf, size);
    }
    has = tmp->bank_count > 0 || tmp->wave_count > 0;
    /* Not plain free(): the probe decodes real waves into both banks, so it
     * has to go through the same teardown a live synth does or every call
     * leaks the decoded PCM (hundreds of KB for a streaming Mwa). */
    _WM_MAFM_FreeSynth(tmp);
    return has;
}

/* Resolve a wave number for a SAMPLED VOICE.  The Mtsu setup bank holds the
 * instrument samples voices are written against, so it wins; the Awa/Mwa bank
 * is the score's streaming audio and is only consulted for files whose voices
 * reference a number solely it supplies. */
static struct mafm_wave *mafm_voice_wave(struct mafm_synth *s, int id) {
    if (id < 0 || id >= MAFM_MAX_WAVES) return NULL;
    if (s->setup_waves[id].pcm && s->setup_waves[id].len)
        return &s->setup_waves[id];
    if (s->waves[id].pcm && s->waves[id].len)
        return &s->waves[id];
    return NULL;
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
    mafm_build_mtsp_waves(s, smaf, size);

    /* Streaming-audio special case: some MA-7 files ship an entire song as
     * one Mwa wave in Mtsp, then trigger it with a single note-on on the
     * drum bank (0x7d) with no matching Mtsu voice-exclusive - the chip's own
     * "bank 0x7d + wave loaded" implicit binding.  If we loaded Mtsp waves
     * but no voice-exclusive claimed them, synthesise a bank entry so that
     * lone note-on plays the wave instead of falling back to a GM patch. */
    if (s->bank_count == 0 && s->wave_count > 0) {
        int firstwave = -1;
        for (i = 0; i < s->wave_count; i++)
            if (s->waves[i].pcm) { firstwave = i; break; }
        if (firstwave >= 0 && MAFM_MAX_BANK > 0) {
            struct mafm_bank_entry *e = &s->bank[s->bank_count++];
            memset(e, 0, sizeof(*e));
            e->bank = 0;                 /* any bank (matched wildly, see select) */
            e->pc = 0;
            e->is_pcm = 1;
            e->wave_id = firstwave;
            /* Non-zero drum_note = "fixed pitch, play the wave at its native
             * rate regardless of the incoming note".  The streaming files
             * trigger with note 0; without this, the melodic-voice pitch
             * shift below would resample the wave down 5 octaves and drag the
             * playback to a crawl (16000 Hz -> ~500 Hz). */
            e->drum_note = 1;
        }
    }
    return s;
}

void _WM_MAFM_FreeSynth(void *synth) {
    struct mafm_synth *s = (struct mafm_synth *) synth;
    int i;
    if (!s) return;
    for (i = 0; i < MAFM_MAX_WAVES; i++) {
        free(s->waves[i].pcm);
        free(s->setup_waves[i].pcm);
    }
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
    /* Count a pending ATR wave trigger as active if it's due within a bounded
     * window ahead of the current sample cursor.  This keeps audio-only files
     * (whose smaf2mid placeholder is a short 3 s MIDI) alive long enough for
     * their ATR-scheduled triggers to fire - without which wildmidi_lib's
     * "past total_samples + no active voices -> break" logic would end
     * playback before the wave hit.  The cap prevents a malformed schedule
     * with a far-future trigger from spinning the synth forever. */
    if (s->trig_next < s->trig_count) {
        uint32_t next_at = s->trigs[s->trig_next].at_sample;
        uint32_t window = (uint32_t)(s->rate * 15.0);   /* 15 s ahead */
        if (next_at >= s->cursor && next_at - s->cursor <= window) n++;
    }
    return n;
}

/* Chip envelope-rate (0..15) to a per-sample linear increment.  Same shape as
 * the FM operator EG in ma_fm_core.c: rate 0 = never advances (attack floors
 * to instant, decay/release fall to a slow ~20s glide so the slot always
 * retires), rate 15 = ~1ms, rate 1 = ~4s.  Reused for AR/DR/SR/RR. */
static double pcm_rate_to_step(uint8_t rate, double sample_rate, int attack) {
    double fast, slow, t, samples;
    if (rate == 0) return attack ? 0.0 : (1.0 / (sample_rate * 20.0));
    if (rate > 15) rate = 15;
    fast = attack ? 0.0008 : 0.004;              /* rate 15 sweep time (sec) */
    slow = attack ? 0.35   : 4.0;                /* rate 1  sweep time (sec) */
    t = slow * pow(fast / slow, ((double) rate - 1.0) / 14.0);
    samples = t * sample_rate;
    if (samples < 1.0) samples = 1.0;
    return 1.0 / samples;
}

/* One envelope step, mirroring the FM operator EG. */
static double pcm_env_advance(struct mafm_pcm_voice *pv) {
    switch (pv->env_phase) {
    case MAFM_PCM_EG_IDLE:
        return 0.0;
    case MAFM_PCM_EG_ATTACK:
        pv->env_level += (pv->atk_step <= 0.0 ? 1.0 : pv->atk_step);
        if (pv->env_level >= 1.0) {
            pv->env_level = 1.0;
            pv->env_phase = MAFM_PCM_EG_DECAY;
        }
        break;
    case MAFM_PCM_EG_DECAY:
        pv->env_level -= pv->dec_step;
        if (pv->env_level <= pv->sus_level) {
            pv->env_level = pv->sus_level;
            pv->env_phase = pv->sustaining ? MAFM_PCM_EG_SUSTAIN
                                           : MAFM_PCM_EG_RELEASE;
        }
        break;
    case MAFM_PCM_EG_SUSTAIN:
        pv->env_level -= pv->sus_step;
        if (pv->env_level <= 0.0) {
            pv->env_level = 0.0;
            pv->env_phase = MAFM_PCM_EG_IDLE;
        }
        break;
    case MAFM_PCM_EG_RELEASE:
        pv->env_level -= pv->rel_step;
        if (pv->env_level <= 0.0) {
            pv->env_level = 0.0;
            pv->env_phase = MAFM_PCM_EG_IDLE;
        }
        break;
    }
    return pv->env_level;
}

/* Start a sampled wave playing on a free PCM slot.  channel < 0 marks an
 * "ownerless" trigger (ATR drums, phrases) which are one-shot at centre pan
 * with no envelope; channel >= 0 attaches the slot to a MIDI channel so pan
 * tracks the channel's pan CC and note-off can find and release its own
 * trigger.  base_note is the MIDI note at which the wave plays at its native
 * rate (drums fix it to the played note; melodic voices use 60).  params is
 * the voice's env + loop config, or NULL for an unenvelope one-shot. */
static void mafm_start_pcm_full(struct mafm_synth *s, struct mafm_wave *w,
                                float gain, int channel, int note,
                                int base_note,
                                const struct mafm_pcm_params *params) {
    struct mafm_pcm_voice *pv = NULL;
    int i;
    float pan;
    if (!w || !w->pcm || w->len == 0) return;
    for (i = 0; i < MAFM_PCM_POOL; i++)
        if (!s->pcm[i].active) { pv = &s->pcm[i]; break; }
    if (!pv) pv = &s->pcm[0];       /* steal slot 0 if the pool is full */
    pv->pcm = w->pcm;
    pv->len = w->len;
    pv->pos = 0.0;
    /* Pitch step.  For a drum voice base_note == the played note (so the ratio
     * is 1.0 and pitch bend is ignored - drums never pitch-bend).  For a
     * melodic voice base_note is a fixed root and the played note plus the
     * channel's pitch wheel shift determines the resample ratio. */
    {
        double bend_semitones = 0.0;
        double ratio;
        int fs = w->fs;
        /* MA-7 wave-delivery records (7F 23) carry no rate of their own, and
         * are stored with fs 0; the voice record referencing the wave supplies
         * it.  Awa/Mwa waves always set a rate, so they are unaffected. */
        if (fs <= 0) fs = (params && params->fs > 0) ? params->fs : 8000;
        if (channel >= 0 && base_note != note) {
            bend_semitones = ((double)(s->chan_pitch[channel] - 0x2000) / 8192.0) * 2.0;
        }
        ratio = pow(2.0, ((double)(note - base_note) + bend_semitones) / 12.0);
        pv->step = (double) fs / s->rate * ratio;
    }
    pv->gain = gain;
    pv->channel = channel;
    pv->note = note;
    /* Pan.  channel < 0 stays centred; channel >= 0 tracks its chan_pan CC.
     * PCM voices carry no per-patch pan_default field, so unlike FM voices
     * an unset chan_pan (0xff sentinel) just means centre. */
    if (channel < 0) {
        pv->pan_l = 1.0f;
        pv->pan_r = 1.0f;
    } else {
        uint8_t cp = s->chan_pan[channel];
        pan = (cp == 0xff) ? 0.0f : (((float) cp - 64.0f) / 64.0f);
        if      (pan < -1.0f) pan = -1.0f;
        else if (pan >  1.0f) pan =  1.0f;
        pv->pan_l = 1.0f - pan;
        pv->pan_r = 1.0f + pan;
    }
    /* Loop region.  end_pt = 0 means "play the whole wave" per the parser;
     * loop_pt/end_pt are sample indices from the SMAF PCM header, clamped to
     * the actual decoded length.  do_loop drives the wrap in the tick loop. */
    pv->loop_pt = 0;
    pv->end_pt = w->len;
    pv->do_loop = 0;
    if (params && params->end_pt > 0 && (uint32_t) params->end_pt <= w->len) {
        pv->end_pt = (uint32_t) params->end_pt;
        if ((uint32_t) params->loop_pt < pv->end_pt) {
            pv->loop_pt = (uint32_t) params->loop_pt;
        }
        pv->do_loop = params->loop;
    }
    /* Envelope.  Without params (ATR one-shots) leave the EG idle and the
     * sample plays at its raw gain, matching the pre-envelope behaviour.
     * With params, initialise ADSR from the voice's TL/AR/DR/SL/SR/RR fields
     * and enter the attack phase.  TL folds into the base gain (steady-state
     * attenuation, 6-bit: 0 = full, 63 = silent) so the envelope value itself
     * stays in [0..1] and the ADSR maths matches the FM operator's.
     *
     * Skip the envelope entirely when every field is zero (the memset state
     * the streaming special case leaves behind, and MA-7 sampled forms whose
     * env has not been reverse-engineered).  Otherwise those slots would
     * attack instantly, hit the RR=0 "slow ~20s glide" floor and fade out
     * long before their wave finishes. */
    if (params && (params->env.ar || params->env.dr || params->env.sr ||
                   params->env.rr || params->env.sl || params->env.tl ||
                   params->env.eg_type)) {
        const struct mafm_op_patch *e = &params->env;
        double tl_gain;
        pv->atk_step = pcm_rate_to_step(e->ar, s->rate, 1);
        pv->dec_step = pcm_rate_to_step(e->dr, s->rate, 0);
        pv->sus_step = pcm_rate_to_step(e->sr, s->rate, 0);
        pv->rel_step = pcm_rate_to_step(e->rr, s->rate, 0);
        pv->sus_level = 1.0 - ((double) e->sl / 15.0);   /* sl 0 top, 15 low */
        pv->sustaining = e->eg_type;
        pv->env_phase = MAFM_PCM_EG_ATTACK;
        pv->env_level = 0.0;
        /* -0.75 dB per TL step matches the FM engine's tl_to_gain(). */
        tl_gain = (e->tl >= 63) ? 0.0 : pow(10.0, (-0.75 * (double) e->tl) / 20.0);
        pv->gain *= (float) tl_gain;
    } else {
        pv->atk_step = 0.0;
        pv->dec_step = 0.0;
        pv->sus_step = 0.0;
        pv->rel_step = 0.0;
        pv->sus_level = 1.0;
        pv->sustaining = 0;
        pv->env_phase = MAFM_PCM_EG_IDLE;
        pv->env_level = 1.0;                     /* pass-through gain */
    }
    pv->active = 1;
}

static void mafm_start_pcm(struct mafm_synth *s, int wave) {
    /* ATR triggers are ownerless one-shots; note = base_note keeps ratio = 1. */
    if (wave < 0 || wave >= MAFM_MAX_WAVES) return;
    mafm_start_pcm_full(s, &s->waves[wave], 1.0f, -1, 0, 0, NULL);
}

/* Advance the ATR trigger schedule and render one PCM sample, summed into
 * *l and *r with each slot's own pan (ownerless ATR triggers stay centred). */
static void mafm_pcm_tick(struct mafm_synth *s, double *l, double *r) {
    int i;
    /* fire any triggers whose time has arrived */
    while (s->trig_next < s->trig_count &&
           s->trigs[s->trig_next].at_sample <= s->cursor) {
        mafm_start_pcm(s, s->trigs[s->trig_next].wave);
        s->trig_next++;
    }
    for (i = 0; i < MAFM_PCM_POOL; i++) {
        struct mafm_pcm_voice *pv = &s->pcm[i];
        double x, env;
        uint32_t idx;
        if (!pv->active) continue;
        idx = (uint32_t) pv->pos;
        /* End of the region: either wrap for looped voices or retire. */
        if (idx >= pv->end_pt) {
            if (pv->do_loop && pv->end_pt > pv->loop_pt) {
                double span = (double)(pv->end_pt - pv->loop_pt);
                double over = pv->pos - (double) pv->end_pt;
                if (span > 0.0) over = fmod(over, span);
                pv->pos = (double) pv->loop_pt + over;
                idx = (uint32_t) pv->pos;
                if (idx >= pv->end_pt) { pv->active = 0; continue; }
            } else {
                pv->active = 0;
                continue;
            }
        }
        env = (pv->env_phase == MAFM_PCM_EG_IDLE) ? pv->env_level
                                                  : pcm_env_advance(pv);
        /* An envelope-driven slot retires when the envelope decays to zero,
         * even if the wave still has samples left (a plucked note that has
         * long finished releasing must free its slot). */
        if (pv->env_phase == MAFM_PCM_EG_IDLE && env <= 0.0) {
            pv->active = 0;
            continue;
        }
        x = (double) pv->pcm[idx] / 32768.0 * pv->gain * env;
        *l += x * pv->pan_l;
        *r += x * pv->pan_r;
        pv->pos += pv->step;
    }
    s->cursor++;
}

static void mafm_note_on(struct mafm_synth *s, int ch, int note, int vel) {
    struct mafm_voice_patch patch;
    struct mafm_voice *v;
    int is_drum = (ch == 9);
    int bank_idx = mafm_select_patch(s, ch, is_drum, note, &patch);
    int sounding_note;
    float vel01, vel_curve;
    /* Matched voice is a sampled instrument.  Play its wave one-shot at the
     * native rate: SMAF sampled voices are overwhelmingly drums (a corpus
     * audit puts them at 2537 records vs 2355 FM), where fixed-pitch playback
     * is the intended behaviour.  A future pass
     * can add pitch shift for the melodic sampled voices some MA-7 files
     * carry; for now they play at concert pitch, which is more useful than
     * the silence they used to. */
    if (bank_idx >= 0 && s->bank[bank_idx].is_pcm) {
        struct mafm_wave *w = mafm_voice_wave(s, s->bank[bank_idx].wave_id);
        if (w) {
            /* Same default-velocity trick the FM branch uses just below:
             * SMAF's HandyPhone score carries no velocity byte, so the
             * converter emits vel=0 to mean "no explicit velocity", which we
             * treat as 100. */
            float pv = (vel ? (float) vel : 100.0f) / 127.0f;
            float g = s->chan_volume[ch] * s->chan_expression[ch] * pv * pv;
            /* Fixed pitch for drums (drum_note != 0) means playing the wave
             * at native rate regardless of the incoming note.  A melodic PCM
             * voice takes root note 60, matching the "root=middle C" default
             * every wavetable synth WildMIDI already ships uses. */
            int drum_note = s->bank[bank_idx].drum_note;
            int base = drum_note ? note : 60;
            mafm_start_pcm_full(s, w, g, ch, note, base,
                                &s->bank[bank_idx].pcm);
            return;
        }
        /* Wave not loaded.  Yamaha MA chips carry a ROM sample bank that PCM
         * voices reference by wave_id; the corpus audit shows 361 of 415
         * MA-3/5 files with PCM voices point exclusively at ROM waves 0..~25
         * that no file provides.  Rather than silence those notes, fall back
         * to the FM drum approximation - a synthetic kick/snare beats a
         * missing hit for percussion tracks.  See docs/formats/SmafFileFormat.txt
         * for the ROM-wave situation. */
        _WM_MAFM_DrumApprox(note, &patch);
        /* fall through to the FM code path below with the approx patch */
    }
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
    /* No FM match: try the PCM pool.  Envelope-driven PCM voices transition
     * to their release phase and fade over RR; slots without an envelope
     * (ATR one-shots) ignore note-off and ring out naturally, which is what
     * a drum hit should do. */
    for (i = 0; i < MAFM_PCM_POOL; i++) {
        struct mafm_pcm_voice *pv = &s->pcm[i];
        if (pv->active && pv->channel == ch && pv->note == note &&
            pv->env_phase != MAFM_PCM_EG_IDLE &&
            pv->env_phase != MAFM_PCM_EG_RELEASE) {
            pv->env_phase = MAFM_PCM_EG_RELEASE;
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
        double l = 0.0, r = 0.0, peak, gain;

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
        /* ATR sampled drums/phrases and note-driven PCM voices play alongside
         * the FM voices, on the synth's own sample clock (advanced once per
         * output frame).  ATR triggers are centre-panned; note-driven PCM
         * voices carry each slot's own L/R gains from its channel's pan CC. */
        {
            double pcm_l = 0.0, pcm_r = 0.0;
            mafm_pcm_tick(s, &pcm_l, &pcm_r);
            /* Match the reference mixer: 0.32 headroom * int16 range = ~10485
             * per voice at centre pan.  With the (1-pan)/(1+pan) pan law
             * above, each channel receives voice_sum * 0.32 at centre, up to
             * voice_sum * 0.64 when hard-panned toward it - constant total
             * energy across the pan field.  The soft limiter below catches
             * dense-passage overshoot without squashing sparse notes. */
            l = l * 10500.0 + pcm_l * 20000.0;
            r = r * 10500.0 + pcm_r * 20000.0;
        }
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
