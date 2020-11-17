/*
 * out_dart.h -- OS/2 DART output
 *
 * Copyright (C) WildMidi Developers 2020
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

#ifndef OUT_DART_H
#define OUT_DART_H

#include "config.h"

#if (AUDIODRV_OS2DART == 1)

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_OS2MM
#ifdef __EMX__
#define INCL_KBD
#define INCL_VIO
#endif
#include <os2.h>
#include <os2me.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#define msleep(s) DosSleep((s))
#include <fcntl.h>
#include <io.h>
#include "getopt_long.h"


int open_dart_output(void);
int write_dart_output(int8_t *output_data, int output_size);
void close_dart_output(void);

#else // AUDIODRV_OS2DART == 1

#define open_dart_output open_output_noout
#define write_dart_output send_output_noout
#define close_dart_output close_output_noout


#endif // AUDIODRV_OS2DART == 1

#endif // OUT_DART_H
