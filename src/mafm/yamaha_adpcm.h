/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * yamaha_adpcm.h -- the 4-bit Yamaha ADPCM codec used by SMAF ATR / stream-PCM.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  See NOTICE and
 * docs/SMAF_FM.md.
 */

#ifndef MAFM_YAMAHA_ADPCM_H
#define MAFM_YAMAHA_ADPCM_H

#include <stdint.h>

/* Decoder state.  Reset before decoding an independent block. */
struct mafm_adpcm {
    int last;   /* last output sample */
    int step;   /* adaptive step size */
};

/* Reset the running state (last=0, step=127). */
void _WM_MAFM_AdpcmReset(struct mafm_adpcm *a);

/* Decode one 4-bit code to a 16-bit PCM sample, advancing the state. */
int16_t _WM_MAFM_AdpcmDecodeNibble(struct mafm_adpcm *a, uint8_t code);

/* Decode a packed buffer of n bytes (two codes each) to 16-bit PCM.
 * high_nibble_first selects which nibble of each byte is decoded first.
 * Writes 2*n samples into out (which must have room); returns the count. */
uint32_t _WM_MAFM_AdpcmDecodeAll(const uint8_t *data, uint32_t n,
                                 int high_nibble_first, int16_t *out);

#endif /* MAFM_YAMAHA_ADPCM_H */
