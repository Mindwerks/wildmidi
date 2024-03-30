/*
 * out_coreaudio.h -- CoreAudio output for Mac OS X
 *
 * Copyright (C) WildMidi Developers 2024
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

#ifndef OUT_COREAUDIO_H
#define OUT_COREAUDIO_H

#include "config.h"

#if (AUDIODRV_COREAUDIO == 1)

#include <stdint.h>

int open_coreaudio_output(const char * output);
void pause_coreaudio_output(void);
void resume_coreaudio_output(void);
int write_coreaudio_output(int8_t *output_data, int output_size);
void close_coreaudio_output(void);

#else /* AUDIODRV_COREAUDIO */

#define open_coreaudio_output open_output_noout
#define pause_coreaudio_output pause_output_noout
#define resume_coreaudio_output resume_output_noout
#define write_coreaudio_output send_output_noout
#define close_coreaudio_output close_output_noout

#endif

#endif /* OUT_COREAUDIO_H */
