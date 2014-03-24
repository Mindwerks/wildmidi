/*
 Copyright (C) 2000 - 2001  Ryan Nunn
 Copyright (C) 2013 - 2014 Bret Curtis

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* XMIDI Converter */

#ifndef XMIDILIB_H
#define XMIDILIB_H

/* Conversion types for Midi files */
#define XMIDI_CONVERT_NOCONVERSION		0
#define XMIDI_CONVERT_MT32_TO_GM		1
#define XMIDI_CONVERT_MT32_TO_GS		2
#define XMIDI_CONVERT_MT32_TO_GS127		3 /* This one is broken, don't use */
#define XMIDI_CONVERT_MT32_TO_GS127DRUM		4 /* This one is broken, don't use */
#define XMIDI_CONVERT_GS127_TO_GS		5

typedef struct {
	unsigned char *buf, *buf_ptr;
	unsigned int size;
} DataSource;

/* Retrieve it to a data source */
int retrieve(unsigned int track, DataSource *source, DataSource *dest);

#endif /* XMIDILIB_H */
