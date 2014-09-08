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
        float samples;
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
    } *xmi_noteoff = NULL;
    uint32_t xmi_noteoff_count = 0;
    float xmi_lowest_noteoff_samples = 0;
    uint32_t i;

    xmi_mdi = _WM_initMDI();

    
    /* ... some code here ... */

    /* ... Note On code here ... */
    xmi_note = 0; // placeholder until note on code is done.
    xmi_channel =0; // placeholder until note on code is done.
    xmi_velocity =0; // placeholder until note on code is done.
    xmi_note_duration = 0; // place holder until note on code is done.

    /* ... Convert duration to samples ... */
    xmi_samples = (float)xmi_duration * samples_per_tick;

    /* Store Note On Duration */
    xmi_noteoff_count++;
    xmi_noteoff = realloc(xmi_noteoff, (sizeof(struct _xmi_noteoff) * xmi_noteoff_count));
    xmi_noteoff[xmi_noteoff_count-1]->samples = xmi_samples;
    xmi_noteoff[xmi_noteoff_count-1]->note = xmi_note;
    xmi_noteoff[xmi_noteoff_count-1]->channel = xmi_channel;
    xmi_noteoff[xmi_noteoff_count-1]->velocity = xmi_velocity;

    /* Check if duration is smallest */
    if ((xmi_samples < lowest_noteoff_samples) || (lowest_noteoff_samples ==0)) {
        lowest_noteoff_samples = xmi_samples;
    }
    
    /* ... Some code here ... */

    /* Get samples until next event */
    samples_f_till_next = 0.0; // place holder until the code for this is done.

    if ((samples_f_till_next > 0.0) && (lowest_noteoff_samples > 0.0) && (xmi_noteoff_count)) {
        i = 0;
        tmp_lowest = 0.0;
        do {
            xmi_noteoff[i]->samples -= lowest_noteoff_samples;
            if (xmi_noteoff[i]->samples <= 0.0) {
                /* Time to insert noteoff event */

                /* Remove xmi_noteoff entry */

            } else {
                if ((xmi_noteoff[i]->samples < tmp_lowest) || (tmp_lowest ==0)) {
                    tmp_lowest = xmi_noteoff[i]->samples;
                }
                i++;
            }
        } while (i < xmi_noteoff_count);
        lowest_noteoff_samples = tmp_lowest;
    }
    
    /* Insert time until next event */

    /* ... some code here ... */
    
    
    free(xmi_mdi);
    return NULL;
}
