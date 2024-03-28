/*
 * out_wave.h -- WAVE output
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

#ifndef OUT_WAVE_H
#define OUT_WAVE_H

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if (AUDIODRV_WAVE == 1)

int open_wav_output(const char * output);
int write_wav_output(int8_t *output_data, int output_size);
void close_wav_output(void);

#else // AUDIODRV_WAVE == 1

#define open_wav_output open_output_noout
#define write_wav_output send_output_noout
#define close_wav_output close_output_noout

#endif // AUDIODRV_WAVE == 1

#endif // OUT_WAVE_H
