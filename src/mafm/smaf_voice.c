/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * smaf_voice.c -- the VM35 / VMA exclusive -> voice patch decode.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  See NOTICE and
 * docs/SMAF_FM.md.
 *
 * The tricky bit is the MA-3 "packed" form: Yamaha stole the top bit of a few
 * fields and stashed them in two carrier bytes per operator to save room.  We
 * un-pack those first, then both MA-3 and MA-5 share the same 3-global + 7/op
 * layout.  The MA-1/2 (VMA) form is a smaller 2-global + 5/op layout widened
 * into the same struct.
 */

#include "mafm/smaf_voice.h"

#include <string.h>

/* basic-octave code -> semitone transpose.  BO 0 = +12, 1 = 0, 2 = -12, 3 = -24. */
static int bo_to_semitones(int bo) {
    switch (bo & 3) {
    case 0:  return 12;
    case 1:  return 0;
    case 2:  return -12;
    default: return -24;
    }
}

/* panpot 0..31 -> -1..+1 (0..14 = left, 15 = centre, 16..31 = right). */
static float panpot_to_pan(int pp) {
    pp &= 31;
    if (pp == 15) return 0.0f;
    if (pp < 15)  return -(float)(15 - pp) / 15.0f;
    return (float)(pp - 15) / 16.0f;
}

/* The FM core implements all 8 SMAF/YMF825 connection algorithms directly, so
 * pass the real algorithm straight through.  alg 0/1 are 2-op, 2..7 are 4-op. */
static void apply_alg(struct mafm_voice_patch *v, int alg) {
    alg &= 7;
    v->four_op = (alg >= 2);
    v->algorithm = (uint8_t) alg;
}

/* operator count from the algorithm nibble (alg 0-1 = 2op, 2-7 = 4op). */
static int op_count_from_alg(int alg) {
    return (alg & 7) <= 1 ? 2 : 4;
}

/* Apply the shared 3-global + 7-byte/op VM35 body starting at g[0]. */
static void apply_vm35_body(const uint8_t *g, uint32_t n, int operator_count,
                            struct mafm_voice_patch *v) {
    int bo, pp, pe, alg, count, i;
    const uint8_t *op;

    if (n < 3) return;
    bo  = g[1] & 0x03;
    pp  = (g[1] >> 3) & 0x1f;
    pe  = (g[2] >> 5) & 1;
    alg = g[2] & 0x07;
    apply_alg(v, alg);
    v->note_shift = bo_to_semitones(bo);
    v->pan_default = pe ? panpot_to_pan(pp) : 0.0f;
    v->lfo = (g[2] >> 6) & 0x03;            /* voice LFO rate (vm35fm Global+2) */

    op = g + 3;
    count = operator_count < 1 ? 1 : (operator_count > 4 ? 4 : operator_count);
    for (i = 0; i < count; i++) {
        struct mafm_op_patch *o;
        if ((uint32_t)(op - g) + 7 > n) break;
        o = &v->ops[i];
        o->sr  = (op[0] >> 4) & 0x0f;
        o->ksr =  op[0] & 0x01;
        /* op+0 bit1 = SUS (inert on YMF825).  The sustaining-vs-percussive
         * selector is governed by SR != 0, matching the VMA->VM35 derivation. */
        o->eg_type = (o->sr != 0);
        o->rr = (op[1] >> 4) & 0x0f;
        o->dr =  op[1] & 0x0f;
        o->ar = (op[2] >> 4) & 0x0f;
        o->sl =  op[2] & 0x0f;
        o->tl = (op[3] >> 2) & 0x3f;
        o->ksl =  op[3] & 0x03;
        o->xof = ((op[0] >> 3) & 1) != 0;   /* ignore key-off (drums ring out) */
        o->am  = (op[4] >> 4) & 1;          /* EAM */
        o->dam = (uint8_t)((op[4] >> 5) & 3); /* tremolo depth */
        o->vib =  op[4] & 1;                /* EVB */
        o->dvb = (uint8_t)((op[4] >> 1) & 3); /* vibrato depth */
        o->multi = (op[5] >> 4) & 0x0f;
        o->dt =  op[5] & 0x07;
        o->wave = (op[6] >> 3) & 0x1f;
        o->fb  =  op[6] & 0x07;             /* feedback is PER-OPERATOR on YMF825 */
        if (i == 0) v->feedback = o->fb;    /* also expose op0's fb on the header */
        op += 7;
    }
    /* 2-op voices leave ops[2..3] at defaults (harmless; core only ticks 2). */
}

/* Un-pack the MA-3 (VM3Exclusive) form into the plain VM35 body, byte-for-byte
 * per go-smaf voice/vm35fm Read().  The packed data is 36 bytes (4 global + 8
 * per op); carriers live at raw[op*8] (stealing SR/RR/AR/TL bit7) and at
 * raw[8+op*8] (stealing MUL/WS bit7); raw[0] doubles as the global carrier.
 * After restoring the stolen bits we splice out the carriers to build the
 * standard 3-global + 7/op stream and hand it to apply_vm35_body. */
static void apply_ma3_packed(const uint8_t *body, uint32_t n, int operator_count,
                             struct mafm_voice_patch *v) {
    uint8_t raw[36];
    uint8_t fixed[3 + 7 * 4];
    uint32_t copy, i, w;
    int op;

    memset(raw, 0, sizeof(raw));
    copy = (n < 36) ? n : 36;
    for (i = 0; i < copy; i++) raw[i] = body[i];

    raw[2] = (uint8_t)(raw[2] | ((raw[0] << 2) & 0x80));   /* PANPOT bit7 */
    raw[3] = (uint8_t)(raw[3] | ((raw[0] << 3) & 0x80));   /* ALG-area bit7 */
    for (op = 0; op < 4; op++) {
        raw[4 + op*8]  = (uint8_t)(raw[4 + op*8]  | ((raw[op*8]     << 4) & 0x80)); /* SR */
        raw[5 + op*8]  = (uint8_t)(raw[5 + op*8]  | ((raw[op*8]     << 5) & 0x80)); /* RR */
        raw[6 + op*8]  = (uint8_t)(raw[6 + op*8]  | ((raw[op*8]     << 6) & 0x80)); /* AR */
        raw[7 + op*8]  = (uint8_t)(raw[7 + op*8]  | ((raw[op*8]     << 7) & 0x80)); /* TL */
        raw[10 + op*8] = (uint8_t)(raw[10 + op*8] | ((raw[8 + op*8] << 2) & 0x80)); /* MUL */
        raw[11 + op*8] = (uint8_t)(raw[11 + op*8] | ((raw[8 + op*8] << 3) & 0x80)); /* WS */
    }
    /* fixed = raw[1:4] (3 global) + per op raw[4+op*8:8+op*8] + raw[9+op*8:12+op*8] */
    fixed[0] = raw[1]; fixed[1] = raw[2]; fixed[2] = raw[3];
    w = 3;
    for (op = 0; op < 4; op++) {
        fixed[w++] = raw[4 + op*8]; fixed[w++] = raw[5 + op*8];
        fixed[w++] = raw[6 + op*8]; fixed[w++] = raw[7 + op*8];
        fixed[w++] = raw[9 + op*8]; fixed[w++] = raw[10 + op*8];
        fixed[w++] = raw[11 + op*8];
    }
    apply_vm35_body(fixed, w, operator_count, v);
}

void _WM_MAFM_ParseVoiceExclusive(const uint8_t *p, uint32_t n,
                                  struct mafm_parsed_voice *out) {
    memset(out, 0, sizeof(*out));
    _WM_MAFM_DefaultPatch(&out->patch);
    if (!p || n < 5) return;
    if (p[0] != 0x43) return;   /* not a Yamaha maker id */

    /* MA-3 / MA-5 long form: 43 79 06|07 7F 01 [bank...] body */
    if (n >= 11 && p[1] == 0x79 && (p[2] == 0x06 || p[2] == 0x07) &&
        p[3] == 0x7f && p[4] == 0x01) {
        int voice_type;
        out->key.bank_msb = p[5];
        out->key.bank_lsb = p[6];
        out->key.pc = p[7];
        out->key.drum_note = p[8];
        voice_type = p[9];
        if (voice_type != 0) {                       /* PCM (sampled) voice */
            const uint8_t *b = p + 10;
            uint32_t bn = n - 10;
            out->is_pcm = 1;
            if (bn >= 16) {
                struct mafm_pcm_params *pc = &out->pcm;
                pc->fs     = ((int)b[0] << 8) | b[1];        /* Fs, u16 BE */
                pc->env.tl = (b[7] >> 2) & 0x3f;             /* TL */
                pc->env.sr = (b[4] >> 4) & 0x0f;             /* SR */
                pc->env.rr = (b[5] >> 4) & 0x0f;             /* RR */
                pc->env.dr =  b[5] & 0x0f;                   /* DR */
                pc->env.ar = (b[6] >> 4) & 0x0f;             /* AR */
                pc->env.sl =  b[6] & 0x0f;                   /* SL */
                pc->env.eg_type = (pc->env.sr != 0);
                pc->loop_pt = ((int)b[11] << 8) | b[12];     /* LP, u16 BE */
                pc->end_pt  = ((int)b[13] << 8) | b[14];     /* EP, u16 BE */
                pc->loop    = (b[15] & 0x80) != 0;           /* RM flag */
                pc->wave_id =  b[15] & 0x7f;                 /* WaveID */
                if (pc->fs < 2000 || pc->fs > 48000) pc->fs = 8000;
                out->valid = 1;
            }
            return;
        }
        {
            const uint8_t *body = p + 10;
            uint32_t bn = n - 10;
            /* Need the algorithm to know the op count.  MA-5 DIRECT: alg in body
             * byte 2.  MA-3 PACKED: byte 2 is still PANPOT and the alg sits in
             * byte 3 (its low 3 bits survive the bit-steal); peek the right byte
             * or 4-op voices truncate to 2. */
            uint32_t alg_byte = (p[2] == 0x06) ? 3 : 2;
            int alg = (bn > alg_byte) ? (body[alg_byte] & 0x07) : 0;
            int ops = op_count_from_alg(alg);
            if (p[2] == 0x06) apply_ma3_packed(body, bn, ops, &out->patch);
            else              apply_vm35_body(body, bn, ops, &out->patch);
            out->valid = 1;
        }
        return;
    }

    /* MA-5 short form: 43 05 01 [bankLSB] [pc] body */
    if (n >= 6 && p[1] == 0x05 && p[2] == 0x01) {
        const uint8_t *body = p + 5;
        uint32_t bn = n - 5;
        int alg = (bn >= 3) ? (body[2] & 0x07) : 0;
        out->key.bank_msb = 0;
        out->key.bank_lsb = p[3];
        out->key.pc = p[4];
        apply_vm35_body(body, bn, op_count_from_alg(alg), &out->patch);
        out->valid = 1;
        return;
    }

    /* MA-1 / MA-2 (VMA) form: 43 03 [Enigma][Bank][PC] then the voice body
     * (2 global + 5 bytes/op).  The note stream selects the voice by (Bank, PC). */
    if (n >= 7 && p[1] == 0x03) {
        const uint8_t *g = p + 5;    /* body starts after the header */
        uint32_t gn = n - 5;
        out->key.bank_msb = 0;
        out->key.bank_lsb = p[3] & 0x7f;  /* Bank (bit7 = drum bank, masked off) */
        out->key.pc       = p[4];         /* PC */
        if (gn >= 2) {
            int fb  = (g[0] >> 3) & 0x07;
            int alg = g[0] & 0x07;        /* g[1] = the 0x01 enigma constant */
            int count, i;
            const uint8_t *op;
            apply_alg(&out->patch, alg);
            out->patch.feedback = (uint8_t) fb;
            out->patch.lfo = (uint8_t)((g[0] >> 6) & 3);   /* voice LFO rate */
            count = op_count_from_alg(alg);
            op = g + 2;
            for (i = 0; i < count; i++) {
                struct mafm_op_patch *o;
                int egt;
                if ((uint32_t)(op - g) + 5 > gn) break;
                o = &out->patch.ops[i];
                /* VMAFMOperator.Read: +0 MULT|VIB(b3)|EGT(b2)|SUS(b1)|KSR(b0) */
                o->multi = (op[0] >> 4) & 0x0f;
                o->vib   = (op[0] & 0x08) != 0;   /* VIB (bit3) */
                egt      = (op[0] & 0x04) != 0;   /* EGT (bit2) */
                o->ksr   = (op[0] & 0x01);
                o->rr = (op[1] >> 4) & 0x0f; o->dr = op[1] & 0x0f;
                o->ar = (op[2] >> 4) & 0x0f; o->sl = op[2] & 0x0f;
                o->tl = (op[3] >> 2) & 0x3f; o->ksl = op[3] & 0x03;
                /* +4: DVB(b7-6) | DAM(b5-4) | AM(b3) | WS(b2-0) */
                o->am   = (op[4] & 0x08) != 0;
                o->dvb  = (uint8_t)((op[4] >> 6) & 3);
                o->dam  = (uint8_t)((op[4] >> 4) & 3);
                o->wave = op[4] & 0x07;
                /* VMAFMOperator.ToVM35: SR = EGT ? 0 : RR ; sustaining if SR!=0 */
                o->sr = egt ? 0 : (op[1] >> 4) & 0x0f;
                o->eg_type = (o->sr != 0);
                if (i == 0) o->fb = (uint8_t) fb; /* VMA feedback lands on op0 */
                op += 5;
            }
            out->valid = 1;
        }
        return;
    }

    /* unknown Yamaha sub-form: leave out->valid = 0 */
}
