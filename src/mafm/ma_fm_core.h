/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ma_fm_core.h -- FM voice engine of the Yamaha MA-series ringtone chips.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  See NOTICE and
 * docs/SMAF_FM.md.
 *
 * The MA-3/MA-5 sound source is a close cousin of the classic OPL/OPN Yamaha FM
 * line: sine-family operators, an exponential envelope generator, 2-op and 4-op
 * voices with a small set of connection algorithms.  Patch field values are in
 * the chip's own units (0..15 rates, 0..63 TL, ...) so sequencer register bytes
 * drop straight in.  The DSP runs in double where the chip used fixed-point
 * log/exp ROMs; what matters is that the envelope shape and operator algebra
 * match, which is where FM timbre lives.
 */

#ifndef MAFM_MA_FM_CORE_H
#define MAFM_MA_FM_CORE_H

#include <stdint.h>

/* One operator's patch. */
struct mafm_op_patch {
    uint8_t multi;    /* frequency multiple (0..15; 0 = x0.5) */
    uint8_t tl;       /* total level / attenuation (0..63, 0 = loudest) */
    uint8_t ar;       /* attack rate  (0..15) */
    uint8_t dr;       /* decay rate   (0..15) */
    uint8_t sr;       /* sustain rate (0..15) (aka second decay) */
    uint8_t rr;       /* release rate (0..15) */
    uint8_t sl;       /* sustain level (0..15) */
    uint8_t ksl;      /* key-scale level (0..3) */
    uint8_t ksr;      /* key-scale rate  (0..1) */
    uint8_t wave;     /* waveform select (0..31; the YMF825/OPL wave set) */
    uint8_t dt;       /* detune (0..7) */
    uint8_t fb;       /* per-operator self-feedback (0..7) */
    uint8_t am;       /* amplitude modulation (tremolo) enable (0/1) */
    uint8_t vib;      /* vibrato enable (0/1) */
    uint8_t eg_type;  /* 1 = sustaining (hold at SL), 0 = percussive */
    uint8_t dvb;      /* vibrato depth (0..3) */
    uint8_t dam;      /* tremolo depth (0..3) */
    uint8_t xof;      /* ignore key-off (drums ring out fully) (0/1) */
};

/* A voice patch: 2-op or 4-op, one connection algorithm. */
struct mafm_voice_patch {
    uint8_t  four_op;      /* 0 = 2-op, 1 = 4-op */
    uint8_t  algorithm;    /* connection algo (0..1 for 2-op, 0..7 for 4-op) */
    uint8_t  feedback;     /* op0 self-feedback (0..7) */
    int      note_shift;   /* basic-octave transpose in semitones (BO) */
    float    pan_default;  /* patch panpot default, -1..+1 (0 = centre) */
    uint8_t  lfo;          /* voice LFO rate select (0..3) */
    struct mafm_op_patch ops[4];  /* ops[0..1] for 2-op, all four for 4-op */
};

/* Fill *v with the safe default patch (single carrier + modulator, 2-op). */
void _WM_MAFM_DefaultPatch(struct mafm_voice_patch *v);

/* The built-in, ROM-free GM approximation bank (program 0..127) and the drum
 * approximation, for channels with no bound custom voice.  Fill *out. */
void _WM_MAFM_GmApprox(int program, struct mafm_voice_patch *out);
void _WM_MAFM_DrumApprox(int note, struct mafm_voice_patch *out);

/* ── DSP ──────────────────────────────────────────────────────────────────── */

/* Envelope generator (ADSR, exponential, chip-style discrete rates). */
enum mafm_eg_phase {
    MAFM_EG_IDLE = 0, MAFM_EG_ATTACK, MAFM_EG_DECAY,
    MAFM_EG_SUSTAIN, MAFM_EG_RELEASE
};

struct mafm_env {
    enum mafm_eg_phase phase;
    double level;                        /* 0 (silent) .. 1 (peak), linear */
    double atk_step, dec_step, sus_step, rel_step;
    double sus_level;
    int    sustaining;
    int    xof;                          /* ignore key-off (drums ring out) */
};

/* Operator: phase generator + waveform + envelope. */
struct mafm_operator {
    struct mafm_op_patch patch;
    struct mafm_env env;
    double sample_rate;
    double freq_hz;
    double phase;        /* 0..1 */
    double phase_inc;
    double tl_gain;      /* linear gain from total-level */
    double ksl_gain;     /* key-scale level attenuation (freq-dependent) */
    double vib_amt;      /* vibrato depth as a phase-inc factor */
    float  am_amt;       /* tremolo depth as a linear gain span */
    int    ny_mute;      /* operator lands above fs/2 -> silent, no alias */
    uint8_t wave;
};

/* A playing voice. */
struct mafm_voice {
    struct mafm_voice_patch patch;
    struct mafm_operator ops[4];
    double sample_rate;
    float  velocity;
    float  volume;
    int    active;
    int    four_op;
    uint8_t algo;
    uint8_t feedback;
    uint8_t op_fb[4];                    /* per-operator feedback amount (0..7) */
    double  fb_mem[4][2];                /* per-op feedback history */
    double  lfo_phase, lfo_inc;          /* one LFO per voice */
    double  lfo_sin;                     /* this sample's LFO value */
    int     channel;                     /* owning smaf channel (for stealing) */
    int     note;                        /* current note (for stealing) */
};

/* Voice lifecycle.  Set the sample rate once, then note-on/off and tick. */
void  _WM_MAFM_VoiceInit(struct mafm_voice *v, double sample_rate);
void  _WM_MAFM_VoiceNoteOn(struct mafm_voice *v, const struct mafm_voice_patch *p,
                           double freq_hz, float velocity);
void  _WM_MAFM_VoiceNoteOff(struct mafm_voice *v);
void  _WM_MAFM_VoiceSetPitch(struct mafm_voice *v, double freq_hz);
void  _WM_MAFM_VoiceSetVolume(struct mafm_voice *v, float vol);
float _WM_MAFM_VoiceTick(struct mafm_voice *v);      /* one mono sample ~[-1,1] */
int   _WM_MAFM_VoiceActive(const struct mafm_voice *v);

#endif /* MAFM_MA_FM_CORE_H */
