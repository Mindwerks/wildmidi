/*
	XMIDI: Miles XMIDI to MID Library Header

	Copyright (C) 2001  Ryan Nunn
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

/* XMIDI Converter */

#ifndef XMIDILIB_H
#define XMIDILIB_H

#include <stdint.h>

/* Conversion types for Midi files */
#define XMIDI_CONVERT_NOCONVERSION		0
#define XMIDI_CONVERT_MT32_TO_GM		1
#define XMIDI_CONVERT_MT32_TO_GS		2
#define XMIDI_CONVERT_MT32_TO_GS127		3 /* This one is broken, don't use */
#define XMIDI_CONVERT_MT32_TO_GS127DRUM		4 /* This one is broken, don't use */
#define XMIDI_CONVERT_GS127_TO_GS		5

struct xmi_ctx;

struct xmi_ctx *xmi2midi(uint8_t *data, uint32_t size, int convert_type);
uint8_t *xmi_getmididata(struct xmi_ctx *);
uint32_t xmi_getmidisize(struct xmi_ctx *);
void xmi_free(struct xmi_ctx *);

#endif /* XMIDILIB_H */
