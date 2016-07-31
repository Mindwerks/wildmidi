/*
 * common.h -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2016
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

#ifndef __COMMON_H
#define __COMMON_H

#ifndef __VBCC__
#define UNUSED(x) (void)(x)
#else
#define UNUSED(x) /* vbcc emits an annoying warning for (void)(x) */
#endif
#define MEM_CHUNK 8192

extern int16_t _WM_MasterVolume;
extern uint16_t _WM_SampleRate;
extern uint16_t _WM_MixerOptions;

extern float _WM_reverb_room_width;  /* = 16.875f; */
extern float _WM_reverb_room_length; /* = 22.5f;   */

extern float _WM_reverb_listen_posx; /* = 8.4375f; */
extern float _WM_reverb_listen_posy; /* = 16.875f; */

extern void _cvt_reset_options (void);
extern uint16_t _cvt_get_option (uint16_t tag);

/* Set our global defines here */
#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#endif /* __COMMON_H */
