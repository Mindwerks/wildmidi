/*
 * mafm.h -- Yamaha MA-series FM rendering for SMAF files.
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
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 *
 * This is the WildMIDI-side wrapper around the (Apache-2.0) FM synth core under
 * src/mafm/.  It follows the same shape as sf2.h: a per-mdi synth instance that
 * consumes WildMIDI events and renders PCM into the mix buffer.  The
 * SMAF-specific instrument-bank parsing (Mtsu voice-exclusives + ATR/Awa waves)
 * lives here with the synth, so the SMAF->MIDI converter (smaf2mid.c) stays
 * format-generic.  See docs/SMAF_FM.md.
 */

#ifndef __MAFM_H
#define __MAFM_H

#include <stdint.h>

struct _mdi;
struct _event;

/* Does this .mmf carry custom FM voice data (an Mtsu voice bank) worth
 * synthesising?  If not, the caller should use the plain GM conversion path. */
int _WM_MAFM_HasCustomVoices(const uint8_t *smaf, uint32_t size);

/* Create / free a per-mdi FM synth instance at the given output rate.  The
 * synth parses the .mmf's voice + wave banks on creation. */
void *_WM_MAFM_NewSynth(const uint8_t *smaf, uint32_t size, uint16_t rate);
void  _WM_MAFM_FreeSynth(void *synth);
void  _WM_MAFM_Reset(void *synth);

/* Translate a WildMIDI event to the synth. */
void  _WM_MAFM_Event(void *synth, struct _mdi *mdi, struct _event *event);

/* Nonzero while notes are still sounding (release tails). */
int   _WM_MAFM_ActiveVoices(void *synth);

/* Render stereo frames into the 32-bit mix buffer (accumulates). */
void  _WM_MAFM_Render(void *synth, int32_t *out, uint32_t frames);

#endif /* __MAFM_H */
