/*
 * HMP2MIDI: HMI Sound Operating System HMP to MIDI Library Header
 *
 * Copyright (C) WildMIDI Developers 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef HMPLIB_H
#define HMPLIB_H

#include <stdint.h>

int _WM_hmp2midi(const uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize);

/* Gremlin Interactive "mangled" (compressed) HMP/HMI files,
 * as shipped by Fatal Racing / Whiplash */
int _WM_IsMangled(const uint8_t *in, uint32_t insize);
int _WM_Unmangle(const uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize);

#endif /* HMPLIB_H */
