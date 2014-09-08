/*
 xmi.c
 
 Midi Wavetable Processing library
 
 Copyright (C) WildMIDI Developers 2001-2014
 
 This file is part of WildMIDI.
 
 WildMIDI is free software: you can redistribute and/or modify the player
 under the terms of the GNU General Public License and you can redistribute
 and/or modify the library under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either version 3 of
 the licenses, or(at your option) any later version.
 
 WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 the GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU General Public License and the
 GNU Lesser General Public License along with WildMIDI.  If not,  see
 <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "wm_error.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "reverb.h"
#include "xmi.h"


struct _mdi *_WM_ParseNewXmi(uint8_t *xmi_data, uint32_t xmi_size) {
    struct _mdi *xmi_mdi;
    struct _xmi_noteoff {
        uint32_t samples;
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
    } *xmi_noteoff = NULL;
    uint32_t xmi_noteoff_count = 0;
    uint32_t i;

    xmi_mdi = _WM_initMDI();

    
    /* ... some code here ... */
    
    if (xmi_noteoff_count) {
        // now that we have samples until next, find lowest sample count until
        // next noteoff event.
        uint32_t lowest_noteoff_samples = 0;
        
        for (i = 0; i < xmi_noteoff_count; i++) {
        }
    }
    
    /* ... some code here ... */
    
    
    free(xmi_mdi);
    return NULL;
}
