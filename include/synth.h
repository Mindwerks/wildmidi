/*
 * synth.h -- Built-in synthesiser: fabricates _sample chains so WildMIDI
 *            plays even without a GUS patch set or SoundFont.
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
 */

#ifndef __SYNTH_H
#define __SYNTH_H

#include <stdint.h>

#define WM_OPL3_CONFIG "@opl3"

struct _sample;

/* Fabricate a _sample chain for the given (possibly-drum) patch id.
   Uses the current _WM_SampleRate. Returns NULL on OOM. */
extern struct _sample *_WM_synth_patch(uint16_t patchid);

/* Populate _WM_patch[] with 128 tonal + GM drum entries, all with
   filename==NULL so _WM_load_sample routes them to _WM_synth_patch. */
extern int _WM_opl3_init_patches(void);

/* GENMIDI (.op2) FM instrument bank support: when a bank is loaded, the
   OPL3 synth renders its register data instead of the built-in table.
   DMXOPL (MIT) is a known-good GM bank in this format. */
extern int _WM_OP2_Magic(const uint8_t *data, uint32_t size);
extern int _WM_OP2_Load(const uint8_t *data, uint32_t size);
extern void _WM_OP2_Unload(void);

#endif /* __SYNTH_H */
