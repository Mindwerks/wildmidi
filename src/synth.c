/*
 * synth.c -- Built-in synthesiser for WildMIDI's emergency soundbank.
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
 * All code below is original clean-room work: no lift from Gervill's
 * EmergencySoundbank (GPL2), csharpsynth, or any other bank. The techniques
 * (additive synthesis, filtered noise, pitched sine sweeps for kicks,
 * inharmonic partials for cymbals) are textbook.
 */

#include "config.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "wm_error.h"
#include "patches.h"
#include "sample.h"
#include "synth.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern uint16_t _WM_SampleRate;

/* ------------------------------------------------------------------ */
/* Envelopes                                                          */
/* ------------------------------------------------------------------ */

static int32_t env_rate_for_seconds(float seconds) {
    if (seconds <= 0.0f) seconds = 0.001f;
    return (int32_t)(4194303.0f / ((float)_WM_SampleRate * seconds));
}

/* Fill the 6+1 mixer envelope stages. 0..3 run while note is held,
   4..6 after note-off. peak/sustain are in mixer fixed-point (4194303 = 1.0). */
static void set_envelope(struct _sample *s, int32_t sustain,
                        float attack_s, float decay_s, float release_s) {
    const int32_t peak = 4194303;
    s->env_target[0] = peak;    s->env_rate[0] = env_rate_for_seconds(attack_s);
    s->env_target[1] = sustain; s->env_rate[1] = env_rate_for_seconds(decay_s);
    s->env_target[2] = sustain; s->env_rate[2] = env_rate_for_seconds(0.5f);
    s->env_target[3] = sustain; s->env_rate[3] = env_rate_for_seconds(4.0f);
    s->env_target[4] = 0;       s->env_rate[4] = env_rate_for_seconds(release_s);
    s->env_target[5] = 0;       s->env_rate[5] = env_rate_for_seconds(release_s);
    s->env_target[6] = 0;       s->env_rate[6] = env_rate_for_seconds(0.010f);
}

/* ------------------------------------------------------------------ */
/* Per-family harmonic profiles                                       */
/* ------------------------------------------------------------------ */

/* Eight amplitudes: h[k] weights partial k+1. Values live in [0,1];
   normalisation happens after summing to keep the peak at ~28000. */
typedef struct {
    float h[8];
    int32_t sustain;
    float attack, decay, release;
} tonal_voice;

/* Sixteen GM sound groups (program >> 3). Written by ear for family distinction. */
static const tonal_voice gm_voice[16] = {
    /* 0  Piano       */ {{1.0f,0.60f,0.40f,0.30f,0.25f,0.20f,0.15f,0.10f}, 1258291, 0.003f, 0.400f, 0.180f},
    /* 1  Chrom.perc  */ {{1.0f,0.30f,0.50f,0.20f,0.30f,0.15f,0.20f,0.10f},  838860, 0.002f, 0.300f, 0.120f},
    /* 2  Organ       */ {{1.0f,0.90f,0.50f,0.70f,0.30f,0.40f,0.20f,0.50f}, 4194303, 0.012f, 0.080f, 0.100f},
    /* 3  Guitar      */ {{1.0f,0.70f,0.50f,0.40f,0.30f,0.25f,0.20f,0.15f}, 1677721, 0.003f, 0.500f, 0.180f},
    /* 4  Bass        */ {{1.0f,0.80f,0.50f,0.35f,0.25f,0.18f,0.12f,0.08f}, 2097151, 0.004f, 0.400f, 0.160f},
    /* 5  Strings     */ {{1.0f,0.80f,0.60f,0.50f,0.40f,0.30f,0.25f,0.20f}, 3355443, 0.060f, 0.200f, 0.250f},
    /* 6  Ensemble    */ {{1.0f,0.85f,0.65f,0.55f,0.45f,0.35f,0.28f,0.22f}, 3355443, 0.050f, 0.200f, 0.300f},
    /* 7  Brass       */ {{0.60f,1.00f,0.90f,0.80f,0.70f,0.50f,0.40f,0.30f}, 3145727, 0.030f, 0.150f, 0.150f},
    /* 8  Reed        */ {{1.0f,0.20f,0.70f,0.20f,0.50f,0.15f,0.40f,0.10f}, 3355443, 0.025f, 0.150f, 0.120f},
    /* 9  Pipe        */ {{1.0f,0.15f,0.10f,0.05f,0.02f,0.00f,0.00f,0.00f}, 3355443, 0.030f, 0.150f, 0.140f},
    /* 10 Synth Lead  */ {{1.0f,0.50f,0.33f,0.25f,0.20f,0.17f,0.14f,0.13f}, 3355443, 0.008f, 0.100f, 0.100f},
    /* 11 Synth Pad   */ {{1.0f,0.50f,0.30f,0.20f,0.10f,0.05f,0.02f,0.01f}, 3355443, 0.150f, 0.400f, 0.400f},
    /* 12 Synth FX    */ {{1.0f,0.40f,0.60f,0.30f,0.40f,0.20f,0.30f,0.15f}, 2097151, 0.020f, 0.300f, 0.250f},
    /* 13 Ethnic      */ {{1.0f,0.55f,0.45f,0.35f,0.25f,0.20f,0.15f,0.10f}, 1677721, 0.005f, 0.400f, 0.180f},
    /* 14 Percussive  */ {{1.0f,0.30f,0.45f,0.20f,0.25f,0.15f,0.15f,0.10f},  838860, 0.002f, 0.300f, 0.120f},
    /* 15 SFX         */ {{1.0f,0.40f,0.35f,0.25f,0.20f,0.15f,0.10f,0.08f}, 1677721, 0.005f, 0.400f, 0.200f}
};

/* ------------------------------------------------------------------ */
/* Sample construction helpers                                        */
/* ------------------------------------------------------------------ */

static struct _sample *alloc_sample(uint32_t n) {
    struct _sample *s = (struct _sample *)calloc(1, sizeof(struct _sample));
    if (!s) return NULL;
    s->data = (int16_t *)calloc((size_t)n + 2, sizeof(int16_t));
    if (!s->data) { free(s); return NULL; }
    return s;
}

/* Populate the mixer-facing fields of a _sample. root_hz is the pitch of the
   raw buffer when played at _WM_SampleRate; the mixer pitch-shifts around it. */
static void configure(struct _sample *s, uint32_t n, double root_hz, int looped) {
    s->rate = _WM_SampleRate;
    /* freq_root in GUS patches is Hz * 1000: derived from gus_pat's inc_div
       formula ((freq_root*512)/rate)*2. Larger values silently overflow uint32
       in freq_root*512, producing pitch-per-note inconsistency. */
    s->freq_root = (uint32_t)(root_hz * 1000.0);
    s->freq_low  = 0;
    s->freq_high = 0xFFFFFFFFu;
    s->loop_fraction = 0;
    s->inc_div = ((s->freq_root * 512u) / s->rate) * 2u;
    s->modes = SAMPLE_16BIT | SAMPLE_ENVELOPE | (looped ? SAMPLE_LOOP : 0);
    s->data_length = n << 10;
    s->loop_start  = 0;
    s->loop_end    = n << 10;
    s->loop_size   = s->loop_end - s->loop_start;
    s->note_off_decay = _WM_SampleRate;
    s->next = NULL;
}

/* Normalise a float buffer to [-target, +target] and write it as int16.
   `tail_fade` linearly fades the last N samples to zero so non-looped one-shots
   don't click when the mixer cuts off at data_length. */
static void normalise_and_write(struct _sample *s, uint32_t n, float *buf,
                                float target, uint32_t tail_fade) {
    float peak = 0.0f;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak) peak = a;
    }
    if (peak < 1e-6f) peak = 1.0f;
    if (tail_fade > n) tail_fade = n;
    for (i = 0; i < n; i++) {
        float v = buf[i] * target / peak;
        if (tail_fade && i >= n - tail_fade) {
            v *= (float)(n - 1 - i) / (float)tail_fade;
        }
        s->data[i] = (int16_t)v;
    }
    /* gus_pat-style guard samples for the mixer's interpolator. */
    s->data[n]     = s->data[0];
    s->data[n + 1] = s->data[1 % n];
}

/* ------------------------------------------------------------------ */
/* Tonal synthesis (additive harmonics per family)                    */
/* ------------------------------------------------------------------ */

static struct _sample *synth_tonal(uint16_t patchid) {
    uint8_t program = patchid & 0x7F;
    const tonal_voice *v = &gm_voice[program >> 3];
    /* One fundamental period: loop covers exactly one cycle. All partials
       are integer multiples of the fundamental so the boundary is bit-exact. */
    uint32_t period = _WM_SampleRate / 262u;
    if (period < 32) period = 32;
    uint32_t n = period;
    double root_hz = (double)_WM_SampleRate / (double)period;
    struct _sample *s;
    float *buf;
    uint32_t i;
    int k;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        double phase = 2.0 * M_PI * (double)i / (double)period;
        float acc = 0.0f;
        for (k = 0; k < 8; k++) {
            if (v->h[k] != 0.0f) {
                acc += v->h[k] * (float)sin(phase * (double)(k + 1));
            }
        }
        buf[i] = acc;
    }
    /* Tonal is looped; loop_start == data[0] == 0 so no tail fade needed. */
    normalise_and_write(s, n, buf, 28000.0f, 0);
    free(buf);

    configure(s, n, root_hz, 1);
    set_envelope(s, v->sustain, v->attack, v->decay, v->release);
    return s;
}

/* ------------------------------------------------------------------ */
/* Drum synthesis                                                     */
/* ------------------------------------------------------------------ */

typedef struct { uint32_t s; } rng_t;
static float rng_bipolar(rng_t *r) {
    r->s = r->s * 1103515245u + 12345u;
    return ((int32_t)(r->s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

/* Kick / tom: rapid pitch drop from f_start Hz to f_end Hz over the buffer,
   plus a short attack click. Toms use the same generator, higher pitch. */
static struct _sample *synth_kick(uint8_t note) {
    uint32_t n = _WM_SampleRate * 3u / 10u;   /* 300 ms */
    struct _sample *s;
    float *buf;
    uint32_t i;
    float f_start = (note <= 36) ? 150.0f : 120.0f + 6.0f * (float)note;
    float f_end   = (note <= 36) ? 45.0f  : 60.0f + 3.5f * (float)note;
    float click   = (note <= 36) ? 1.0f   : 0.35f;
    double phase = 0.0;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float f = f_end + (f_start - f_end) * expf(-6.0f * t);
        phase += 2.0 * M_PI * (double)f / (double)_WM_SampleRate;
        float body = sinf((float)phase);
        float amp = expf(-4.5f * t);
        float attack_click = (i < 30) ? click * (1.0f - (float)i / 30.0f) : 0.0f;
        buf[i] = amp * body + attack_click;
    }
    normalise_and_write(s, n, buf, 30000.0f, 128);
    free(buf);

    configure(s, n, 100.0, 0);
    set_envelope(s, 0, 0.001f, 0.5f, 0.5f);
    return s;
}

/* Snare: mid-freq body + band-limited noise, both decaying quickly. */
static struct _sample *synth_snare(uint8_t note) {
    uint32_t n = _WM_SampleRate * 2u / 10u;    /* 200 ms */
    struct _sample *s;
    float *buf;
    uint32_t i;
    rng_t r = { 0xC0FFEEu ^ ((uint32_t)note * 2654435761u) };
    double phase = 0.0;
    float tone_freq = (note == 40 || note == 38) ? 180.0f : 220.0f;
    float lpf = 0.0f;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        phase += 2.0 * M_PI * (double)tone_freq / (double)_WM_SampleRate;
        float body = sinf((float)phase) * expf(-6.0f * t);
        float x = rng_bipolar(&r);
        lpf += 0.45f * (x - lpf);
        buf[i] = 0.55f * body + 0.9f * lpf * expf(-4.0f * t);
    }
    normalise_and_write(s, n, buf, 28000.0f, 128);
    free(buf);

    configure(s, n, 200.0, 0);
    set_envelope(s, 0, 0.001f, 0.15f, 0.15f);
    return s;
}

/* Hi-hat / shaker / clap: HP-flavoured noise. `open` selects longer decay. */
static struct _sample *synth_hat(uint8_t note, int open) {
    uint32_t n = open ? (_WM_SampleRate * 3u / 10u) : (_WM_SampleRate / 15u);
    struct _sample *s;
    float *buf;
    uint32_t i;
    rng_t r = { 0xBADF00Du ^ ((uint32_t)note * 2654435761u) };
    float lpf = 0.0f;
    float decay_k = open ? 4.0f : 25.0f;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float x = rng_bipolar(&r);
        lpf += 0.15f * (x - lpf);
        float hp = x - lpf;
        buf[i] = hp * expf(-decay_k * t);
    }
    normalise_and_write(s, n, buf, 22000.0f, 128);
    free(buf);

    configure(s, n, 8000.0, 0);
    set_envelope(s, 0, 0.001f, open ? 0.25f : 0.06f, open ? 0.15f : 0.05f);
    return s;
}

/* Cymbal / crash / ride: three inharmonic partials + broadband noise. */
static struct _sample *synth_cymbal(uint8_t note) {
    uint32_t n = _WM_SampleRate * 3u / 5u;     /* 600 ms */
    struct _sample *s;
    float *buf;
    uint32_t i;
    rng_t r = { 0xCA55EEu ^ ((uint32_t)note * 2654435761u) };
    const float ratios[3] = { 1.0f, 1.593f, 2.135f };
    const float base = (note == 51 || note == 53 || note == 59) ? 700.0f : 500.0f;
    double ph[3] = { 0.0, 0.0, 0.0 };
    float noise_decay = (note == 49 || note == 57) ? 2.5f : 4.5f;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        int k;
        float partials = 0.0f;
        for (k = 0; k < 3; k++) {
            ph[k] += 2.0 * M_PI * (double)(base * ratios[k]) / (double)_WM_SampleRate;
            partials += sinf((float)ph[k]);
        }
        float x = rng_bipolar(&r);
        buf[i] = 0.5f * partials * expf(-2.0f * t) + 0.7f * x * expf(-noise_decay * t);
    }
    normalise_and_write(s, n, buf, 26000.0f, 128);
    free(buf);

    configure(s, n, 800.0, 0);
    set_envelope(s, 0, 0.001f, 0.5f, 0.4f);
    return s;
}

/* Bell-like: fundamental + inharmonic partial. Cowbell / agogo / triangle. */
static struct _sample *synth_bell(uint8_t note, float base_hz, float decay_s) {
    uint32_t n = (uint32_t)((float)_WM_SampleRate * (decay_s + 0.1f));
    struct _sample *s;
    float *buf;
    uint32_t i;
    double ph1 = 0.0, ph2 = 0.0;
    (void)note;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        ph1 += 2.0 * M_PI * (double)base_hz / (double)_WM_SampleRate;
        ph2 += 2.0 * M_PI * (double)(base_hz * 2.76f) / (double)_WM_SampleRate;
        buf[i] = (0.7f * sinf((float)ph1) + 0.3f * sinf((float)ph2)) * expf(-3.0f / decay_s * t);
    }
    normalise_and_write(s, n, buf, 26000.0f, 128);
    free(buf);

    configure(s, n, (double)base_hz, 0);
    set_envelope(s, 0, 0.001f, decay_s * 0.7f, decay_s * 0.5f);
    return s;
}

/* Short pitched click for wood-block / claves / sticks / bongo / conga. */
static struct _sample *synth_click(uint8_t note, float freq, float decay_s) {
    uint32_t n = (uint32_t)((float)_WM_SampleRate * (decay_s + 0.03f));
    struct _sample *s;
    float *buf;
    uint32_t i;
    double phase = 0.0;
    (void)note;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }

    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        phase += 2.0 * M_PI * (double)freq / (double)_WM_SampleRate;
        buf[i] = sinf((float)phase) * expf(-6.0f / decay_s * t);
    }
    normalise_and_write(s, n, buf, 22000.0f, 128);
    free(buf);

    configure(s, n, (double)freq, 0);
    set_envelope(s, 0, 0.001f, decay_s * 0.7f, decay_s * 0.5f);
    return s;
}

/* Fallback for any drum key not otherwise handled. */
static struct _sample *synth_drum_generic(uint8_t note) {
    uint32_t n = _WM_SampleRate / 6u;
    struct _sample *s;
    float *buf;
    uint32_t i;
    rng_t r = { 0xDEADBEEFu ^ ((uint32_t)note * 2654435761u) };
    float lpf = 0.0f;
    if (!(s = alloc_sample(n))) return NULL;
    buf = (float *)calloc(n, sizeof(float));
    if (!buf) { free(s->data); free(s); return NULL; }
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float x = rng_bipolar(&r);
        lpf += 0.4f * (x - lpf);
        buf[i] = lpf * expf(-6.0f * t);
    }
    normalise_and_write(s, n, buf, 22000.0f, 128);
    free(buf);
    configure(s, n, 300.0, 0);
    set_envelope(s, 0, 0.001f, 0.10f, 0.08f);
    return s;
}

/* Dispatch by GM percussion key (General MIDI Level 1 chart). */
static struct _sample *synth_drum(uint16_t patchid) {
    uint8_t k = patchid & 0x7F;
    switch (k) {
    case 35: case 36:                                         /* kicks */
    case 41: case 43: case 45: case 47: case 48: case 50:     /* toms  */
        return synth_kick(k);
    case 37:                                                  /* side stick */
        return synth_click(k, 800.0f, 0.05f);
    case 38: case 40:                                         /* snares */
        return synth_snare(k);
    case 39:                                                  /* hand clap */
    case 42: case 44:                                         /* closed hat / pedal hat */
    case 54:                                                  /* tambourine */
    case 69: case 70: case 82:                                /* cabasa / maracas / shaker */
        return synth_hat(k, 0);
    case 46:                                                  /* open hat */
    case 74:                                                  /* long guiro */
        return synth_hat(k, 1);
    case 49: case 51: case 52: case 53: case 55: case 57: case 59:  /* crashes / rides / china / splash */
        return synth_cymbal(k);
    case 56:                                                  /* cowbell */
        return synth_bell(k, 540.0f, 0.20f);
    case 60: case 61:                                         /* bongos */
        return synth_click(k, k == 60 ? 900.0f : 600.0f, 0.15f);
    case 62: case 63: case 64:                                /* congas */
        return synth_click(k, k == 64 ? 220.0f : 350.0f, 0.20f);
    case 65: case 66:                                         /* timbales */
        return synth_click(k, k == 65 ? 500.0f : 380.0f, 0.15f);
    case 67: case 68:                                         /* agogos */
        return synth_bell(k, k == 67 ? 850.0f : 700.0f, 0.20f);
    case 73: case 75:                                         /* short guiro / claves */
        return synth_click(k, 2500.0f, 0.04f);
    case 76: case 77:                                         /* wood blocks */
        return synth_click(k, k == 76 ? 1200.0f : 850.0f, 0.08f);
    case 80: case 81:                                         /* mute / open triangle */
        return synth_bell(k, 5000.0f, k == 81 ? 0.60f : 0.10f);
    default:
        return synth_drum_generic(k);
    }
}

/* ------------------------------------------------------------------ */
/* Public entry points                                                */
/* ------------------------------------------------------------------ */

struct _sample *_WM_synth_patch(uint16_t patchid) {
    struct _sample *s = (patchid & 0x80) ? synth_drum(patchid)
                                         : synth_tonal(patchid);
    if (!s) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, errno);
    }
    return s;
}

static struct _patch *alloc_patch(uint16_t patchid, uint8_t keep) {
    struct _patch *p = (struct _patch *)calloc(1, sizeof(struct _patch));
    if (!p) return NULL;
    p->patchid = patchid;
    p->amp = 1024;
    p->keep = keep;
    return p;
}

int _WM_emergency_init_patches(void) {
    uint16_t id;

    for (id = 0; id < 128; id++) {
        struct _patch *p = alloc_patch(id, 0);
        if (!p) return -1;
        _WM_patch[id] = p;
    }
    /* Drum kit chains onto _WM_patch[note & 0x7F] because _find_matched_patch
       keys off patchid&0x7F. keep=SAMPLE_ENVELOPE prevents _WM_load_sample from
       stripping envelope mode on drums (see sample.c). */
    for (id = 27; id <= 87; id++) {
        uint16_t drumid = 0x80u | id;
        struct _patch *p = alloc_patch(drumid, SAMPLE_ENVELOPE);
        struct _patch *head = _WM_patch[id];
        if (!p) return -1;
        while (head->next) head = head->next;
        head->next = p;
    }
    return 0;
}
