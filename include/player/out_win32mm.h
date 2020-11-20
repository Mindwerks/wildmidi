/*
 * out_win32mm.h -- Windows MM output
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

#ifndef OUT_WIN32MM_H
#define OUT_WIN32MM_H

#include "config.h"

#if (AUDIODRV_WIN32_MM == 1)

#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#include <mmsystem.h>   // should be after windows.h

#include "getopt_long.h"

#ifdef __WATCOMC__
#define _putch putch
#endif

int open_mm_output(const char * output);
int write_mm_output(int8_t *output_data, int output_size);
void close_mm_output(void);

#else

#define open_mm_output open_output_noout
#define write_mm_output send_output_noout
#define close_mm_output close_output_noout

#endif


#endif // OUT_WIN32MM_H
