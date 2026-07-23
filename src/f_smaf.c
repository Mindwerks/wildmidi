/*
 * f_smaf.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2026
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

/*
 * Yamaha SMAF ("MMF") support.  SMAF score tracks are converted to a Standard
 * MIDI File by smaf2mid.c and then parsed by the regular MIDI parser, so this
 * file is a thin wrapper that keeps SMAF loading consistent with the other
 * f_*.c format front-ends.
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "wm_error.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "reverb.h"
#include "f_midi.h"
#include "smaf2mid.h"
#include "f_smaf.h"
#ifdef WILDMIDI_MAFM
#include "mafm.h"
#endif

struct _mdi *_WM_ParseNewSmaf(const uint8_t *smaf_data, uint32_t smaf_size) {
    struct _mdi *smaf_mdi = NULL;
    uint8_t *smf_data = NULL;
    uint32_t smf_size = 0;

    if (_WM_smaf2midi(smaf_data, smaf_size, &smf_data, &smf_size) < 0) {
        /* smaf2midi has already set the global error */
        return NULL;
    }

    smaf_mdi = _WM_ParseNewMidi(smf_data, smf_size);
    free(smf_data);

#ifdef WILDMIDI_MAFM
    /* If the file carries a custom FM voice bank (Mtsu voice-exclusives), attach
     * an FM synth so playback uses the chip's real voices instead of the GM
     * approximation.  The note/timing stream still comes from smaf2midi above;
     * the FM synth only replaces the sound generation.  Files with no custom
     * voices keep the plain GM path. */
    if (smaf_mdi != NULL && _WM_MAFM_HasCustomVoices(smaf_data, smaf_size)) {
        smaf_mdi->mafm_synth = _WM_MAFM_NewSynth(smaf_data, smaf_size,
                                                 _WM_SampleRate);
    }
#endif

    return smaf_mdi;
}
