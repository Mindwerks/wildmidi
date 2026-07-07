/*
 * sf2.h -- SoundFont2 rendering via TinySoundFont
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
 */

#ifndef __SF2_H
#define __SF2_H

struct _mdi;
struct _event;

/* returns 1 if the buffer looks like a RIFF sfbk (SoundFont2) file */
extern int _WM_SF2_Magic(const uint8_t *data, uint32_t size);

/* load/free the global soundfont instance */
extern int _WM_SF2_Load(const uint8_t *data, uint32_t size);
extern void _WM_SF2_Unload(void);
extern int _WM_SF2_Active(void);

/* per-mdi synth instances (voices private, sample data shared) */
extern void *_WM_SF2_NewSynth(uint16_t rate);
extern void _WM_SF2_FreeSynth(void *synth);
extern void _WM_SF2_Reset(void *synth);

/* translate a wildmidi event to the synth */
extern void _WM_SF2_Event(void *synth, struct _mdi *mdi, struct _event *event);

/* returns nonzero while notes are still sounding (release tails) */
extern int _WM_SF2_ActiveVoices(void *synth);

/* render stereo frames into the 32bit mix buffer */
extern void _WM_SF2_Render(void *synth, int32_t *out, uint32_t frames);

#endif /* __SF2_H */
