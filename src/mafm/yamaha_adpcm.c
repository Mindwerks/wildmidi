/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * yamaha_adpcm.c -- the 4-bit Yamaha ADPCM codec used by SMAF ATR / stream-PCM.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  The C translation
 * preserves the original algorithm; see NOTICE and docs/SMAF_FM.md.
 *
 * This is the YM2608 (OPNA) ADPCM-B family, the same 4-bit codec Yamaha reused
 * across their FM parts.  One nibble in, one 16-bit sample out, with an
 * adaptive step size.  Two codes are packed per byte; MA files disagree on
 * which nibble comes first, so the caller selects high- or low-nibble-first
 * (high-nibble-first is the Yamaha-standard default).
 */

#include "mafm/yamaha_adpcm.h"

static int adpcm_clamp16(int v) {
    if (v < -32768) return -32768;
    if (v >  32767) return  32767;
    return v;
}

void _WM_MAFM_AdpcmReset(struct mafm_adpcm *a) {
    a->last = 0;
    a->step = 127;
}

int16_t _WM_MAFM_AdpcmDecodeNibble(struct mafm_adpcm *a, uint8_t code) {
    int delta = a->step >> 3;
    if (code & 1) delta += a->step >> 2;
    if (code & 2) delta += a->step >> 1;
    if (code & 4) delta += a->step;
    if (code & 8) delta = -delta;
    a->last = adpcm_clamp16(a->last + delta);

    /* step adaptation (OPNA-B table, as the published ratios) */
    switch (code & 7) {
    case 0: case 1: case 2: case 3: a->step = a->step * 115 / 128; break;
    case 4: a->step = a->step * 307 / 256; break;
    case 5: a->step = a->step * 409 / 256; break;
    case 6: a->step = a->step * 2;         break;
    default: a->step = a->step * 307 / 128; break;   /* 7 */
    }
    if (a->step < 127)   a->step = 127;
    if (a->step > 24576) a->step = 24576;

    return (int16_t) a->last;
}

uint32_t _WM_MAFM_AdpcmDecodeAll(const uint8_t *data, uint32_t n,
                                 int high_nibble_first, int16_t *out) {
    struct mafm_adpcm a;
    uint32_t i, w = 0;
    _WM_MAFM_AdpcmReset(&a);
    for (i = 0; i < n; i++) {
        uint8_t b = data[i];
        uint8_t hi = (uint8_t)((b >> 4) & 0x0f);
        uint8_t lo = (uint8_t)(b & 0x0f);
        if (high_nibble_first) {
            out[w++] = _WM_MAFM_AdpcmDecodeNibble(&a, hi);
            out[w++] = _WM_MAFM_AdpcmDecodeNibble(&a, lo);
        } else {
            out[w++] = _WM_MAFM_AdpcmDecodeNibble(&a, lo);
            out[w++] = _WM_MAFM_AdpcmDecodeNibble(&a, hi);
        }
    }
    return w;
}
