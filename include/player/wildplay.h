/*
 * wildplay.h -- WildMidi player header
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

#ifndef WILDPLAY_H
#define WILDPLAY_H

#include <stdint.h>

// Macros to suppress unused variables warnings
#define UNUSED(x) (void)(x)

// Supported sound backends
enum {
    NO_OUT,       // No out
    WAVE_OUT,     // WAVe raw output
    ALSA_OUT,     // ALSA
    OSS_OUT,      // OSS
    OPENAL_OUT,   // OpenAL
    AHI_OUT,      // Amiga AHI output
    WIN32_MM_OUT, // Windows native output
    OS2DART_OUT,  // DART OS/2 output
    DOSSB_OUT,    // SoundBlaster output (DOS)
    // Add here new output backends

    TOTAL_OUT     // Total supported outputs
};

typedef struct {
    char * name;
    char * description;
    int enabled;
    int (* open_out)(const char *);
    int (* send_out)(int8_t *, int);
    void (* close_out)();
    void (* pause_out)();
    void (* resume_out)();
} wildmidi_info;

#endif // __WILDPLAY_H
