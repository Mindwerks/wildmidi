/*
 * out_alsa.h -- ALSA output
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

#ifndef OUT_ALSA_H
#define OUT_ALSA_H

#include "config.h"

#if (AUDIODRV_ALSA == 1)

#include <alsa/asoundlib.h>

int open_alsa_output(void);
int write_alsa_output(int8_t *output_data, int output_size);
void close_alsa_output(void);

#else

#define open_alsa_output open_output_noout
#define pause_alsa_openal pause_output_noout
#define write_alsa_output send_output_noout
#define close_alsa_output close_output_noout

#endif



#endif // OUT_ALSA_H
