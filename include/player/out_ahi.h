/*
 * out_ahi.h -- Amiga AHI output
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

#ifndef OUT_AHI_H
#define OUT_AHI_H

#include "config.h"

#if (AUDIODRV_AHI == 1)

#include <proto/exec.h>
#include <proto/dos.h>
#include "getopt_long.h"
#include <devices/ahi.h>

#ifdef __amigaos4__
#define SHAREDMEMFLAG MEMF_SHARED
#else
#define SHAREDMEMFLAG MEMF_PUBLIC
#endif

int open_ahi_output(void);
int write_ahi_output(int8_t *output_data, int output_size);
void close_ahi_output(void);

#else // AUDIODRV_AHI == 1

#define open_ahi_output open_output_noout
#define write_ahi_output send_output_noout
#define close_ahi_output close_output_noout

#endif // AUDIODRV_AHI == 1

#endif // OUT_AHI_H
