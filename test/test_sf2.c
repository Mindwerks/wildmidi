/* test_sf2.c -- checks that a midi renders audibly through a SoundFont2 file
 *
 * usage: test_sf2 <config: .sf2 file or .cfg with a soundfont line> <midi file>
 *
 * Copyright (C) WildMIDI Developers 2026
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 */

#include <stdio.h>
#include <stdint.h>

#include "wildmidi_lib.h"

int main(int argc, char **argv) {
    midi *m;
    int8_t buffer[16384];
    int res, i;
    long total = 0;
    int nonsilent = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <sf2-or-cfg> <midi>\n", argv[0]);
        return 1;
    }

    if (WildMidi_Init(argv[1], 44100, 0) == -1) {
        fprintf(stderr, "WildMidi_Init(%s) failed: %s\n", argv[1], WildMidi_GetError());
        return 1;
    }

    m = WildMidi_Open(argv[2]);
    if (m == NULL) {
        fprintf(stderr, "WildMidi_Open(%s) failed: %s\n", argv[2], WildMidi_GetError());
        WildMidi_Shutdown();
        return 1;
    }

    while ((res = WildMidi_GetOutput(m, buffer, sizeof(buffer))) > 0) {
        total += res;
        for (i = 0; i < res; i++) {
            if (buffer[i] != 0) {
                nonsilent = 1;
                break;
            }
        }
    }

    WildMidi_Close(m);
    WildMidi_Shutdown();

    if (res < 0) {
        fprintf(stderr, "WildMidi_GetOutput failed: %s\n", WildMidi_GetError());
        return 1;
    }
    if (total == 0 || !nonsilent) {
        fprintf(stderr, "rendered %ld bytes, nonsilent=%d: expected audible output\n", total, nonsilent);
        return 1;
    }

    printf("OK: rendered %ld bytes of audio\n", total);
    return 0;
}
