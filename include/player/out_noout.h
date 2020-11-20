/*
 * out_noout.h -- NULL output
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

#ifndef OUT_NOOUT_H
#define OUT_NOOUT_H

#include <stdint.h>

#include "wildplay.h"

static inline int open_output_noout(const char * output) {
    UNUSED(output);
    fprintf(stderr, "Selected audio output driver was not enabled at compile time.\r\n");
    return (-1);
}
static inline int send_output_noout(int8_t *output_data, int output_size) {
  UNUSED(output_data);
  UNUSED(output_size);
  return (-1);
}
static inline void close_output_noout(void) {}
static inline void pause_output_noout(void) {}
static inline void resume_output_noout(void) {}

#endif // OUT_NOOUT_H
