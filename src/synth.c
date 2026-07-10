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

/* All tonal patches keep EGT set (0x20 in chr) so the OPL envelope holds its
   sustain level forever; note gating/decay is done by the WildMIDI mixer
   envelope, and the held plateau gives us a clean loop region. AM/VIB stay
   clear so the sustained tone is exactly periodic (loopable). Values are
   family-based with per-program variation on multiplier/level/feedback. */
static const fm_patch gm_fm[128] = {
    /* 0-7 Piano */
    {0x21,0x1a,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,0, 0x0a}, /* 0 Acoustic Grand */
    {0x21,0x18,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,0, 0x0a}, /* 1 Bright Acoustic */
    {0x21,0x16,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,1, 0x0a}, /* 2 Electric Grand */
    {0x23,0x1c,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,0, 0x0a}, /* 3 Honky-tonk */
    {0x21,0x1e,0xf4,0x05,1, 0x21,0x00,0xf4,0x05,0, 0x08}, /* 4 E.Piano 1 */
    {0x22,0x1e,0xf4,0x05,1, 0x21,0x00,0xf4,0x05,0, 0x08}, /* 5 E.Piano 2 */
    {0x24,0x1c,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,0, 0x0c}, /* 6 Harpsichord */
    {0x25,0x18,0xf4,0x05,0, 0x21,0x00,0xf4,0x05,0, 0x0c}, /* 7 Clavinet */
    /* 8-15 Chromatic percussion */
    {0x24,0x1d,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 8 Celesta */
    {0x27,0x1d,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 9 Glockenspiel */
    {0x24,0x1b,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 10 Music Box */
    {0x27,0x1b,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 11 Vibraphone */
    {0x25,0x1d,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 12 Marimba */
    {0x27,0x19,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 13 Xylophone */
    {0x24,0x17,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x0a}, /* 14 Tubular Bells */
    {0x24,0x1d,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 15 Dulcimer */
    /* 16-23 Organ (additive connection for drawbar character) */
    {0x21,0x10,0xf0,0x0f,0, 0x21,0x00,0xf0,0x0f,0, 0x03}, /* 16 Drawbar */
    {0x22,0x10,0xf0,0x0f,0, 0x21,0x00,0xf0,0x0f,0, 0x03}, /* 17 Percussive */
    {0x21,0x0c,0xf0,0x0f,0, 0x21,0x00,0xf0,0x0f,0, 0x05}, /* 18 Rock */
    {0x21,0x12,0xa0,0x0f,0, 0x21,0x00,0xa0,0x0f,0, 0x03}, /* 19 Church */
    {0x22,0x12,0xa0,0x0f,0, 0x21,0x00,0xa0,0x0f,0, 0x03}, /* 20 Reed */
    {0x21,0x14,0xc0,0x0f,0, 0x21,0x00,0xc0,0x0f,0, 0x03}, /* 21 Accordion */
    {0x22,0x14,0xc0,0x0f,0, 0x21,0x00,0xc0,0x0f,0, 0x03}, /* 22 Harmonica */
    {0x21,0x14,0xc0,0x0f,0, 0x22,0x00,0xc0,0x0f,0, 0x03}, /* 23 Tango Accordion */
    /* 24-31 Guitar */
    {0x23,0x18,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x0c}, /* 24 Nylon */
    {0x23,0x16,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x0c}, /* 25 Steel */
    {0x22,0x1a,0xf5,0x05,1, 0x21,0x00,0xf5,0x05,0, 0x0a}, /* 26 Jazz */
    {0x22,0x18,0xf5,0x05,1, 0x21,0x00,0xf5,0x05,0, 0x0a}, /* 27 Clean */
    {0x23,0x14,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x0e}, /* 28 Muted */
    {0x21,0x10,0xf5,0x15,1, 0x21,0x00,0xf5,0x15,0, 0x0e}, /* 29 Overdriven */
    {0x21,0x0c,0xf5,0x15,1, 0x21,0x00,0xf5,0x15,0, 0x0e}, /* 30 Distortion */
    {0x25,0x16,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x0c}, /* 31 Harmonics */
    /* 32-39 Bass (carrier at half multiplier for depth) */
    {0x21,0x14,0xf5,0x05,0, 0x20,0x00,0xf5,0x05,0, 0x0a}, /* 32 Acoustic */
    {0x21,0x12,0xf5,0x05,0, 0x20,0x00,0xf5,0x05,0, 0x0a}, /* 33 Finger */
    {0x21,0x10,0xf5,0x05,0, 0x20,0x00,0xf5,0x05,0, 0x0c}, /* 34 Pick */
    {0x21,0x16,0xf5,0x05,0, 0x20,0x00,0xf5,0x05,0, 0x08}, /* 35 Fretless */
    {0x22,0x10,0xf5,0x05,1, 0x20,0x00,0xf5,0x05,0, 0x0c}, /* 36 Slap 1 */
    {0x22,0x0e,0xf5,0x05,1, 0x20,0x00,0xf5,0x05,0, 0x0c}, /* 37 Slap 2 */
    {0x21,0x0e,0xf5,0x05,1, 0x20,0x00,0xf5,0x05,1, 0x0c}, /* 38 Synth 1 */
    {0x21,0x12,0xf5,0x05,1, 0x20,0x00,0xf5,0x05,1, 0x0a}, /* 39 Synth 2 */
    /* 40-47 Strings */
    {0x21,0x22,0x84,0x05,0, 0x21,0x00,0x84,0x05,0, 0x06}, /* 40 Violin */
    {0x21,0x22,0x84,0x05,0, 0x21,0x00,0x84,0x05,0, 0x06}, /* 41 Viola */
    {0x21,0x24,0x84,0x05,0, 0x21,0x00,0x84,0x05,0, 0x06}, /* 42 Cello */
    {0x21,0x26,0x74,0x05,0, 0x21,0x00,0x74,0x05,0, 0x06}, /* 43 Contrabass */
    {0x21,0x24,0x64,0x05,0, 0x21,0x00,0x64,0x05,0, 0x06}, /* 44 Tremolo */
    {0x23,0x1e,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x08}, /* 45 Pizzicato */
    {0x24,0x1a,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 46 Harp */
    {0x21,0x1c,0xf7,0x05,0, 0x21,0x00,0xf7,0x05,0, 0x06}, /* 47 Timpani */
    /* 48-55 Ensemble */
    {0x21,0x24,0x74,0x05,0, 0x21,0x00,0x74,0x05,0, 0x04}, /* 48 String Ens 1 */
    {0x21,0x26,0x64,0x05,0, 0x21,0x00,0x64,0x05,0, 0x04}, /* 49 String Ens 2 */
    {0x21,0x26,0x54,0x05,1, 0x21,0x00,0x54,0x05,0, 0x04}, /* 50 Synth Strings 1 */
    {0x21,0x28,0x54,0x05,1, 0x21,0x00,0x54,0x05,0, 0x04}, /* 51 Synth Strings 2 */
    {0x21,0x2a,0x64,0x05,0, 0x21,0x00,0x64,0x05,0, 0x02}, /* 52 Choir Aahs */
    {0x21,0x28,0x64,0x05,0, 0x21,0x00,0x64,0x05,0, 0x02}, /* 53 Voice Oohs */
    {0x21,0x26,0x64,0x05,1, 0x21,0x00,0x64,0x05,0, 0x04}, /* 54 Synth Voice */
    {0x21,0x1e,0x85,0x05,0, 0x21,0x00,0x85,0x05,0, 0x0a}, /* 55 Orchestra Hit */
    /* 56-63 Brass */
    {0x21,0x12,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x0e}, /* 56 Trumpet */
    {0x21,0x14,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x0e}, /* 57 Trombone */
    {0x21,0x16,0x76,0x05,0, 0x21,0x00,0x76,0x05,0, 0x0c}, /* 58 Tuba */
    {0x21,0x10,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0e}, /* 59 Muted Trumpet */
    {0x21,0x16,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x0c}, /* 60 French Horn */
    {0x21,0x12,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x0e}, /* 61 Brass Section */
    {0x21,0x10,0x86,0x05,1, 0x21,0x00,0x86,0x05,0, 0x0e}, /* 62 Synth Brass 1 */
    {0x21,0x12,0x86,0x05,1, 0x21,0x00,0x86,0x05,0, 0x0c}, /* 63 Synth Brass 2 */
    /* 64-71 Reed */
    {0x22,0x16,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 64 Soprano Sax */
    {0x22,0x18,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 65 Alto Sax */
    {0x22,0x1a,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 66 Tenor Sax */
    {0x22,0x1c,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 67 Baritone Sax */
    {0x23,0x16,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x08}, /* 68 Oboe */
    {0x23,0x18,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x08}, /* 69 English Horn */
    {0x22,0x1a,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x08}, /* 70 Bassoon */
    {0x22,0x14,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x08}, /* 71 Clarinet */
    /* 72-79 Pipe (gentle modulation, near-flute) */
    {0x21,0x28,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x02}, /* 72 Piccolo */
    {0x21,0x28,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x02}, /* 73 Flute */
    {0x21,0x2a,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x02}, /* 74 Recorder */
    {0x21,0x2a,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x04}, /* 75 Pan Flute */
    {0x21,0x2c,0x76,0x05,0, 0x21,0x00,0x76,0x05,0, 0x04}, /* 76 Blown Bottle */
    {0x22,0x2a,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x04}, /* 77 Shakuhachi */
    {0x21,0x26,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x02}, /* 78 Whistle */
    {0x21,0x28,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x02}, /* 79 Ocarina */
    /* 80-87 Synth lead */
    {0x21,0x10,0xf6,0x05,2, 0x21,0x00,0xf6,0x05,0, 0x0e}, /* 80 Square */
    {0x21,0x10,0xf6,0x05,1, 0x21,0x00,0xf6,0x05,1, 0x0e}, /* 81 Sawtooth */
    {0x21,0x1c,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x04}, /* 82 Calliope */
    {0x21,0x18,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x06}, /* 83 Chiff */
    {0x22,0x14,0xa6,0x05,1, 0x21,0x00,0xa6,0x05,0, 0x0a}, /* 84 Charang */
    {0x21,0x1a,0x86,0x05,0, 0x21,0x00,0x86,0x05,0, 0x04}, /* 85 Voice */
    {0x25,0x12,0xa6,0x05,1, 0x21,0x00,0xa6,0x05,0, 0x0c}, /* 86 Fifths */
    {0x21,0x0e,0xa6,0x05,1, 0x21,0x00,0xa6,0x05,1, 0x0e}, /* 87 Bass+Lead */
    /* 88-95 Synth pad (slow attack) */
    {0x21,0x20,0x54,0x05,0, 0x21,0x00,0x54,0x05,0, 0x04}, /* 88 New Age */
    {0x21,0x22,0x44,0x05,0, 0x21,0x00,0x44,0x05,0, 0x04}, /* 89 Warm */
    {0x21,0x1e,0x54,0x05,1, 0x21,0x00,0x54,0x05,0, 0x06}, /* 90 Polysynth */
    {0x21,0x24,0x44,0x05,0, 0x21,0x00,0x44,0x05,0, 0x02}, /* 91 Choir */
    {0x21,0x22,0x44,0x05,0, 0x21,0x00,0x44,0x05,0, 0x04}, /* 92 Bowed */
    {0x21,0x20,0x54,0x05,1, 0x21,0x00,0x54,0x05,0, 0x04}, /* 93 Metallic */
    {0x21,0x24,0x44,0x05,0, 0x21,0x00,0x44,0x05,0, 0x02}, /* 94 Halo */
    {0x21,0x22,0x54,0x05,1, 0x21,0x00,0x54,0x05,0, 0x04}, /* 95 Sweep */
    /* 96-103 Synth FX */
    {0x24,0x18,0x86,0x05,2, 0x21,0x00,0x86,0x05,1, 0x08}, /* 96 Rain */
    {0x22,0x18,0x66,0x05,2, 0x21,0x00,0x66,0x05,0, 0x08}, /* 97 Soundtrack */
    {0x26,0x16,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 98 Crystal */
    {0x24,0x18,0x86,0x05,1, 0x21,0x00,0x86,0x05,0, 0x08}, /* 99 Atmosphere */
    {0x21,0x16,0x76,0x05,2, 0x21,0x00,0x76,0x05,0, 0x0a}, /* 100 Brightness */
    {0x25,0x1a,0x66,0x05,2, 0x21,0x00,0x66,0x05,1, 0x08}, /* 101 Goblins */
    {0x24,0x18,0x76,0x05,1, 0x21,0x00,0x76,0x05,1, 0x0a}, /* 102 Echoes */
    {0x26,0x18,0x86,0x05,2, 0x21,0x00,0x86,0x05,2, 0x0a}, /* 103 Sci-Fi */
    /* 104-111 Ethnic */
    {0x24,0x1c,0xf5,0x05,0, 0x21,0x00,0xf5,0x05,0, 0x0a}, /* 104 Sitar */
    {0x25,0x1c,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x0a}, /* 105 Banjo */
    {0x24,0x1a,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x0a}, /* 106 Shamisen */
    {0x26,0x1a,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 107 Koto */
    {0x27,0x1a,0xf6,0x05,0, 0x21,0x00,0xf6,0x05,0, 0x08}, /* 108 Kalimba */
    {0x22,0x18,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 109 Bagpipe */
    {0x21,0x20,0x84,0x05,0, 0x21,0x00,0x84,0x05,0, 0x06}, /* 110 Fiddle */
    {0x22,0x16,0x96,0x05,0, 0x21,0x00,0x96,0x05,0, 0x0a}, /* 111 Shanai */
    /* 112-119 Percussive (sustain plateau lower: SL=3) */
    {0x27,0x18,0xf6,0x35,0, 0x21,0x00,0xf6,0x35,0, 0x0c}, /* 112 Tinkle Bell */
    {0x25,0x16,0xf6,0x35,0, 0x21,0x00,0xf6,0x35,0, 0x0c}, /* 113 Agogo */
    {0x24,0x16,0xf7,0x35,0, 0x21,0x00,0xf7,0x35,0, 0x0a}, /* 114 Steel Drums */
    {0x26,0x14,0xf7,0x35,0, 0x21,0x00,0xf7,0x35,0, 0x0c}, /* 115 Woodblock */
    {0x21,0x16,0xf7,0x35,0, 0x21,0x00,0xf7,0x35,0, 0x0a}, /* 116 Taiko */
    {0x22,0x16,0xf7,0x35,0, 0x21,0x00,0xf7,0x35,0, 0x0a}, /* 117 Melodic Tom */
    {0x23,0x14,0xf7,0x35,1, 0x21,0x00,0xf7,0x35,0, 0x0c}, /* 118 Synth Drum */
    {0x25,0x12,0x67,0x35,2, 0x21,0x00,0x67,0x35,1, 0x0c}, /* 119 Reverse Cymbal */
    /* 120-127 Sound FX */
    {0x27,0x1e,0x77,0x05,3, 0x21,0x00,0x77,0x05,2, 0x0e}, /* 120 Fret Noise */
    {0x25,0x1e,0x87,0x05,2, 0x21,0x00,0x87,0x05,2, 0x0e}, /* 121 Breath Noise */
    {0x21,0x22,0x55,0x05,2, 0x21,0x00,0x55,0x05,2, 0x0e}, /* 122 Seashore */
    {0x27,0x14,0x97,0x05,3, 0x21,0x00,0x97,0x05,0, 0x0e}, /* 123 Bird Tweet */
    {0x26,0x14,0xa7,0x05,2, 0x21,0x00,0xa7,0x05,0, 0x0e}, /* 124 Telephone */
    {0x22,0x20,0x55,0x05,3, 0x21,0x00,0x55,0x05,2, 0x0e}, /* 125 Helicopter */
    {0x21,0x22,0x45,0x05,3, 0x21,0x00,0x45,0x05,3, 0x0e}, /* 126 Applause */
    {0x24,0x10,0xf7,0x05,0, 0x21,0x00,0xf7,0x05,0, 0x0e}  /* 127 Gunshot */
};

/* Per-family mixer ADSR (program >> 3): sustain (mixer fixed-point,
   4194303 = 1.0), attack/decay/release seconds. */
typedef struct { int32_t sustain; float attack, decay, release; } mix_env;
static const mix_env gm_env[16] = {
    /* Piano       */ {1258291, 0.003f, 0.400f, 0.180f},
    /* Chrom.perc  */ { 838860, 0.002f, 0.300f, 0.120f},
    /* Organ       */ {4194303, 0.012f, 0.080f, 0.100f},
    /* Guitar      */ {1677721, 0.003f, 0.500f, 0.180f},
    /* Bass        */ {2097151, 0.004f, 0.400f, 0.160f},
    /* Strings     */ {3355443, 0.060f, 0.200f, 0.250f},
    /* Ensemble    */ {3355443, 0.050f, 0.200f, 0.300f},
    /* Brass       */ {3145727, 0.030f, 0.150f, 0.150f},
    /* Reed        */ {3355443, 0.025f, 0.150f, 0.120f},
    /* Pipe        */ {3355443, 0.030f, 0.150f, 0.140f},
    /* Synth Lead  */ {3355443, 0.008f, 0.100f, 0.100f},
    /* Synth Pad   */ {3355443, 0.150f, 0.400f, 0.400f},
    /* Synth FX    */ {2097151, 0.020f, 0.300f, 0.250f},
    /* Ethnic      */ {1677721, 0.005f, 0.400f, 0.180f},
    /* Percussive  */ { 838860, 0.002f, 0.300f, 0.120f},
    /* SFX         */ {1677721, 0.005f, 0.400f, 0.200f}
};

/* ------------------------------------------------------------------ */
/* Drum voices                                                        */
/* ------------------------------------------------------------------ */

/* One-shot drums: EGT clear so the OPL envelope decays on its own while the
   key is held for the whole render. High feedback approximates noise. */
static const fm_patch drum_fm[6] = {
    /* kick   */ {0x00,0x08,0xf8,0x4f,0, 0x00,0x00,0xf6,0x4f,0, 0x0e},
    /* tom    */ {0x01,0x0c,0xf7,0x4f,0, 0x00,0x00,0xf6,0x4f,0, 0x0a},
    /* snare  */ {0x0f,0x08,0xf8,0x7f,6, 0x00,0x00,0xf7,0x6f,0, 0x0e},
    /* hat    */ {0x0f,0x06,0xf9,0xff,6, 0x0b,0x00,0xf8,0xaf,5, 0x0e},
    /* cymbal */ {0x0e,0x08,0xf6,0x5f,6, 0x0b,0x00,0xf5,0x4f,5, 0x0e},
    /* bell   */ {0x07,0x10,0xf6,0x5f,0, 0x02,0x00,0xf5,0x5f,0, 0x08}
};
enum { D_KICK, D_TOM, D_SNARE, D_HAT, D_CYMBAL, D_BELL };

/* GM percussion key -> preset, OPL pitch, render length. */
typedef struct { uint8_t preset; uint16_t fnum; uint8_t block; uint16_t ms; } drum_voice;

static drum_voice drum_for_key(uint8_t k) {
    drum_voice d;
    switch (k) {
    case 35: case 36:                     /* kicks */
        d.preset = D_KICK;   d.fnum = 290; d.block = 2; d.ms = 300; break;
    case 41: case 43: case 45:            /* low/mid toms */
    case 47: case 48: case 50:            /* high toms */
        d.preset = D_TOM;    d.fnum = (uint16_t)(200 + (k - 41) * 40);
        d.block = 3;         d.ms = 250; break;
    case 38: case 40:                     /* snares */
        d.preset = D_SNARE;  d.fnum = 300; d.block = 5; d.ms = 200; break;
    case 37: case 39:                     /* side stick / clap */
        d.preset = D_SNARE;  d.fnum = 350; d.block = 5; d.ms = 120; break;
    case 42: case 44: case 54:            /* closed/pedal hat, tambourine */
    case 69: case 70: case 82:            /* cabasa, maracas, shaker */
        d.preset = D_HAT;    d.fnum = 600; d.block = 7; d.ms = 90;  break;
    case 46: case 74:                     /* open hat, long guiro */
        d.preset = D_HAT;    d.fnum = 600; d.block = 7; d.ms = 300; break;
    case 49: case 51: case 52: case 53:   /* crash, ride, china */
    case 55: case 57: case 59:            /* splash, crash2, ride2 */
        d.preset = D_CYMBAL; d.fnum = 500; d.block = 6; d.ms = 600; break;
    case 56: case 67: case 68:            /* cowbell, agogos */
        d.preset = D_BELL;   d.fnum = 450; d.block = 5; d.ms = 200; break;
    case 80: case 81:                     /* triangles */
        d.preset = D_BELL;   d.fnum = 700; d.block = 7;
        d.ms = (k == 81) ? 500 : 100; break;
    case 60: case 61: case 62: case 63: case 64:   /* bongos, congas */
    case 65: case 66: case 73: case 75:            /* timbales, guiro, claves */
    case 76: case 77:                              /* wood blocks */
        d.preset = D_TOM;    d.fnum = (uint16_t)(250 + (k - 60) * 20);
        d.block = 4;         d.ms = 150; break;
    default:
        d.preset = D_TOM;    d.fnum = 344; d.block = 4; d.ms = 150; break;
    }
    return d;
}

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

/* Normalisation target that preserves the bank's instrument balance: full
   scale attenuated by the carrier total level (0.75 dB per step, loudest
   voice wins for doubled instruments). Chip output alone is too quiet and
   drifts with FM depth, so normalise-then-attenuate beats a fixed gain. */
static int32_t voice_norm_target(const fm_voice *v1, const fm_voice *v2) {
    static const float frac[6] = { 1.0f, 0.891f, 0.794f, 0.708f, 0.631f, 0.562f };
    uint8_t tl = v1->fm->car_lvl & 0x3F;
    float att, t;
    int steps;
    if (v2) {
        uint8_t tl2 = v2->fm->car_lvl & 0x3F;
        if (tl2 < tl) tl = tl2;
    }
    att = 0.75f * (float)tl;
    steps = (int)(att / 6.0f);
    t = 31000.0f * frac[(int)(att - (float)steps * 6.0f)];
    while (steps--) t *= 0.5f;
    if (t < 400.0f) t = 400.0f;
    return (int32_t)t;
}

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
    normalise_to(s->data, hold + 2, voice_norm_target(v1, v2), 0);
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
    normalise_to(s->data, n, voice_norm_target(v1, v2), 128);

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

/* Note-off release for a bank voice, from the carrier's release rate. */
static float voice_release(const fm_patch *fm) {
    float t = opl_rate_seconds(fm->car_sr & 0x0F);
    if (t > 1.5f) t = 1.5f;
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

        if (op2_active && key >= 35 && key <= 81) {
            /* GENMIDI percussion record: fixed note + voice note offsets. */
            const uint8_t *rec = op2_bank + (128 + (key - 35)) * OP2_RECSIZE;
            uint16_t flags = (uint16_t)(rec[0] | (rec[1] << 8));
            double fixed_hz = note_hz(rec[3] & 0x7F);
            fm_patch fm1, fm2;
            fm_voice v1, v2;
            int16_t off1, off2;
            op2_voice(rec + 4, &fm1, &off1);
            v1.fm = &fm1;
            hz_to_fnum(fixed_hz * semitone_ratio(off1), &v1.fnum, &v1.block);
            if (flags & OP2_FLAG_DOUBLE) {
                op2_voice(rec + 20, &fm2, &off2);
                v2.fm = &fm2;
                hz_to_fnum(fixed_hz * semitone_ratio(off2) * op2_fine_ratio(rec[2]),
                           &v2.fnum, &v2.block);
            }
            s = render_oneshot(chip, &v1, (flags & OP2_FLAG_DOUBLE) ? &v2 : NULL,
                               voice_oneshot_len(&fm1), key_hz,
                               voice_release(&fm1));
        } else {
            drum_voice d = drum_for_key(key);
            fm_voice v1;
            v1.fm = &drum_fm[d.preset];
            v1.fnum = d.fnum;
            v1.block = d.block;
            s = render_oneshot(chip, &v1, NULL,
                               (uint32_t)((uint64_t)_WM_SampleRate * d.ms / 1000u),
                               key_hz, 0.08f);
            if (s) {
                /* Built-in drums bake shorter decays; match the mixer env. */
                mix_env e;
                e.sustain = 0;
                e.attack = 0.001f;
                e.decay = e.release = (float)d.ms / 1000.0f;
                set_envelope(s, &e);
            }
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
        double roots[3], ratio;
        int i;

        if (op2_active) {
            rec = op2_bank + program * OP2_RECSIZE;
            flags = (uint16_t)(rec[0] | (rec[1] << 8));
            op2_voice(rec + 4, &fm1, &off1);
            if (flags & OP2_FLAG_DOUBLE) {
                op2_voice(rec + 20, &fm2, &off2);
                doubled = 1;
            }
            /* The .op2 voice carries its own attack in the render, so the
               mixer env just gates, releasing at the carrier's own rate. */
            op2_env.sustain = 4194303;
            op2_env.attack = 0.001f;
            op2_env.decay = 0.5f;
            op2_env.release = voice_release(&fm1);
            env = &op2_env;
        } else {
            fm1 = gm_fm[program];
            env = &gm_env[program >> 3];
        }
        v1.fm = &fm1;
        v2.fm = &fm2;
        ratio = semitone_ratio(off1);

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
                /* Voice 2 keeps its offset/detune relative to voice 1. */
                hz_to_fnum(base_hz * semitone_ratio(off2 - off1)
                               * op2_fine_ratio(rec[2]),
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
            roots[i] = claimed;
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

    for (id = 0; id < 128; id++) {
        struct _patch *p = alloc_patch(id, 0);
        if (!p) { free_all_patches(); return -1; }
        _WM_patch[id] = p;
    }
    /* Drum kit chains onto _WM_patch[note & 0x7F] because _find_matched_patch
       keys off patchid&0x7F. keep=SAMPLE_ENVELOPE prevents _WM_load_sample
       from stripping envelope mode on drums (see sample.c). */
    for (id = 27; id <= 87; id++) {
        uint16_t drumid = 0x80u | id;
        struct _patch *p = alloc_patch(drumid, SAMPLE_ENVELOPE);
        struct _patch *head = _WM_patch[id];
        if (!p) { free_all_patches(); return -1; }
        while (head->next) head = head->next;
        head->next = p;
    }
    return 0;
}
