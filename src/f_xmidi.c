/*
 * xmi.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2016
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
#include "f_xmidi.h"


struct _mdi *_WM_ParseNewXmi(uint8_t *xmi_data, uint32_t xmi_size) {
    struct _mdi *xmi_mdi = NULL;
    uint32_t xmi_tmpdata = 0;
    uint8_t xmi_formcnt = 0;
    uint32_t xmi_catlen = 0;
    uint32_t xmi_subformlen = 0;
    uint32_t i = 0;
    uint32_t j = 0;

    uint32_t xmi_evntlen = 0;
    uint32_t xmi_divisions = 60;
    uint32_t xmi_tempo = 500000;
    uint32_t xmi_sample_count = 0;
    float xmi_sample_count_f = 0.0;
    float xmi_sample_remainder = 0.0;
    float xmi_samples_per_delta_f = 0.0;
    uint8_t xmi_ch = 0;
    uint8_t xmi_note = 0;
    uint32_t *xmi_notelen = NULL;

    uint32_t setup_ret = 0;
    uint32_t xmi_delta = 0;
    uint32_t xmi_lowestdelta = 0;

    uint32_t xmi_evnt_cnt = 0;


    if (memcmp(xmi_data,"FORM",4)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
        return NULL;
    }

    xmi_data += 4;
    xmi_size -= 4;

    // bytes until next entry
    xmi_tmpdata = *xmi_data++ << 24;
    xmi_tmpdata |= *xmi_data++ << 16;
    xmi_tmpdata |= *xmi_data++ << 8;
    xmi_tmpdata |= *xmi_data++;
    xmi_size -= 4;

    if (memcmp(xmi_data,"XDIRINFO",8)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
        return NULL;
    }
    xmi_data += 8;
    xmi_size -= 8;

    /*
     0x00 0x00 0x00 0x02 at this point are unknown
     so skip over them
     */
    xmi_data += 4;
    xmi_size -= 4;

    // number of forms contained after this point
    xmi_formcnt = *xmi_data++;
    if (xmi_formcnt == 0) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
        return NULL;
    }
    xmi_size--;

    /*
     at this stage unsure if remaining data in
     this section means anything
     */
    xmi_tmpdata -= 13;
    xmi_data += xmi_tmpdata;
    xmi_size -= xmi_tmpdata;

    /* FIXME: Check: may not even need to process CAT information */
    if (memcmp(xmi_data,"CAT ",4)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
        return NULL;
    }
    xmi_data += 4;
    xmi_size -= 4;

    xmi_catlen = *xmi_data++ << 24;
    xmi_catlen |= *xmi_data++ << 16;
    xmi_catlen |= *xmi_data++ << 8;
    xmi_catlen |= *xmi_data++;
    xmi_size -= 4;

    UNUSED(xmi_catlen);

    if (memcmp(xmi_data,"XMID",4)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
        return NULL;
    }
    xmi_data += 4;
    xmi_size -= 4;

    xmi_mdi = _WM_initMDI();
    _WM_midi_setup_divisions(xmi_mdi, xmi_divisions);
    _WM_midi_setup_tempo(xmi_mdi, xmi_tempo);

    xmi_samples_per_delta_f = _WM_GetSamplesPerTick(xmi_divisions, xmi_tempo);

    xmi_notelen = malloc(sizeof(uint32_t) * 16 * 128);
    memset(xmi_notelen, 0, (sizeof(uint32_t) * 16 * 128));

    for (i = 0; i < xmi_formcnt; i++) {
        if (memcmp(xmi_data,"FORM",4)) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
            goto _xmi_end;
        }
        xmi_data += 4;
        xmi_size -= 4;

        xmi_subformlen = *xmi_data++ << 24;
        xmi_subformlen |= *xmi_data++ << 16;
        xmi_subformlen |= *xmi_data++ << 8;
        xmi_subformlen |= *xmi_data++;
        xmi_size -= 4;

        if (memcmp(xmi_data,"XMID",4)) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
            goto _xmi_end;
        }
        xmi_data += 4;
        xmi_size -= 4;
        xmi_subformlen -= 4;

        // Process Subform
        do {
            if (!memcmp(xmi_data,"TIMB",4)) {
                // Holds patch information
                // FIXME: May not be needed for playback as EVNT seems to
                //        hold patch events
                xmi_data += 4;

                xmi_tmpdata = *xmi_data++ << 24;
                xmi_tmpdata |= *xmi_data++ << 16;
                xmi_tmpdata |= *xmi_data++ << 8;
                xmi_tmpdata |= *xmi_data++;
                xmi_data += xmi_tmpdata;
                xmi_size -= (8 + xmi_tmpdata);
                xmi_subformlen -= (8 + xmi_tmpdata);

            } else if (!memcmp(xmi_data,"RBRN",4)) {
                // Unknown what this is
                // FIXME: May not be needed for playback
                xmi_data += 4;

                xmi_tmpdata = *xmi_data++ << 24;
                xmi_tmpdata |= *xmi_data++ << 16;
                xmi_tmpdata |= *xmi_data++ << 8;
                xmi_tmpdata |= *xmi_data++;
                xmi_data += xmi_tmpdata;
                xmi_size -= (8 + xmi_tmpdata);
                xmi_subformlen -= (8 + xmi_tmpdata);

            } else if (!memcmp(xmi_data,"EVNT",4)) {
                // EVNT is where all the MIDI music information is stored
                xmi_data += 4;

                xmi_evnt_cnt++;

                xmi_evntlen = *xmi_data++ << 24;
                xmi_evntlen |= *xmi_data++ << 16;
                xmi_evntlen |= *xmi_data++ << 8;
                xmi_evntlen |= *xmi_data++;
                xmi_size -= 8;
                xmi_subformlen -= 8;

                do {
                    if (*xmi_data < 0x80) {
                        xmi_delta = 0;
                        if (*xmi_data > 0x7f) {
                            while (*xmi_data > 0x7f) {
                                xmi_delta = (xmi_delta << 7) | (*xmi_data++ & 0x7f);
                                xmi_size--;
                                xmi_evntlen--;
                                xmi_subformlen--;
                            }
                        }
                        xmi_delta = (xmi_delta << 7) | (*xmi_data++ & 0x7f);
                        xmi_size--;
                        xmi_evntlen--;
                        xmi_subformlen--;

                        do {
                            // determine delta till next event
                            if ((xmi_lowestdelta != 0) && (xmi_lowestdelta <= xmi_delta)) {
                                xmi_tmpdata = xmi_lowestdelta;
                            } else {
                                xmi_tmpdata = xmi_delta;
                            }

                            xmi_sample_count_f= (((float) xmi_tmpdata * xmi_samples_per_delta_f) + xmi_sample_remainder);

                            xmi_sample_count = (uint32_t) xmi_sample_count_f;
                            xmi_sample_remainder = xmi_sample_count_f - (float) xmi_sample_count;

                            xmi_mdi->events[xmi_mdi->event_count - 1].samples_to_next += xmi_sample_count;
                            xmi_mdi->extra_info.approx_total_samples += xmi_sample_count;

                            xmi_lowestdelta = 0;

                            // scan through on notes
                            for (j = 0; j < (16*128); j++) {
                                // only want notes that are on
                                if (xmi_notelen[j] == 0) continue;

                                // remove delta to next event from on notes
                                xmi_notelen[j] -= xmi_tmpdata;

                                // Check if we need to turn note off
                                if (xmi_notelen[j] == 0) {
                                    xmi_ch = j / 128;
                                    xmi_note = j - (xmi_ch * 128);
                                    _WM_midi_setup_noteoff(xmi_mdi, xmi_ch, xmi_note, 0);
                                } else {
                                    // otherwise work out new lowest delta
                                    if ((xmi_lowestdelta == 0) || (xmi_lowestdelta > xmi_notelen[j])) {
                                        xmi_lowestdelta = xmi_notelen[j];
                                    }
                                }
                            }
                            xmi_delta -= xmi_tmpdata;
                        } while (xmi_delta);

                    } else {
                        if ((xmi_data[0] == 0xff) && (xmi_data[1] == 0x51) && (xmi_data[2] == 0x03)) {
                            // Ignore tempo events
                            setup_ret = 6;
                            goto _XMI_Next_Event;
                        }
                        if ((setup_ret = _WM_SetupMidiEvent(xmi_mdi,xmi_data,0)) == 0) {
                            goto _xmi_end;
                        }

                        if ((*xmi_data & 0xf0) == 0x90) {
                            // Note on has extra data stating note length
                            xmi_ch = *xmi_data & 0x0f;
                            xmi_note = xmi_data[1];
                            xmi_data += setup_ret;
                            xmi_size -= setup_ret;
                            xmi_evntlen -= setup_ret;
                            xmi_subformlen -= setup_ret;

                            xmi_tmpdata = 0;

                            if (*xmi_data > 0x7f) {
                                while (*xmi_data > 0x7f) {
                                    xmi_tmpdata = (xmi_tmpdata << 7) | (*xmi_data++ & 0x7f);
                                    xmi_size--;
                                    xmi_evntlen--;
                                    xmi_subformlen--;
                                }
                            }
                            xmi_tmpdata = (xmi_tmpdata << 7) | (*xmi_data++ & 0x7f);
                            xmi_size--;
                            xmi_evntlen--;
                            xmi_subformlen--;

                            // store length
                            xmi_notelen[128 * xmi_ch + xmi_note] = xmi_tmpdata;
                            if ((xmi_tmpdata > 0) && ((xmi_lowestdelta == 0) || (xmi_tmpdata < xmi_lowestdelta))) {
                                xmi_lowestdelta = xmi_tmpdata;
                            }

                        } else {
                        _XMI_Next_Event:
                            xmi_data += setup_ret;
                            xmi_size -= setup_ret;
                            xmi_evntlen -= setup_ret;
                            xmi_subformlen -= setup_ret;
                        }
                    }

                } while (xmi_evntlen);

            } else {
                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_XMI, NULL, 0);
                goto _xmi_end;
            }

        } while (xmi_subformlen);
    }

    // Finalise mdi structure
    if ((xmi_mdi->reverb = _WM_init_reverb(_WM_SampleRate, _WM_reverb_room_width, _WM_reverb_room_length, _WM_reverb_listen_posx, _WM_reverb_listen_posy)) == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to init reverb", 0);
        goto _xmi_end;
    }
    xmi_mdi->extra_info.current_sample = 0;
    xmi_mdi->current_event = &xmi_mdi->events[0];
    xmi_mdi->samples_to_mix = 0;
    xmi_mdi->note = NULL;
    /* More than 1 event form in XMI means treat as type 2 */
    if (xmi_evnt_cnt > 1) {
        xmi_mdi->is_type2 = 1;
    }
    _WM_ResetToStart(xmi_mdi);

_xmi_end:
    if (xmi_notelen != NULL) free(xmi_notelen);
    if (xmi_mdi->reverb) return (xmi_mdi);
    _WM_freeMDI(xmi_mdi);
    return NULL;
}
