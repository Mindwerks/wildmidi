/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ma_fm_core.c -- the FM DSP: phase generator, exponential ADSR, operators,
 *                 2-op/4-op algorithms, and a compact built-in GM patch bank.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  See NOTICE and
 * docs/SMAF_FM.md.
 *
 * The chip did all of this in fixed point with log/exp lookup ROMs; we do it in
 * double.  What matters is that the envelope shape and the operator algebra
 * match -- FM timbre lives in the modulator/carrier ratio and the attack/decay
 * curve, not in the mantissa.
 */

#include "mafm/ma_fm_core.h"

#include <math.h>
#include <string.h>

#include "common.h"      /* M_PI, M_LN2 */

#define MAFM_TWO_PI (2.0 * M_PI)

/* base-2 log via natural log (log2 is C99+; keep this C89-portable) */
static double mafm_log2(double x) {
    return log(x) / M_LN2;
}

static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* total-level (attenuation) -> linear gain.  0.75 dB per TL unit, 63 ~ silence. */
static double tl_to_gain(uint8_t tl) {
    if (tl >= 63) return 0.0;
    return pow(10.0, (-0.75 * (double) tl) / 20.0);
}

/* a chip "rate" (0..15) mapped to a per-sample linear envelope step. */
static double rate_to_step(uint8_t rate, double sample_rate, int attack) {
    double fast, slow, t, samples;
    if (rate == 0) {
        /* rate 0: attack completes instantly (caller floors it); a zero-step
         * decay/sustain/release would freeze the level and never retire, so
         * floor it to a slow ~20 s glide. */
        return attack ? 0.0 : (1.0 / (sample_rate * 20.0));
    }
    fast = attack ? 0.0008 : 0.004;   /* rate 15 */
    slow = attack ? 0.35   : 4.0;     /* rate 1  */
    t = slow * pow(fast / slow, ((double) rate - 1.0) / 14.0);
    samples = t * sample_rate;
    if (samples < 1.0) samples = 1.0;
    return 1.0 / samples;
}

/* ── patches ────────────────────────────────────────────────────────────── */

void _WM_MAFM_DefaultPatch(struct mafm_voice_patch *v) {
    struct mafm_op_patch *op0, *op1;
    memset(v, 0, sizeof(*v));
    v->four_op = 0;
    v->algorithm = 0;      /* op0 modulates op1 (carrier) */
    v->feedback = 0;
    v->note_shift = 0;
    v->pan_default = 0.0f;
    v->lfo = 0;

    op0 = &v->ops[0];
    op0->multi = 2; op0->tl = 20; op0->ar = 15; op0->dr = 6; op0->sr = 2;
    op0->rr = 7; op0->sl = 4; op0->eg_type = 1;

    op1 = &v->ops[1];
    op1->multi = 1; op1->tl = 0; op1->ar = 15; op1->dr = 4; op1->sr = 1;
    op1->rr = 7; op1->sl = 2; op1->eg_type = 1;
}

void _WM_MAFM_GmApprox(int program, struct mafm_voice_patch *out) {
    int fam;
    _WM_MAFM_DefaultPatch(out);
    if (program < 0) program = 0;
    if (program > 127) program = 127;
    fam = program / 8;                 /* 16 GM families */
    switch (fam) {
    case 0:  out->ops[0].multi=1; out->ops[0].dr=7; break;      /* pianos */
    case 1:  out->ops[0].multi=2; out->ops[0].tl=14; break;     /* chromatic perc */
    case 2:  out->ops[0].multi=1; out->ops[1].sr=0; out->ops[0].dr=2; break; /* organs */
    case 3:  out->ops[0].multi=1; out->ops[0].dr=5; break;      /* guitars */
    case 4:  out->ops[0].multi=2; out->ops[0].dr=4; break;      /* basses */
    case 5:  out->ops[0].multi=1; out->ops[1].sr=0; out->ops[0].dr=1; break; /* strings */
    case 6:  out->ops[0].multi=1; out->ops[1].sr=0; break;      /* ensemble */
    case 7:  out->ops[0].multi=3; out->ops[1].sr=0; break;      /* brass */
    case 8:  out->ops[0].multi=1; out->ops[1].sr=0; out->ops[0].tl=26; break; /* reed */
    case 9:  out->ops[0].multi=1; out->ops[1].sr=0; out->ops[0].tl=28; break; /* pipe */
    case 10: out->ops[0].multi=4; out->ops[0].wave=1; break;    /* synth lead */
    case 11: out->ops[0].multi=1; out->ops[0].wave=2; out->ops[1].sr=0; break; /* synth pad */
    case 12: out->ops[0].multi=6; out->ops[0].wave=3; break;    /* synth fx */
    case 13: out->ops[0].multi=2; out->ops[0].dr=6; break;      /* ethnic */
    case 14: out->ops[0].multi=8; out->ops[0].dr=10; out->ops[0].wave=1; break; /* percussive */
    default: out->ops[0].multi=12; out->ops[0].wave=4; out->ops[0].dr=12; break; /* sfx */
    }
}

void _WM_MAFM_DrumApprox(int note, struct mafm_voice_patch *out) {
    /* three short percussive hits, all eg_type=0 so they retire on their own. */
    struct mafm_op_patch *op0, *op1;
    _WM_MAFM_DefaultPatch(out);
    op0 = &out->ops[0];
    op1 = &out->ops[1];
    if (note < 44) {                   /* kick-ish: sub carrier, fast thump */
        memset(op0, 0, sizeof(*op0)); memset(op1, 0, sizeof(*op1));
        op0->multi=0; op0->tl=8;  op0->ar=15; op0->dr=10; op0->rr=12; op0->sl=15;
        op1->multi=0; op1->tl=0;  op1->ar=15; op1->dr=9;  op1->rr=12; op1->sl=15; op1->fb=5;
    } else if (note < 52) {            /* snare-ish: mid noise burst */
        memset(op0, 0, sizeof(*op0)); memset(op1, 0, sizeof(*op1));
        op0->multi=11; op0->tl=4; op0->ar=15; op0->dr=9;  op0->rr=11; op0->sl=15; op0->fb=7;
        op1->multi=1;  op1->tl=2; op1->ar=15; op1->dr=8;  op1->rr=11; op1->sl=15;
    } else {                           /* hat-ish: short metallic tick */
        memset(op0, 0, sizeof(*op0)); memset(op1, 0, sizeof(*op1));
        op0->multi=15; op0->tl=6; op0->ar=15; op0->dr=12; op0->rr=13; op0->sl=15; op0->fb=7;
        op1->multi=9;  op1->tl=8; op1->ar=15; op1->dr=12; op1->rr=13; op1->sl=15;
    }
}

/* ── envelope ───────────────────────────────────────────────────────────── */

static void env_configure(struct mafm_env *e, const struct mafm_op_patch *p,
                          double sample_rate, double rate_scale) {
    e->atk_step = rate_to_step(p->ar, sample_rate, 1) * rate_scale;
    e->dec_step = rate_to_step(p->dr, sample_rate, 0) * rate_scale;
    e->sus_step = rate_to_step(p->sr, sample_rate, 0) * rate_scale;
    e->rel_step = rate_to_step(p->rr, sample_rate, 0) * rate_scale;
    e->sus_level = 1.0 - ((double) p->sl / 15.0);   /* sl 0 = top, 15 = ~silent */
    e->sustaining = p->eg_type;
    e->xof = p->xof;
    e->phase = MAFM_EG_IDLE;
    e->level = 0.0;
}

static void env_key_on(struct mafm_env *e) { e->phase = MAFM_EG_ATTACK; }

static void env_key_off(struct mafm_env *e) {
    /* XOF: ignore key-off and ring out on the envelope; only for percussive
     * shapes (a sustaining XOF voice would never retire). */
    if (e->xof && !e->sustaining) return;
    if (e->phase != MAFM_EG_IDLE) e->phase = MAFM_EG_RELEASE;
}

static float env_advance(struct mafm_env *e) {
    switch (e->phase) {
    case MAFM_EG_IDLE:
        return 0.0f;
    case MAFM_EG_ATTACK:
        e->level += (e->atk_step <= 0.0 ? 1.0 : e->atk_step);
        if (e->level >= 1.0) { e->level = 1.0; e->phase = MAFM_EG_DECAY; }
        break;
    case MAFM_EG_DECAY:
        e->level -= e->dec_step;
        if (e->level <= e->sus_level) {
            e->level = e->sus_level;
            e->phase = e->sustaining ? MAFM_EG_SUSTAIN : MAFM_EG_RELEASE;
        }
        break;
    case MAFM_EG_SUSTAIN:
        e->level -= e->sus_step;   /* second-decay: slow bleed to silence */
        if (e->level <= 0.0) { e->level = 0.0; e->phase = MAFM_EG_IDLE; }
        break;
    case MAFM_EG_RELEASE:
        e->level -= e->rel_step;
        if (e->level <= 0.0) { e->level = 0.0; e->phase = MAFM_EG_IDLE; }
        break;
    }
    return (float) e->level;
}

static int env_finished(const struct mafm_env *e) {
    return e->phase == MAFM_EG_IDLE;
}

/* ── operator ───────────────────────────────────────────────────────────── */

static void op_recalc(struct mafm_operator *o) {
    /* MA/YMF825 frequency multiple: 0 => x0.5, n(1..15) => x n.  LINEAR, not the
     * OPL doubling table. */
    static const double ksl_db_per_oct[4] = { 0.0, 1.5, 3.0, 6.0 };
    double m = (o->patch.multi == 0) ? 0.5 : (double)(o->patch.multi & 15);
    double detune = 1.0 + ((double) o->patch.dt - 3.5) * 0.0006;
    double oct, att;
    o->phase_inc = (o->freq_hz * m * detune) / o->sample_rate;
    oct = mafm_log2((o->freq_hz > 1.0 ? o->freq_hz : 1.0) / 261.63);
    att = ksl_db_per_oct[o->patch.ksl & 3] * (oct > 0.0 ? oct : 0.0);
    o->ksl_gain = pow(10.0, -att / 20.0);
    /* nyquist guard: a high note x high MULTI can land above fs/2; mute it. */
    o->ny_mute = (o->phase_inc >= 0.5);
}

static void op_configure(struct mafm_operator *o, const struct mafm_op_patch *p,
                         double sample_rate, double rate_scale) {
    static const double k_vib[4]  = { 0.00196, 0.00387, 0.00774, 0.01548 };
    static const float  k_trem[4] = { 0.129f, 0.242f, 0.424f, 0.669f };
    o->patch = *p;
    o->sample_rate = sample_rate;
    o->wave = p->wave;
    o->tl_gain = tl_to_gain(p->tl);
    o->vib_amt = p->vib ? k_vib[p->dvb & 3] : 0.0;
    o->am_amt  = p->am  ? k_trem[p->dam & 3] : 0.0f;
    o->phase = 0.0;
    o->freq_hz = 440.0;
    env_configure(&o->env, p, sample_rate, rate_scale);
}

static void op_note_on(struct mafm_operator *o, double freq_hz) {
    o->freq_hz = freq_hz;
    o->phase = 0.0;
    op_recalc(o);
    env_key_on(&o->env);
}

static void op_note_off(struct mafm_operator *o) { env_key_off(&o->env); }

static void op_set_base_freq(struct mafm_operator *o, double f) {
    o->freq_hz = f;
    op_recalc(o);
}

/* the OPL/YMF825 sine-derived waveform family (low 3 bits). */
static float op_waveform(uint8_t wave, double phase01) {
    double x = phase01 - floor(phase01);
    double s = sin(MAFM_TWO_PI * x);
    switch (wave & 7) {
    case 0: return (float) s;                          /* sine */
    case 1: return (float)(s > 0 ? s : 0.0);           /* half sine */
    case 2: return (float) fabs(s);                    /* abs sine (rectified) */
    case 3: {                                          /* quarter sine */
        double q = x - floor(x * 2.0) * 0.5;           /* fold to [0,0.5) */
        return (float)(q < 0.25 ? fabs(sin(MAFM_TWO_PI * q)) : 0.0);
    }
    case 4: return (float)(x < 0.5 ? sin(MAFM_TWO_PI * 2.0 * x) : 0.0);        /* even sine */
    case 5: return (float)(x < 0.5 ? fabs(sin(MAFM_TWO_PI * 2.0 * x)) : 0.0);  /* even abs */
    case 6: return (float) tanh(s * 3.0);              /* soft "square" */
    default: {                                         /* soft saw */
        double v = sin(MAFM_TWO_PI * x) + 0.5 * sin(MAFM_TWO_PI * 2 * x)
                 + 0.333 * sin(MAFM_TWO_PI * 3 * x);
        return (float)(v * 0.6);
    }
    }
}

static float op_tick(struct mafm_operator *o, double mod_cycles, double lfo_sin) {
    float env_gain = env_advance(&o->env);
    double inc, out;
    float am_gain;
    if (o->ny_mute) return 0.0f;
    inc = o->phase_inc;
    if (o->vib_amt != 0.0) inc *= 1.0 + o->vib_amt * lfo_sin;
    o->phase += inc;
    if (o->phase >= 1.0) o->phase -= floor(o->phase);
    out = op_waveform(o->wave, o->phase + mod_cycles);
    am_gain = (o->am_amt != 0.0f)
            ? 1.0f - o->am_amt * (float)(0.5 + 0.5 * lfo_sin) : 1.0f;
    return (float)(out * o->tl_gain * o->ksl_gain * env_gain * am_gain);
}

/* ── voice ──────────────────────────────────────────────────────────────── */

/* how deep a modulator drives a carrier's phase (in cycles), the musical FM
 * range Yamaha voices sit in. */
#define MAFM_MOD_DEPTH 0.42

void _WM_MAFM_VoiceInit(struct mafm_voice *v, double sample_rate) {
    memset(v, 0, sizeof(*v));
    v->sample_rate = sample_rate;
    v->velocity = 1.0f;
    v->volume = 1.0f;
    v->active = 0;
    v->channel = -1;
    v->note = -1;
}

void _WM_MAFM_VoiceNoteOn(struct mafm_voice *v, const struct mafm_voice_patch *p,
                          double freq_hz, float velocity) {
    static const double k_lfo_hz[4] = { 1.8, 4.0, 6.0, 9.7 };
    double semis_up;
    int nops, i;

    v->patch = *p;
    v->four_op = p->four_op;
    v->algo = p->algorithm & 7;
    v->feedback = p->feedback;
    v->velocity = clampf(velocity, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        v->fb_mem[i][0] = v->fb_mem[i][1] = 0.0;
        v->op_fb[i] = p->ops[i].fb;
    }
    /* VMA/2-op voices carry feedback only in the header; honour it on op0. */
    if (p->feedback && !v->op_fb[0]) v->op_fb[0] = p->feedback;

    v->lfo_inc = k_lfo_hz[p->lfo & 3] / v->sample_rate;
    v->lfo_phase = 0.0;
    v->lfo_sin = 0.0;

    /* KSR key-scaling: envelopes speed up as the note rises. */
    semis_up = 12.0 * mafm_log2((freq_hz > 1.0 ? freq_hz : 1.0) / 261.63);
    nops = v->four_op ? 4 : 2;
    for (i = 0; i < nops; i++) {
        double rs = pow(2.0, semis_up / (p->ops[i].ksr ? 24.0 : 96.0));
        op_configure(&v->ops[i], &p->ops[i], v->sample_rate, clampd(rs, 0.5, 4.0));
        op_note_on(&v->ops[i], freq_hz);
    }
    v->active = 1;
}

void _WM_MAFM_VoiceNoteOff(struct mafm_voice *v) {
    int nops = v->four_op ? 4 : 2, i;
    for (i = 0; i < nops; i++) op_note_off(&v->ops[i]);
}

void _WM_MAFM_VoiceSetPitch(struct mafm_voice *v, double freq_hz) {
    int nops = v->four_op ? 4 : 2, i;
    for (i = 0; i < nops; i++) op_set_base_freq(&v->ops[i], freq_hz);
}

void _WM_MAFM_VoiceSetVolume(struct mafm_voice *v, float vol) {
    v->volume = vol;
}

int _WM_MAFM_VoiceActive(const struct mafm_voice *v) {
    return v->active;
}

/* run operator i with its own feedback added to the external phase-mod input. */
static float voice_mod_op(struct mafm_voice *v, int i, double ext_mod_cycles) {
    double fbc = 0.0;
    float o;
    if (v->op_fb[i] > 0) {
        double avg = (v->fb_mem[i][0] + v->fb_mem[i][1]) * 0.5;
        fbc = avg * ((double) v->op_fb[i] / 24.0);
    }
    o = op_tick(&v->ops[i], ext_mod_cycles + fbc, v->lfo_sin);
    v->fb_mem[i][1] = v->fb_mem[i][0];
    v->fb_mem[i][0] = o;
    return o;
}

float _WM_MAFM_VoiceTick(struct mafm_voice *v) {
    const double d = MAFM_MOD_DEPTH;
    double out = 0.0;
    int any_live, nops, i;

    if (!v->active) return 0.0f;

    /* advance the voice LFO once; every VIB/AM operator reads it. */
    v->lfo_phase += v->lfo_inc;
    if (v->lfo_phase >= 1.0) v->lfo_phase -= 1.0;
    v->lfo_sin = sin(MAFM_TWO_PI * v->lfo_phase);

    /* the real MA/YMF825 connection algorithms.  op indices 0..3 = chip ops
     * 1..4.  carriers are summed; "a->b" = a modulates b's phase. */
    switch (v->algo) {
    case 0: {                                   /* FB(1)->2         [2-op] */
        double m = voice_mod_op(v, 0, 0.0);
        out = voice_mod_op(v, 1, m * d);
    } break;
    case 1: {                                   /* FB(1) + 2        [2-op] */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, 0.0);
        out = (a + b) * 0.5;
    } break;
    case 2: {                                   /* FB(1)+2+FB(3)+4 */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, 0.0);
        double c = voice_mod_op(v, 2, 0.0);
        double e = voice_mod_op(v, 3, 0.0);
        out = (a + b + c + e) * 0.35;
    } break;
    case 3: {                                   /* (FB(1)+2->3)->4 */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, 0.0);
        double c = voice_mod_op(v, 2, (a + b) * d);
        out = voice_mod_op(v, 3, c * d);
    } break;
    case 4: {                                   /* FB(1)->2->3->4 (serial) */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, a * d);
        double c = voice_mod_op(v, 2, b * d);
        out = voice_mod_op(v, 3, c * d);
    } break;
    case 5: {                                   /* FB(1)->2 + FB(3)->4 */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, a * d);
        double c = voice_mod_op(v, 2, 0.0);
        double e = voice_mod_op(v, 3, c * d);
        out = (b + e) * 0.5;
    } break;
    case 6: {                                   /* FB(1) + 2->3->4 */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, 0.0);
        double c = voice_mod_op(v, 2, b * d);
        double e = voice_mod_op(v, 3, c * d);
        out = (a + e) * 0.5;
    } break;
    default: {                                  /* 7: FB(1) + 2->3 + 4 */
        double a = voice_mod_op(v, 0, 0.0);
        double b = voice_mod_op(v, 1, 0.0);
        double c = voice_mod_op(v, 2, b * d);
        double e = voice_mod_op(v, 3, 0.0);
        out = (a + c + e) * 0.4;
    } break;
    }

    /* all operators finished -> voice is done. */
    any_live = 0;
    nops = v->four_op ? 4 : 2;
    for (i = 0; i < nops; i++) {
        if (!env_finished(&v->ops[i].env)) { any_live = 1; break; }
    }
    if (!any_live) v->active = 0;

    return (float)(out * v->velocity * v->volume * 0.7);
}
