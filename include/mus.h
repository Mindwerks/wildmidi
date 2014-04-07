/*
	MUS2MIDI: DMX (DOOM) MUS to MIDI Library Header

	Copyright (C) 2014  Bret Curtis

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef MUSLIB_H
#define MUSLIB_H

#include <stdint.h>

struct mus_ctx;

struct mus_ctx *mus2midi(uint8_t *data, uint32_t size);
uint8_t *mus_getmididata(struct mus_ctx *);
uint32_t mus_getmidisize(struct mus_ctx *);
void mus_free(struct mus_ctx *);

#endif /* MUSLIB_H */
