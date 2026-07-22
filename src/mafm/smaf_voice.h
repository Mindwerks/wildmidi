/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * smaf_voice.h -- decode Yamaha VM35 / VMA voice exclusives into a voice patch.
 *
 * Ported to C for WildMIDI from the akustikrausch yamaha-smaf-player
 * (https://github.com/akustikrausch/yamaha-smaf-player), Copyright (c) Andreas
 * Wendorf, licensed under the Apache License, Version 2.0.  See NOTICE and
 * docs/SMAF_FM.md.
 *
 * A SMAF file that uses a custom instrument carries it as a Yamaha sysex blob in
 * the setup chunk (Mtsu) or inline in the sequence: "43 79 06/07 7F 01 ..." for
 * the MA-3/MA-5 form, "43 03 ..." for the MA-1/2 form.  This turns those bytes
 * into the parameters the FM core wants.  Files that only use the chip's
 * built-in bank carry no such blob.
 *
 * Byte layouts follow the go-smaf voice/{vm35fm,vmafm} reference (read as spec).
 */

#ifndef MAFM_SMAF_VOICE_H
#define MAFM_SMAF_VOICE_H

#include <stdint.h>
#include "mafm/ma_fm_core.h"

/* Which instrument slot a voice binds to (from bank-select + program-change;
 * drum_note != 0 marks a percussion voice). */
struct mafm_voice_key {
    int bank_msb;
    int bank_lsb;
    int pc;
    int drum_note;
};

/* PCM (sampled) voice: a drum or sampled instrument that plays an Mwa/Awa wave. */
struct mafm_pcm_params {
    int fs;         /* sampling rate from the body (Hz) */
    int wave_id;    /* binds to the Mwa/Awa wave with this number */
    int loop_pt;    /* loop point (sample index into the decoded wave) */
    int end_pt;     /* end point  (sample index; 0 = whole wave) */
    int loop;       /* RM flag: loop between loop_pt and end_pt (0/1) */
    struct mafm_op_patch env;  /* AR/DR/SR/RR/SL/TL reused for the amplitude EG */
};

struct mafm_parsed_voice {
    struct mafm_voice_key   key;
    struct mafm_voice_patch patch;
    struct mafm_pcm_params  pcm;    /* valid when is_pcm */
    int is_pcm;                     /* pcm voices play a sampled wave, not FM */
    int valid;
};

/* Parse ONE exclusive payload (the bytes between the F0 length and the trailing
 * F7, i.e. starting at the 0x43 maker id) into *out.  Sets out->valid = 0 for
 * anything not synthesised (unknown makers); PCM voices set out->is_pcm. */
void _WM_MAFM_ParseVoiceExclusive(const uint8_t *p, uint32_t n,
                                  struct mafm_parsed_voice *out);

#endif /* MAFM_SMAF_VOICE_H */
