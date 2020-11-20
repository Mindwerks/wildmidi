/*
 * out_dossb.h -- DOS SoundBlaster output
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

#ifndef OUT_DOSSB_H
#define OUT_DOSSB_H

#include "config.h"

#if (AUDIODRV_DOSSB == 1)

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <conio.h>
#include <io.h>
#include <dir.h>
#include "dossb.h"

int open_sb_output(void);
int write_sb_output(int8_t *data, int siz);
void close_sb_output(void);
void pause_sb_output(void);


#else // AUDIODRV_DOSSB == 1

#define open_sb_output open_output_noout
#define write_sb_output send_output_noout
#define close_sb_output close_output_noout
#define pause_sb_output pause_output_noout

#endif // AUDIODRV_DOSSB == 1

#endif // OUT_DOSSB_H
