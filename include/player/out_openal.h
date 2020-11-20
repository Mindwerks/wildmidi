/*
 * out_openal.h -- OpenAL output
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

#ifndef OUT_OPENAL_H
#define OUT_OPENAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#if (AUDIODRV_OPENAL == 1)

#ifndef __APPLE__
#include <al.h>
#include <alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

int open_openal_output(const char * output);
void pause_output_openal(void);
int write_openal_output(int8_t *output_data, int output_size);
void close_openal_output(void);

#else // AUDIODRV_OPENAL == 1

#define open_openal_output open_output_noout
#define pause_output_openal pause_output_noout
#define write_openal_output send_output_noout
#define close_openal_output close_output_noout

#endif // AUDIODRV_OPENAL == 1

#endif // OUT_OPENAL_H
