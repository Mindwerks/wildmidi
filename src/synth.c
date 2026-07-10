/*
 * synth.c -- OPL3-based fallback soundbank for WildMIDI.
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
 * Uses the Nuked OPL3 FM chip emulator (src/opl3.c, LGPL 2.1+, from
 * https://github.com/tgies/Nuked-OPL3-fast) to render short PCM samples per
 * GM program on demand; those samples then flow through WildMIDI's existing
 * wavetable mixer via the standard struct _sample path.
 *
 * The GM instrument register table below is original work written for this
 * file (values chosen per family by ear), not lifted from any existing
 * GENMIDI/AdLib bank. Register semantics per the Yamaha YMF262 datasheet.
 */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "wm_error.h"
#include "patches.h"
#include "sample.h"
#include "internal_midi.h"
#include "opl3.h"
#include "synth.h"
#include "synth_bank.h"

/* Register offsets for op1 (modulator) and op2 (carrier) of OPL3 channel 0. */
#define OP1 0x00
#define OP2 0x03

/* ------------------------------------------------------------------ */
/* FM patch table                                                     */
/* ------------------------------------------------------------------ */

/* Classic 2-op OPL instrument: one register byte per field.
   chr: AM|VIB|EGT|KSR|MULT   (reg 0x20)
   lvl: KSL|TL                (reg 0x40; TL 0 = loudest)
   ad:  attack<<4 | decay     (reg 0x60)
   sr:  sustain<<4 | release  (reg 0x80)
   ws:  waveform 0..7         (reg 0xE0)
   fb:  feedback<<1 | conn    (reg 0xC0 low bits; stereo bits added later) */
typedef struct {
    uint8_t mod_chr, mod_lvl, mod_ad, mod_sr, mod_ws;
    uint8_t car_chr, car_lvl, car_ad, car_sr, car_ws;
    uint8_t fb;
} fm_patch;

/* Mixer-envelope parameters: sustain in mixer fixed-point (4194303 = 1.0),
   attack/decay/release in seconds. */
typedef struct { int32_t sustain; float attack, decay, release; } mix_env;

/* ------------------------------------------------------------------ */
/* GENMIDI (.op2) bank                                                */
/* ------------------------------------------------------------------ */

/* DMX GENMIDI lump: "#OPL_II#" + 175 records x 36 bytes (0-127 GM programs,
   128-174 percussion keys 35-81), then 175 x 32-byte names (ignored).
   Record: u16 flags, u8 finetune, u8 fixed-note, 2 x 16-byte voices.
   Voice: mod chr/ad/sr/ws/ksl/lvl, feedback, car chr/ad/sr/ws/ksl/lvl,
   unused, s16 note offset. */
#define OP2_RECORDS   175
#define OP2_RECSIZE   36
#define OP2_HDRSIZE   8

static uint8_t op2_bank[OP2_RECORDS * OP2_RECSIZE];
static int op2_active = 0;

int _WM_OP2_Magic(const uint8_t *data, uint32_t size) {
    return (size >= OP2_HDRSIZE + OP2_RECORDS * OP2_RECSIZE
            && !memcmp(data, "#OPL_II#", 8));
}

int _WM_OP2_Load(const uint8_t *data, uint32_t size) {
    if (!_WM_OP2_Magic(data, size)) return -1;
    memcpy(op2_bank, data + OP2_HDRSIZE, sizeof(op2_bank));
    op2_active = 1;
    return 0;
}

void _WM_OP2_Unload(void) {
    op2_active = 0;
}

#define OP2_FLAG_FIXED   0x01
#define OP2_FLAG_DOUBLE  0x04

/* Build an fm_patch from one 16-byte GENMIDI voice. Returns the voice's
   note offset in semitones via *note_off. */
static void op2_voice(const uint8_t *v, fm_patch *p, int16_t *note_off) {
    p->mod_chr = v[0];
    p->mod_ad  = v[1];
    p->mod_sr  = v[2];
    p->mod_ws  = v[3];
    p->mod_lvl = (uint8_t)((v[4] & 0xC0) | (v[5] & 0x3F));
    p->fb      = v[6];
    p->car_chr = v[7];
    p->car_ad  = v[8];
    p->car_sr  = v[9];
    p->car_ws  = v[10];
    p->car_lvl = (uint8_t)((v[11] & 0xC0) | (v[12] & 0x3F));
    *note_off  = (int16_t)(v[14] | (v[15] << 8));
}

/* 2^(semitones/12) without libm. */
static double semitone_ratio(int off) {
    static const double semi[12] = {
        1.0, 1.0594630944, 1.1224620483, 1.1892071150, 1.2599210499,
        1.3348398542, 1.4142135624, 1.4983070769, 1.5874010520,
        1.6817928305, 1.7817974363, 1.8877486254
    };
    double r = 1.0;
    while (off >= 12) { r *= 2.0;  off -= 12; }
    while (off < 0)   { r *= 0.5;  off += 12; }
    return r * semi[off];
}

/* MIDI note -> Hz via the existing frequency table (Hz * 100000). */
static double note_hz(uint8_t note) {
    return (double)(_WM_freq_table[(note % 12) * 100] >> (10 - (note / 12)))
           / 100000.0;
}

/* Hz -> (fnum, block) at the OPL's 49716 Hz clock, keeping fnum in the
   high-resolution 512..1023 range where possible. */
static void hz_to_fnum(double hz, uint16_t *fnum, uint8_t *block) {
    int b = 1;
    double f;
    for (;;) {
        f = hz * (double)(1u << (20 - b)) / 49716.0;
        if (f < 1023.0 || b >= 7) break;
        b++;
    }
    if (f > 1023.0) f = 1023.0;
    if (f < 1.0) f = 1.0;
    *fnum = (uint16_t)f;
    *block = (uint8_t)b;
}

/* ------------------------------------------------------------------ */
/* OPL helpers                                                        */
/* ------------------------------------------------------------------ */

/* One playable voice: register data + pitch. */
typedef struct { const fm_patch *fm; uint16_t fnum; uint8_t block; } fm_voice;

static void opl_program_voice(opl3_chip *c, uint8_t ch, const fm_patch *p) {
    OPL3_WriteReg(c, 0x20 + OP1 + ch, p->mod_chr);
    OPL3_WriteReg(c, 0x20 + OP2 + ch, p->car_chr);
    OPL3_WriteReg(c, 0x40 + OP1 + ch, p->mod_lvl);
    OPL3_WriteReg(c, 0x40 + OP2 + ch, p->car_lvl);
    OPL3_WriteReg(c, 0x60 + OP1 + ch, p->mod_ad);
    OPL3_WriteReg(c, 0x60 + OP2 + ch, p->car_ad);
    OPL3_WriteReg(c, 0x80 + OP1 + ch, p->mod_sr);
    OPL3_WriteReg(c, 0x80 + OP2 + ch, p->car_sr);
    OPL3_WriteReg(c, 0xE0 + OP1 + ch, p->mod_ws);
    OPL3_WriteReg(c, 0xE0 + OP2 + ch, p->car_ws);
    OPL3_WriteReg(c, 0xC0 + ch, 0x30 | p->fb); /* both speakers + fb/conn */
}

static void opl_key_on(opl3_chip *c, uint8_t ch, uint16_t fnum, uint8_t block) {
    OPL3_WriteReg(c, 0xA0 + ch, fnum & 0xFF);
    OPL3_WriteReg(c, 0xB0 + ch, 0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
}

/* Reset the chip and start v1 (and v2 for GENMIDI double-voice instruments)
   on channels 0 and 1; the chip mixes the layers itself. */
static void opl_boot(opl3_chip *c, const fm_voice *v1, const fm_voice *v2) {
    OPL3_Reset(c, _WM_SampleRate);
    OPL3_WriteReg(c, 0x105, 0x01);            /* OPL3 "NEW" mode */
    /* NTS=1 (keyboard split point), as DMX programs it: KSR envelope rate
       scaling then matches what GENMIDI banks were tuned against. */
    OPL3_WriteReg(c, 0x08, 0x40);
    opl_program_voice(c, 0, v1->fm);
    opl_key_on(c, 0, v1->fnum, v1->block);
    if (v2) {
        opl_program_voice(c, 1, v2->fm);
        opl_key_on(c, 1, v2->fnum, v2->block);
    }
}

/* Render `n` mono samples, downmixing the chip's stereo output. */
static void opl_render(opl3_chip *c, int16_t *out, uint32_t n) {
    uint32_t i;
    int16_t stereo[2];
    for (i = 0; i < n; i++) {
        OPL3_GenerateResampled(c, stereo);
        out[i] = (int16_t)(((int32_t)stereo[0] + (int32_t)stereo[1]) / 2);
    }
}

/* Rough OPL envelope rate -> seconds maps (each +1 halves the time;
   constants eyeballed from the YMF262 decay-time table). Used to derive
   note-off release, one-shot lengths and attack windows from bank data. */
static float opl_rate_seconds(uint8_t r) {
    float t;
    if (r == 0) return 3.0f;
    t = 20.8f / (float)(1u << r);
    if (t < 0.03f) t = 0.03f;
    if (t > 3.0f) t = 3.0f;
    return t;
}

/* Attack is roughly 4x faster than decay at the same rate value. Slow
   attacks (rate 1-2: reverse cymbal, seashore, sweep pads) genuinely take
   seconds — the render window has to cover them or the sample is silence. */
static float opl_attack_seconds(uint8_t r) {
    float t;
    if (r == 0) return 5.2f;
    t = 5.2f / (float)(1u << r);
    return t;
}

/* OPL pitch for (fnum, block) at the chip's native 49716 Hz clock. */
static double opl_hz(uint16_t fnum, uint8_t block) {
    return (double)fnum * 49716.0 / (double)(1u << (20 - block));
}

/* ------------------------------------------------------------------ */
/* Sample construction                                                */
/* ------------------------------------------------------------------ */

static int32_t env_rate_for_seconds(float seconds) {
    if (seconds <= 0.0f) seconds = 0.001f;
    return (int32_t)(4194303.0f / ((float)_WM_SampleRate * seconds));
}

static void set_envelope(struct _sample *s, const mix_env *e) {
    const int32_t peak = 4194303;
    s->env_target[0] = peak;       s->env_rate[0] = env_rate_for_seconds(e->attack);
    s->env_target[1] = e->sustain; s->env_rate[1] = env_rate_for_seconds(e->decay);
    s->env_target[2] = e->sustain; s->env_rate[2] = env_rate_for_seconds(0.5f);
    s->env_target[3] = e->sustain; s->env_rate[3] = env_rate_for_seconds(4.0f);
    s->env_target[4] = 0;          s->env_rate[4] = env_rate_for_seconds(e->release);
    s->env_target[5] = 0;          s->env_rate[5] = env_rate_for_seconds(e->release);
    s->env_target[6] = 0;          s->env_rate[6] = env_rate_for_seconds(0.010f);
}

static struct _sample *alloc_sample(uint32_t n) {
    struct _sample *s = (struct _sample *)calloc(1, sizeof(struct _sample));
    if (!s) return NULL;
    s->data = (int16_t *)calloc((size_t)n + 2, sizeof(int16_t));
    if (!s->data) { free(s); return NULL; }
    return s;
}

static void free_sample_chain(struct _sample *s) {
    while (s) {
        struct _sample *next = s->next;
        free(s->data);
        free(s);
        s = next;
    }
}

/* Peak-normalise the buffer to `target`, fading the last `tail_fade`
   samples so one-shots don't click at cutoff. */
static void normalise_to(int16_t *d, uint32_t n, int32_t target, uint32_t tail_fade) {
    int32_t peak = 1;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = d[i] < 0 ? -d[i] : d[i];
        if (a > peak) peak = a;
    }
    if (tail_fade > n) tail_fade = n;
    for (i = 0; i < n; i++) {
        int32_t v = (int32_t)d[i] * target / peak;
        if (tail_fade && i >= n - tail_fade) {
            v = v * (int32_t)(n - 1 - i) / (int32_t)tail_fade;
        }
        d[i] = (int16_t)v;
    }
}

/* Normalisation target. One fixed value for every voice: DMX (and thus the
   banks made for it) never uses the carrier level byte for loudness — the
   driver overwrites carrier TL from MIDI volume — so instrument balance
   lives in the modulator level (timbre) alone, and the mixer's velocity /
   channel-volume path plays the role DMX's SetVoiceVolume does. */
#define SYNTH_NORM_TARGET 31000

/* Drop leading dead air (slow OPL attacks sit below audibility for a long
   time); the sampled-OPL soundfonts we match against start loud. Keeps a
   few ms of ramp before the first sample above peak/8. Returns the new
   length; the two interpolation guard samples move along with the data. */
static uint32_t trim_onset(int16_t *d, uint32_t n) {
    int32_t peak = 0, thresh;
    uint32_t i, onset = 0, back;
    for (i = 0; i < n; i++) {
        int32_t a = d[i] < 0 ? -d[i] : d[i];
        if (a > peak) peak = a;
    }
    thresh = peak / 8;
    if (thresh < 1) return n;
    for (i = 0; i < n; i++) {
        int32_t a = d[i] < 0 ? -d[i] : d[i];
        if (a >= thresh) { onset = i; break; }
    }
    back = _WM_SampleRate / 333;   /* ~3 ms of natural ramp */
    onset = (onset > back) ? onset - back : 0;
    if (onset) {
        memmove(d, d + onset, ((size_t)n + 2 - onset) * sizeof(int16_t));
    }
    return n - onset;
}

static void configure(struct _sample *s, uint32_t n, double root_hz, int looped) {
    s->rate = _WM_SampleRate;
    /* freq_root/freq_low/freq_high are Hz * 1000 (gus_pat convention). */
    s->freq_root = (uint32_t)(root_hz * 1000.0);
    s->freq_low  = 0;
    s->freq_high = 0xFFFFFFFFu;
    /* Identity keyboard scaling (1 semitone per semitone). Left at 0/0,
       get_inc()'s GUS scale block collapses every note to note 0 (~8 Hz). */
    s->scale_frequency = 60;
    s->scale_factor    = 1024;
    s->loop_fraction = 0;
    s->inc_div = ((s->freq_root * 512u) / s->rate) * 2u;
    /* SAMPLE_SUSTAIN is required on held voices: without it the mixer's
       envelope falls through from the sustain plateau straight into release
       while the key is still down (wildmidi_lib.c, env stage 2 handling). */
    s->modes = SAMPLE_16BIT | SAMPLE_ENVELOPE
             | (looped ? (SAMPLE_LOOP | SAMPLE_SUSTAIN) : 0);
    s->data_length = n << 10;
    s->loop_start  = 0;
    s->loop_end    = n << 10;
    s->loop_size   = s->loop_end - s->loop_start;
    s->note_off_decay = _WM_SampleRate;
    s->next = NULL;
}

/* Render one looped, sustained tonal sample. `claimed_hz` is the pitch
   reported to the mixer; it differs from the rendered pitch when a GENMIDI
   voice carries a note offset. `chip` is caller-provided scratch: at ~20 KB+
   an opl3_chip is too big for the stack on the small-stack targets (DJGPP,
   OS/2, Amiga) this library still supports. */
static struct _sample *render_tonal(opl3_chip *chip,
                                    const fm_voice *v1, const fm_voice *v2,
                                    const mix_env *env, double claimed_hz) {
    double root_hz = opl_hz(v1->fnum, v1->block);
    double period, loop_target;
    uint32_t hold, hold_min;
    uint32_t loop_len_fp, loop_len_ref;
    struct _sample *s;
    uint8_t lfo;

    /* If any operator runs the chip LFO, the loop must cover a whole LFO
       cycle or the frozen wobble turns into a seam artifact. Tremolo (AM)
       is 13440 chip clocks, vibrato (VIB) 8192; without LFO ~40 ms of
       steady tone is plenty. */
    lfo = (uint8_t)((v1->fm->mod_chr | v1->fm->car_chr
                     | (v2 ? (v2->fm->mod_chr | v2->fm->car_chr) : 0)) & 0xC0);
    if (lfo & 0x80)      loop_target = 13440.0 / 49716.0;
    else if (lfo & 0x40) loop_target = 8192.0 / 49716.0;
    else                 loop_target = 0.04;

    /* The render window must clear the carrier's attack (slow-attack pads
       take seconds to become audible) before the loop region starts. */
    {
        float atk = opl_attack_seconds(v1->fm->car_ad >> 4);
        if (v2) {
            float a2 = opl_attack_seconds(v2->fm->car_ad >> 4);
            if (a2 > atk) atk = a2;
        }
        /* Loop shortly after the attack settles, before an EGT-clear voice
           decays deeply: the sampled-OPL soundfonts this mode is matched
           against loop near peak level the same way. */
        hold = (uint32_t)((atk + 0.25 + 1.5 * loop_target)
                          * (double)_WM_SampleRate);
    }
    hold_min = _WM_SampleRate * 3u / 10u;        /* >= 300 ms into sustain */
    if (hold < hold_min) hold = hold_min;

    s = alloc_sample(hold);
    if (!s) return NULL;

    opl_boot(chip, v1, v2);
    /* Render hold + the 2 guard samples as genuine continuation so the
       interpolator reads real data at the loop seam. */
    opl_render(chip, s->data, hold + 2);
    normalise_to(s->data, hold + 2, SYNTH_NORM_TARGET, 0);
    hold = trim_onset(s->data, hold);

    configure(s, hold, claimed_hz, 1);

    /* Loop an integer number of fundamental periods (in the mixer's <<10
       fixed point, so the fractional part of the period is preserved) taken
       from the end of the hold region, past the OPL attack/decay. The count
       is chosen to span ~loop_target seconds so any LFO cycle fits. */
    period = (double)_WM_SampleRate / root_hz;
    loop_len_ref = (uint32_t)(loop_target * (double)_WM_SampleRate / period + 0.5);
    if (loop_len_ref == 0) loop_len_ref = 1;
    loop_len_fp = (uint32_t)(period * (double)loop_len_ref * 1024.0);
    if (loop_len_fp >= (hold << 10)) loop_len_fp = (hold << 10) / 2;
    s->loop_end   = hold << 10;
    s->loop_start = (hold << 10) - loop_len_fp;
    s->loop_size  = loop_len_fp;

    set_envelope(s, env);
    return s;
}

/* Render a one-shot voice: key held for the whole buffer, the OPL envelope
   (EGT clear) decays naturally, tail faded to kill any residue. */
static struct _sample *render_oneshot(opl3_chip *chip,
                                      const fm_voice *v1, const fm_voice *v2,
                                      uint32_t n, double claimed_hz,
                                      float release) {
    struct _sample *s = alloc_sample(n);
    mix_env e;

    if (!s) return NULL;

    opl_boot(chip, v1, v2);
    opl_render(chip, s->data, n);
    normalise_to(s->data, n, SYNTH_NORM_TARGET, 128);

    configure(s, n, claimed_hz, 0);
    /* Hold the mixer envelope at peak so the sample's own baked-in decay is
       what the listener hears; note-off gets the voice's own release time. */
    e.sustain = 4194303;
    e.attack = 0.001f;
    e.decay = 0.5f;
    e.release = release;
    set_envelope(s, &e);
    return s;
}

/* Note-off release for a bank voice, from the carrier's release rate.
   opl_rate_seconds gives the exponential envelope's full 96 dB time; the
   mixer ramp is linear and stays audible for most of its length, so scale
   down to roughly the exponential's perceptual (-40 dB) point. */
static float voice_release(const fm_patch *fm) {
    float t = opl_rate_seconds(fm->car_sr & 0x0F) * 0.35f;
    if (t > 0.6f) t = 0.6f;
    if (t < 0.03f) t = 0.03f;
    return t;
}

/* One-shot length for a decaying bank voice: attack window plus the slower
   of the carrier's decay and release rates. */
static uint32_t voice_oneshot_len(const fm_patch *fm) {
    uint8_t d = fm->car_ad & 0x0F;
    uint8_t r = fm->car_sr & 0x0F;
    float t = opl_rate_seconds(d < r ? d : r);
    if (t > 1.5f) t = 1.5f;
    if (t < 0.15f) t = 0.15f;
    t += opl_attack_seconds(fm->car_ad >> 4) + 0.1f;
    return (uint32_t)(t * (float)_WM_SampleRate);
}


/* Measure the true fundamental of a rendered buffer by autocorrelation
   around the expected pitch. FM voices sound at a rational multiple of the
   channel frequency (carrier MULT=0 plays an octave down, mod/carrier
   multiplier GCDs lower it further), and banks bake this in as part of the
   instrument's tuning. Searching lags over [0.45x .. 2.2x] of the expected
   period catches those cases; the result is snapped to the nearest semitone
   of the expected pitch to shed measurement noise. Returns the ratio
   sounding_hz / expected_hz. */
static double measure_pitch_ratio(const int16_t *d, uint32_t start,
                                  uint32_t end, double expected_hz) {
    double period = (double)_WM_SampleRate / expected_hz;
    uint32_t min_lag = (uint32_t)(period * 0.45);
    uint32_t max_lag = (uint32_t)(period * 2.2);
    uint32_t span, lag, i, best_lag = 0;
    int64_t best = 0;
    int k, best_k = 0;
    double ratio, cand, best_err = 1e9;

    if (min_lag < 8) min_lag = 8;
    if (end <= start + max_lag * 2) return 1.0;
    span = end - start - max_lag;
    if (span > 4096) span = 4096;

    for (lag = min_lag; lag <= max_lag; lag++) {
        int64_t s = 0;
        for (i = 0; i < span; i += 2) {
            s += (int32_t)d[start + i] * (int32_t)d[start + i + lag];
        }
        if (s > best) { best = s; best_lag = lag; }
    }
    if (!best_lag) return 1.0;
    /* A dual-voice layer a whole octave below the winner leaves the true
       period at twice the measured lag: a strictly better correlation there
       means the composite repeats at 2x. Pure single-period tones correlate
       equally at both, so require a real margin before folding down. */
    if (start + span + best_lag * 2u <= end) {
        int64_t s2 = 0;
        for (i = 0; i < span; i += 2) {
            s2 += (int32_t)d[start + i] * (int32_t)d[start + i + best_lag * 2];
        }
        if (s2 > best + best / 50) best_lag *= 2;
    }
    ratio = period / (double)best_lag;   /* sounding / expected */

    /* snap to the nearest of 2^(k/12), k in [-14..14] */
    for (k = -14; k <= 14; k++) {
        double e;
        cand = semitone_ratio(k);
        e = ratio > cand ? ratio / cand : cand / ratio;
        if (e < best_err) { best_err = e; best_k = k; }
    }
    return semitone_ratio(best_k);
}

/* Multi-sample roots: same fnum at three octaves so FM timbre stays put
   instead of being pitch-stretched across the whole keyboard. Boundaries at
   the geometric midpoints between roots, in Hz * 1000. */
static const uint8_t tonal_blocks[3] = { 2, 4, 6 };

/* GENMIDI fine-tune: byte 2 of the record, 128 = no detune, applied to
   voice 2. Linear approximation of 2^(x/(12*64)) — accurate to <1% over the
   small detunes real banks use. */
static double op2_fine_ratio(uint8_t fine) {
    return 1.0 + ((int)fine - 128) * 0.000903;
}

struct _sample *_WM_synth_patch(uint16_t patchid) {
    /* Chip state is ~20 KB+: heap scratch, one per patch, shared by all
       renders below — never on the stack (DJGPP/OS2/Amiga budgets). */
    opl3_chip *chip = (opl3_chip *)malloc(sizeof(opl3_chip));
    if (!chip) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, errno);
        return NULL;
    }
    if (patchid & 0x80) {
        /* Drum one-shot. */
        uint8_t key = patchid & 0x7F;
        double key_hz = note_hz(key);
        struct _sample *s;

        if (key >= 35 && key <= 81) {
            /* GENMIDI percussion record: fixed note + voice note offsets. */
            const uint8_t *rec = op2_bank + (128 + (key - 35)) * OP2_RECSIZE;
            uint16_t flags = (uint16_t)(rec[0] | (rec[1] << 8));
            double fixed_hz = note_hz(rec[3] & 0x7F);
            fm_patch fm1, fm2;
            fm_voice v1, v2;
            int16_t off1, off2;
            op2_voice(rec + 4, &fm1, &off1);
            v1.fm = &fm1;
            /* DMX ignores base_note_offset on fixed-pitch records, and
               percussion records are fixed-pitch. */
            hz_to_fnum(fixed_hz * ((flags & OP2_FLAG_FIXED)
                                       ? 1.0 : semitone_ratio(off1)),
                       &v1.fnum, &v1.block);
            if (flags & OP2_FLAG_DOUBLE) {
                op2_voice(rec + 20, &fm2, &off2);
                v2.fm = &fm2;
                hz_to_fnum(fixed_hz * ((flags & OP2_FLAG_FIXED)
                                           ? 1.0 : semitone_ratio(off2))
                               * op2_fine_ratio(rec[2]),
                           &v2.fnum, &v2.block);
            }
            s = render_oneshot(chip, &v1, (flags & OP2_FLAG_DOUBLE) ? &v2 : NULL,
                               voice_oneshot_len(&fm1), key_hz,
                               voice_release(&fm1));
        } else {
            /* No GENMIDI record for this key; DMX skips it too. */
            free(chip);
            return NULL;
        }
        free(chip);
        if (!s) _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, errno);
        return s;
    } else {
        /* Tonal: chain three octave renders selected via freq_low/freq_high. */
        uint8_t program = patchid & 0x7F;
        const mix_env *env;
        mix_env op2_env;
        const uint16_t fnum = 345;   /* ~261.7 Hz at block 4 */
        const uint8_t *rec = NULL;
        uint16_t flags = 0;
        fm_patch fm1, fm2;
        fm_voice v1, v2;
        int16_t off1 = 0, off2 = 0;
        int doubled = 0, oneshot;
        struct _sample *chain = NULL, *tail = NULL;
        double roots[3], ratio, pitch_corr = 1.0;
        int i;

        rec = op2_bank + program * OP2_RECSIZE;
        flags = (uint16_t)(rec[0] | (rec[1] << 8));
        op2_voice(rec + 4, &fm1, &off1);
        if (flags & OP2_FLAG_DOUBLE) {
            op2_voice(rec + 20, &fm2, &off2);
            doubled = 1;
        }
        /* The bank voice carries its own attack in the render, so the
           mixer env just gates, releasing at the carrier's own rate. */
        op2_env.sustain = 4194303;
        op2_env.attack = 0.001f;
        op2_env.decay = 0.5f;
        op2_env.release = voice_release(&fm1);
        env = &op2_env;
        v1.fm = &fm1;
        v2.fm = &fm2;
        if (flags & OP2_FLAG_FIXED) {
            /* Fixed-pitch melodic instrument (helicopter, applause): DMX
               plays the record's fixed note for every key. Best wavetable
               approximation: pin the pitch so middle C plays it exactly. */
            ratio = note_hz(rec[3] & 0x7F) / note_hz(60);
        } else {
            /* base_note_offset is NOT applied to the sounding pitch: DMX
               shifts its register writes by it, but the sampled-OPL
               soundfonts we match play notes at written pitch (the offset
               only survives inside voice 2's relative tuning below).
               Applying it plays whole songs an octave off. */
            ratio = 1.0;
        }

        /* A voice is a natural one-shot only when its OPL envelope really
           reaches silence quickly: no held sustain (EGT clear, or sustain
           level at the -45 dB floor) and a fast carrier decay. Everything
           else is looped and held while the key is down — matching how
           sampled-OPL soundfonts loop long-ringing instruments and only
           one-shot fast decayers (xylophone yes, vibraphone no). */
        {
            uint8_t d = fm1.car_ad & 0x0F, r = fm1.car_sr & 0x0F;
            int decays_out = !(fm1.car_chr & 0x20) || (fm1.car_sr >> 4) == 0x0F;
            oneshot = decays_out
                      && opl_rate_seconds(d < r ? d : r) <= 0.8f;
        }
        if (!oneshot) {
            fm1.mod_chr |= 0x20; fm1.mod_sr &= 0x0F;
            fm1.car_chr |= 0x20; fm1.car_sr &= 0x0F;
            if (doubled) {
                fm2.mod_chr |= 0x20; fm2.mod_sr &= 0x0F;
                fm2.car_chr |= 0x20; fm2.car_sr &= 0x0F;
            }
        }

        for (i = 0; i < 3; i++) {
            double base_hz = opl_hz(fnum, tonal_blocks[i]);
            double claimed = base_hz / ratio;
            struct _sample *s;
            v1.fnum = fnum;
            v1.block = tonal_blocks[i];
            if (doubled) {
                /* Voice 2 keeps its offset/detune relative to voice 1
                   (offsets are ignored entirely on fixed-pitch records).
                   The relative offset is octave-reduced: the sampled-OPL
                   soundfonts we match carry no sub-octave layer, and a
                   full-octave drop would drag the perceived pitch down. */
                int rel_off = off2 - off1;
                double rel;
                while (rel_off <= -12) rel_off += 12;
                while (rel_off >= 12) rel_off -= 12;
                rel = (flags & OP2_FLAG_FIXED) ? 1.0 : semitone_ratio(rel_off);
                hz_to_fnum(base_hz * rel * op2_fine_ratio(rec[2]),
                           &v2.fnum, &v2.block);
            }
            s = oneshot
                ? render_oneshot(chip, &v1, doubled ? &v2 : NULL,
                                 voice_oneshot_len(&fm1), claimed,
                                 voice_release(&fm1))
                : render_tonal(chip, &v1, doubled ? &v2 : NULL, env, claimed);
            if (!s) {
                free(chip);
                free_sample_chain(chain);
                _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, errno);
                return NULL;
            }
            {
                /* Calibrate each octave render: where does this voice
                   actually sound relative to the channel frequency?
                   Operator KSL shifts the sideband balance per block, so
                   the answer can differ between the three roots. Measure
                   the late plateau, not the onset — layered components
                   decay at different rates and only the steady-state
                   balance is what a held note sounds like. */
                uint32_t n0 = s->data_length >> 10;
                uint32_t st = oneshot ? n0 / 6
                                      : (n0 > 12000 ? n0 - 9000 : n0 / 3);
                pitch_corr = measure_pitch_ratio(s->data, st,
                                                 oneshot ? n0 / 2 : n0,
                                                 base_hz);
            }
            /* Re-claim the root at the measured sounding pitch so written
               notes play at written pitch regardless of how the FM voice
               reaches its octave (carrier MULT, multiplier GCDs, ...). */
            s->freq_root = (uint32_t)(claimed * pitch_corr * 1000.0);
            s->inc_div = ((s->freq_root * 512u) / s->rate) * 2u;
            roots[i] = claimed * pitch_corr;
            if (tail) tail->next = s; else chain = s;
            tail = s;
        }
        /* Range boundaries at geometric midpoints between adjacent roots
           (roots are two octaves apart, so the midpoint is root x 2). */
        {
            struct _sample *s0 = chain, *s1 = chain->next, *s2 = s1->next;
            uint32_t b01 = (uint32_t)(2.0 * roots[0] * 1000.0);
            uint32_t b12 = (uint32_t)(2.0 * roots[1] * 1000.0);
            s0->freq_low = 0;            s0->freq_high = b01;
            s1->freq_low = b01;          s1->freq_high = b12;
            s2->freq_low = b12;          s2->freq_high = 0xFFFFFFFFu;
        }
        free(chip);
        return chain;
    }
}

/* ------------------------------------------------------------------ */
/* Patch table init                                                   */
/* ------------------------------------------------------------------ */

static struct _patch *alloc_patch(uint16_t patchid, uint8_t keep) {
    struct _patch *p = (struct _patch *)calloc(1, sizeof(struct _patch));
    if (!p) return NULL;
    p->patchid = patchid;
    p->amp = 1024;
    p->keep = keep;
    return p;
}

static void free_all_patches(void) {
    uint16_t id;
    for (id = 0; id < 128; id++) {
        struct _patch *p = _WM_patch[id];
        while (p) {
            struct _patch *next = p->next;
            free(p);
            p = next;
        }
        _WM_patch[id] = NULL;
    }
}

int _WM_opl3_init_patches(void) {
    uint16_t id;

    /* No external .op2 bank loaded: install the embedded DMXOPL bank
       (MIT, see include/synth_bank.h) so --opl3 needs no data files. */
    if (!op2_active) {
        memcpy(op2_bank, synth_builtin_bank, sizeof(op2_bank));
        op2_active = 1;
    }

    for (id = 0; id < 128; id++) {
        struct _patch *p = alloc_patch(id, 0);
        if (!p) { free_all_patches(); return -1; }
        _WM_patch[id] = p;
    }
    /* Drum kit chains onto _WM_patch[note & 0x7F] because _find_matched_patch
       keys off patchid&0x7F. keep=SAMPLE_ENVELOPE prevents _WM_load_sample
       from stripping envelope mode on drums (see sample.c). */
    for (id = 35; id <= 81; id++) {   /* GENMIDI percussion key range */
        uint16_t drumid = 0x80u | id;
        struct _patch *p = alloc_patch(drumid, SAMPLE_ENVELOPE);
        struct _patch *head = _WM_patch[id];
        if (!p) { free_all_patches(); return -1; }
        while (head->next) head = head->next;
        head->next = p;
    }
    return 0;
}
