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

#define WM_EMERGENCY_CONFIG "@emergency"

struct _sample;

/* Fabricate a _sample chain for the given (possibly-drum) patch id.
   Uses the current _WM_SampleRate. Returns NULL on OOM. */
extern struct _sample *_WM_synth_patch(uint16_t patchid);

/* Populate _WM_patch[] with 128 tonal + GM drum entries, all with
   filename==NULL so _WM_load_sample routes them to _WM_synth_patch. */
extern int _WM_emergency_init_patches(void);

#endif /* __SYNTH_H */
