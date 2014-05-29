/*
 midi.c
 
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "wm_error.h"
#include "midi.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "reverb.h"


struct _mdi *
_WM_ParseNewMidi(uint8_t *midi_data, uint32_t midi_size) {
	struct _mdi *mdi;
    
	uint32_t tmp_val;
	uint32_t midi_type;
    uint32_t track_size;
	uint8_t **tracks;
	uint32_t end_of_tracks = 0;
	uint32_t no_tracks;
	uint32_t i;
	uint32_t divisions = 96;
	uint32_t tempo = 500000;
	float samples_per_delta_f = 0.0;
	float pulses_per_second_f = 0.0;
    float bpm_f = 0.0;
    
	uint32_t sample_count = 0;
	float sample_count_f = 0.0;
	float sample_remainder = 0.0;
	uint8_t *sysex_store = NULL;
	//uint32_t sysex_store_len = 0;
    
	uint32_t *track_delta;
	uint8_t *track_end;
	uint32_t smallest_delta = 0;
	uint32_t subtract_delta = 0;
	//uint32_t tmp_length = 0;
	//uint8_t current_event = 0;
	//uint8_t current_event_ch = 0;
	uint8_t *running_event;
	uint32_t decay_samples = 0;
    
	uint8_t *cvt = NULL;
	uint32_t cvt_size;

    
#if 0
    /*
        This will be done using a seperate _WM_ParseNewXmi/Mus
        so a double conversion is not needed for playback.
     */
	if (!memcmp(midi_data, "FORM", 4)) {
		if (_WM_xmi2midi(midi_data, midi_size, &cvt, &cvt_size,
                     _cvt_get_option(WM_CO_XMI_TYPE)) < 0) {
			return (NULL);
		}
		midi_data = cvt;
		midi_size = cvt_size;
	}
	else if (!memcmp(midi_data, "MUS", 3)) {
		if (_WM_mus2midi(midi_data, midi_size, &cvt, &cvt_size) < 0) {
			return (NULL);
		}
		midi_data = cvt;
		midi_size = cvt_size;
	}
	else
#endif
    if (!memcmp(midi_data, "RIFF", 4)) {
		midi_data += 20;
		midi_size -= 20;
	}
    
	if (memcmp(midi_data, "MThd", 4)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_MIDI, NULL, 0);
		free(cvt);
		return (NULL);
	}
	midi_data += 4;
	midi_size -= 4;
    
	if (midi_size < 10) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		free(cvt);
		return (NULL);
	}
    
	/*
	 * Get Midi Header Size - must always be 6
	 */
	tmp_val = *midi_data++ << 24;
	tmp_val |= *midi_data++ << 16;
	tmp_val |= *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 4;
	if (tmp_val != 6) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, NULL, 0);
		free(cvt);
		return (NULL);
	}
    
	/*
	 * Get Midi Format - we only support 0, 1 & 2
	 */
	tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 2;
	if (tmp_val > 2) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, NULL, 0);
		free(cvt);
		return (NULL);
	}
    
    midi_type = tmp_val;
    
	/*
	 * Get No. of Tracks
	 */
	tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 2;
	if (tmp_val < 1) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(no tracks)", 0);
		free(cvt);
		return (NULL);
	}
    no_tracks = tmp_val;
    
    /*
     * Check that type 0 midi file has only 1 track
     */
    if ((midi_type == 0) && (no_tracks > 1)) {
        _WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(expected 1 track for type 0 midi file, found more)", 0);
        free(cvt);
		return (NULL);
    }
    
	/*
	 * Get Divisions
	 */
	divisions = *midi_data++ << 8;
	divisions |= *midi_data++;
	midi_size -= 2;
	if (divisions & 0x00008000) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, NULL, 0);
		free(cvt);
		return (NULL);
	}
    
	if ((_WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
		bpm_f = (float) (60000000 / tempo) + 0.5f;
	} else {
        bpm_f = (float) (60000000 / tempo);
    }
	/* Slow but needed for accuracy */
	pulses_per_second_f = ((float)divisions * bpm_f) / 60.0;
	samples_per_delta_f = (float)_WM_SampleRate / pulses_per_second_f;
    
	mdi = _WM_initMDI();
    _WM_midi_setup_divisions(mdi,divisions);
    
	tracks = malloc(sizeof(uint8_t *) * no_tracks);
	track_delta = malloc(sizeof(uint32_t) * no_tracks);
	track_end = malloc(sizeof(uint8_t) * no_tracks);
	running_event = malloc(sizeof(uint8_t) * no_tracks);
    
    smallest_delta = 0xffffffff;
	for (i = 0; i < no_tracks; i++) {
		if (midi_size < 8) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
			goto _end;
		}
		if (memcmp(midi_data, "MTrk", 4) != 0) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing track header)", 0);
			goto _end;
		}
		midi_data += 4;
		midi_size -= 4;
        
		track_size = *midi_data++ << 24;
		track_size |= *midi_data++ << 16;
		track_size |= *midi_data++ << 8;
		track_size |= *midi_data++;
		midi_size -= 4;
		if (midi_size < track_size) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
			goto _end;
		}
		if ((midi_data[track_size - 3] != 0xFF)
            || (midi_data[track_size - 2] != 0x2F)
            || (midi_data[track_size - 1] != 0x00)) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing EOT)", 0);
			goto _end;
		}
		tracks[i] = midi_data;
		midi_data += track_size;
		midi_size -= track_size;
		track_end[i] = 0;
		running_event[i] = 0;
		track_delta[i] = 0;
		decay_samples = 0;
		while (*tracks[i] > 0x7F) {
			track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
			tracks[i]++;
		}
		track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
		tracks[i]++;
        if (midi_type < 2 ) {
            if (track_delta[i] < smallest_delta) {
                smallest_delta = track_delta[i];
            }
        } else {
            // Type 2 midi only needs delta from 1st track.
            if (i == 0) smallest_delta = track_delta[i];
        }
	}
    
    subtract_delta = smallest_delta;
    sample_count_f = (((float) smallest_delta * samples_per_delta_f) + sample_remainder);
    sample_count = (uint32_t) sample_count_f;
    sample_remainder = sample_count_f - (float) sample_count;
    
    mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
    mdi->info.approx_total_samples += sample_count;
    
    
	/*
	 * Handle type 0 & 1 the same, but type 2 differently
	 */
	switch (midi_type) {
        case 0:
        case 1:
            /* Type 0 & 1 can use the same code*/
            while (end_of_tracks != no_tracks) {
                smallest_delta = 0;
                for (i = 0; i < no_tracks; i++) {
                    if (track_end[i])
                        continue;
                    
                    if (track_delta[i]) {
                        track_delta[i] -= subtract_delta;
                        if (track_delta[i]) {
                            if ((!smallest_delta)
                                || (smallest_delta > track_delta[i])) {
                                smallest_delta = track_delta[i];
                            }
                            continue;
                        }
                    }
                    do {
                        if ((tracks[i][0] == 0xff) && (tracks[i][1] == 0x2f) && (tracks[i][2] == 0x00)) {
                            /* End of Track */
                            end_of_tracks++;
                            track_end[i] = 1;
                            tracks[i] += 3;
                            goto NEXT_TRACK;
                        } else {
                            uint32_t setup_ret = 0;
                            setup_ret = _WM_SetupMidiEvent(mdi, tracks[i], running_event[i]);
                            if (setup_ret == 0) {
                                _WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing event)", 0);
                                goto _end;
                            }
                            if (tracks[i][0] > 0x7f) {
                                if (tracks[i][0] < 0xf0) {
                                    // Events 0x80 - 0xef set running event
                                    running_event[i] = tracks[i][0];
                                } else if ((tracks[i][0] == 0xf0) || (tracks[i][0] == 0xf7)) {
                                    // Sysex resets running event
                                    running_event[i] = 0;
                                } else if ((tracks[i][0] == 0xff) && (tracks[i][1] == 0x51) && (tracks[i][2] == 0x03)) {
                                    /* Tempo */
                                    tempo = (tracks[i][3] << 16) + (tracks[i][4] << 8)+ tracks[i][5];
                                    if (!tempo)
                                        tempo = 500000;
                                    
                                    if ((_WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
                                        bpm_f = (float) (60000000 / tempo) + 0.5f;
                                    } else {
                                        bpm_f = (float) (60000000 / tempo);
                                    }
                                        
                                    /* Slow but needed for accuracy */
                                    pulses_per_second_f = ((float)divisions * bpm_f) / 60.0;
                                    samples_per_delta_f = (float)_WM_SampleRate / pulses_per_second_f;
                                }
                            }
                            tracks[i] += setup_ret;
                        }
                        
                        if (*tracks[i] > 0x7f) {
                            do {
                                track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
                                tracks[i]++;
                            } while (*tracks[i] > 0x7f);
                        }
                        track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
                        tracks[i]++;
                    } while (!track_delta[i]);
                    if ((!smallest_delta) || (smallest_delta > track_delta[i])) {
                        smallest_delta = track_delta[i];
                    }
                NEXT_TRACK: continue;
                }
                
                subtract_delta = smallest_delta;
                sample_count_f = (((float) smallest_delta * samples_per_delta_f)
                                  + sample_remainder);
                sample_count = (uint32_t) sample_count_f;
                sample_remainder = sample_count_f - (float) sample_count;
                
                mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
                mdi->info.approx_total_samples += sample_count;
                /* printf("Decay Samples = %lu\n",decay_samples);*/
                if (decay_samples > sample_count) {
                    decay_samples -= sample_count;
                } else {
                    decay_samples = 0;
                }
            }
            break;
            
        case 2: /* Type 2 has to be handled differently */
            for (i=0; i < no_tracks; i++) {
                sample_remainder = 0.0;
                decay_samples = 0;
                track_delta[i] = 0;
                do {
                    if(track_delta[i]) {
                        sample_count_f = (((float) track_delta[i] * samples_per_delta_f)
                                          + sample_remainder);
                        sample_count = (uint32_t) sample_count_f;
                        sample_remainder = sample_count_f - (float) sample_count;
                        mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
                        mdi->info.approx_total_samples += sample_count;
                        /* printf("Decay Samples = %lu\n",decay_samples);*/
                        if (decay_samples > sample_count) {
                            decay_samples -= sample_count;
                        } else {
                            decay_samples = 0;
                        }
                    }
                    if ((tracks[i][0] == 0xff) && (tracks[i][1] == 0x2f) && (tracks[i][2] == 0x00)) {
                        /* End of Track */
                        
                        // TODO: add call to midi_setup_endoftrack as marker
                        // to be used by _WM_Event2Midi
                        
                        end_of_tracks++;
                        track_end[i] = 1;
                        tracks[i] += 3;
                        goto NEXT_TRACK2;
                    } else {
                        uint32_t setup_ret = 0;
                        setup_ret = _WM_SetupMidiEvent(mdi, tracks[i], running_event[i]);
                        if (setup_ret == 0) {
                            _WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing event)", 0);
                            goto _end;
                        }
                        if (tracks[i][0] > 0x7f) {
                            if (tracks[i][0] < 0xf0) {
                                // Events 0x80 - 0xef set running event
                                running_event[i] = tracks[i][0];
                            } else if ((tracks[i][0] == 0xf0) || (tracks[i][0] == 0xf7)) {
                                // Sysex resets running event
                                running_event[i] = 0;
                            } else if ((tracks[i][0] == 0xff) && (tracks[i][1] == 0x51) && (tracks[i][2] == 0x03)) {
                                /* Tempo */
                                tempo = (tracks[i][3] << 16) + (tracks[i][4] << 8)+ tracks[i][5];
                                if (!tempo)
                                    tempo = 500000;

                                if ((_WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
                                    bpm_f = (float) (60000000 / tempo) + 0.5f;
                                } else {
                                    bpm_f = (float) (60000000 / tempo);
                                }
                                
                                /* Slow but needed for accuracy */
                                pulses_per_second_f = ((float)divisions * bpm_f) / 60.0;
                                samples_per_delta_f = (float)_WM_SampleRate / pulses_per_second_f;
                            }
                        }
                        tracks[i] += setup_ret;
                    }
                    
                    if (*tracks[i] > 0x7f) {
                        do {
                            track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
                            tracks[i]++;
                        } while (*tracks[i] > 0x7f);
                    }
                    
                    track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
                    tracks[i]++;
                NEXT_TRACK2:
                    smallest_delta = track_delta[i]; /* Added just to keep Xcode happy */
                    UNUSED(smallest_delta); /* Added to just keep clang happy */
                } while (track_end[i] == 0);
                /*
                 * Add decay at the end of each song
                 */
                if (decay_samples) {
                    if ((mdi->event_count)
                        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
                        mdi->events[mdi->event_count - 1].samples_to_next += decay_samples;
                    } else {
                        _WM_CheckEventMemoryPool(mdi);
                        mdi->events[mdi->event_count].do_event = NULL;
                        mdi->events[mdi->event_count].event_data.channel = 0;
                        mdi->events[mdi->event_count].event_data.data = 0;
                        mdi->events[mdi->event_count].samples_to_next = decay_samples;
                        mdi->event_count++;
                    }
                }
            }
            break;
            
        default: break; /* Don't expect to get here, added for completeness */
    }
    
	/* Set total MIDI time to 1/1000's seconds */
	mdi->info.total_midi_time = (mdi->info.approx_total_samples * 1000)
    / _WM_SampleRate;
	/*mdi->info.approx_total_samples += _WM_SampleRate * 3;*/
    
	/* Add additional samples needed for decay */
	mdi->info.approx_total_samples += decay_samples;
	/*printf("decay_samples = %lu\n",decay_samples);*/
    
	if ((mdi->reverb = _WM_init_reverb(_WM_SampleRate, _WM_reverb_room_width,
                                   _WM_reverb_room_length, _WM_reverb_listen_posx, _WM_reverb_listen_posy))
        == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to init reverb", 0);
		goto _end;
	}
    
	mdi->info.current_sample = 0;
	mdi->current_event = &mdi->events[0];
	mdi->samples_to_mix = 0;
	mdi->note = NULL;
    
	_WM_ResetToStart(mdi);
    
_end:	free(sysex_store);
	free(cvt);
	free(track_end);
	free(track_delta);
	free(running_event);
	free(tracks);
	if (mdi->reverb) return (mdi);
	_WM_freeMDI(mdi);
	return (NULL);
}

/*
 Convert WildMIDI's MDI events into a type 0 MIDI file.
 
 returns
 0 = successful
 -1 = failed
 
 **out points to place to store stuff
 *outsize points to where to store byte counts
 
 NOTE: This will only write out events that we do support.
 
 *** CAUTION ***
 This will output type 0 midi file reguardless of the original file type. Type 2 midi files will have each original track play on the same track one after the other in the type 0 file.
 */
int
_WM_Event2Midi(struct _mdi *mdi, uint8_t **out, uint32_t *outsize) {
    uint32_t i = 0;
    uint32_t out_ofs = 0;
    uint8_t running_event = 0;
    uint32_t divisions = 0;
    uint32_t tempo = 500000;
	float samples_per_delta_f = 0.0;
	float pulses_per_second_f = 0.0;
    float bpm_f = 0.0;
    uint32_t value = 0;
    float value_f = 0.0;
    
    if (!mdi->event_count) {
        _WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CONVERT, "(No events to convert)", 0);
        return -1;
    }
    
    (*out) = malloc (sizeof(uint8_t) * (mdi->event_count * 12));
    
    // Midi Header
    (*out)[0] = 'M';
    (*out)[1] = 'T';
    (*out)[2] = 'h';
    (*out)[3] = 'd';
    (*out)[4] = 0x00;
    (*out)[5] = 0x00;
    (*out)[6] = 0x00;
    (*out)[7] = 0x06;
    // Type 0
    (*out)[8] = 0x00;
    (*out)[9] = 0x00;
    (*out)[10] = 0x00;
    (*out)[11] = 0x01;
    // Division stored in 12-13
    // Track Header
    (*out)[14] = 'M';
    (*out)[15] = 'T';
    (*out)[16] = 'r';
    (*out)[17] = 'k';
    // Track size stored in 18-21 *** see below
    out_ofs = 22;
    
    for (i = 0; i < mdi->event_count; i++) {
        
        // TODO Is there a better way?
        if (mdi->events[i].do_event == NULL) {
        } else if (mdi->events[i].do_event == _WM_do_midi_divisions) {
            // DEBUG
            // fprintf(stderr,"Division: %u\r\n",mdi->events[i].event_data.data);
            divisions = mdi->events[i].event_data.data;
            (*out)[12] = (divisions >> 8) & 0xff;
            (*out)[13] = divisions & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_note_off) {
            // DEBUG
            // fprintf(stderr,"Note Off: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0x80 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0x80 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 8) & 0xff;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_note_on) {
            // DEBUG
            // fprintf(stderr,"Note On: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0x90 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0x90 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 8) & 0xff;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_aftertouch) {
            // DEBUG
            // fprintf(stderr,"Aftertouch: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xa0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xa0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 8) & 0xff;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_bank_select) {
            // DEBUG
            // fprintf(stderr,"Control Bank Select: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 0;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_data_entry_course) {
            // DEBUG
            // fprintf(stderr,"Control Data Entry Course: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 6;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_volume) {
            // DEBUG
            // fprintf(stderr,"Control Channel Volume: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 7;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_balance) {
            // DEBUG
            // fprintf(stderr,"Control Channel Balance: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 8;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_pan) {
            // DEBUG
            // fprintf(stderr,"Control Channel Pan: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 10;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_expression) {
            // DEBUG
            // fprintf(stderr,"Control Channel Expression: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 11;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_data_entry_fine) {
            // DEBUG
            // fprintf(stderr,"Control Data Entry Fine: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 38;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_hold) {
            // DEBUG
            // fprintf(stderr,"Control Channel Hold: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 64;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_data_increment) {
            // DEBUG
            // fprintf(stderr,"Control Data Increment: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 96;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_data_decrement) {
            // DEBUG
            //fprintf(stderr,"Control Data Decrement: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 97;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_non_registered_param_fine) {
            // DEBUG
            // fprintf(stderr,"Control Non Registered Param: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 98;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0x7f;
        } else if (mdi->events[i].do_event == _WM_do_control_non_registered_param_course) {
            // DEBUG
            // fprintf(stderr,"Control Non Registered Param: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 99;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 7) & 0x7f;
        } else if (mdi->events[i].do_event == _WM_do_control_registered_param_fine) {
            // DEBUG
            // fprintf(stderr,"Control Registered Param Fine: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 100;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0x7f;
        } else if (mdi->events[i].do_event == _WM_do_control_registered_param_course) {
            // DEBUG
            // fprintf(stderr,"Control Registered Param Course: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 101;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 7) & 0x7f;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_sound_off) {
            // DEBUG
            // fprintf(stderr,"Control Channel Sound Off: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 120;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_controllers_off) {
            // DEBUG
            // fprintf(stderr,"Control Channel Controllers Off: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 121;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_control_channel_notes_off) {
            // DEBUG
            // fprintf(stderr,"Control Channel Notes Off: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xb0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xb0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = 123;
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_patch) {
            // DEBUG
            // fprintf(stderr,"Patch: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xc0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xc0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_channel_pressure) {
            // DEBUG
            // fprintf(stderr,"Channel Pressure: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xd0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xd0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0xff;
        } else if (mdi->events[i].do_event == _WM_do_pitch) {
            // DEBUG
            // fprintf(stderr,"Pitch: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            if (running_event != (0xe0 | mdi->events[i].event_data.channel)) {
                (*out)[out_ofs++] = 0xe0 | mdi->events[i].event_data.channel;
                running_event = (*out)[out_ofs - 1];
            }
            (*out)[out_ofs++] = mdi->events[i].event_data.data & 0x7f;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data >> 7) & 0x7f;
        } else if (mdi->events[i].do_event == _WM_do_sysex_roland_drum_track) {
            // DEBUG
            // fprintf(stderr,"Sysex Roland Drum Track: %u %.4x\r\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
            int8_t foo[] = {0xf0, 0x09, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x15, 0x00, 0xf7};
            uint8_t foo_ch = mdi->events[i].event_data.channel;
            if (foo_ch == 9) {
                foo_ch = 0;
            } else if (foo_ch < 9) {
                foo_ch++;
            }
            foo[7] = 0x10 | foo_ch;
            foo[9] = mdi->events[i].event_data.data;
            memcpy(&((*out)[out_ofs]),foo,11);
            out_ofs += 11;
            running_event = 0;
        }  else if (mdi->events[i].do_event == _WM_do_sysex_gm_reset) {
            // DEBUG
            // fprintf(stderr,"Sysex GM Reset\r\n");
            int8_t foo[] = {0xf0, 0x05, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
            memcpy(&((*out)[out_ofs]),foo,7);
            out_ofs += 7;
            running_event = 0;
        } else if (mdi->events[i].do_event == _WM_do_sysex_roland_reset) {
            // DEBUG
            // fprintf(stderr,"Sysex Roland Reset\r\n");
            int8_t foo[] = {0xf0, 0x0a, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
            memcpy(&((*out)[out_ofs]),foo,12);
            out_ofs += 12;
            running_event = 0;
        }  else if (mdi->events[i].do_event == _WM_do_sysex_yamaha_reset) {
            // DEBUG
            // fprintf(stderr,"Sysex Yamaha Reset\r\n");
            int8_t foo[] = {0xf0, 0x08, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};
            memcpy(&((*out)[out_ofs]),foo,10);
            out_ofs += 10;
            running_event = 0;
        } else if (mdi->events[i].do_event == _WM_do_meta_endoftrack) {
            // DEBUG
            // fprintf(stderr,"End Of Track\r\n");
        } else if (mdi->events[i].do_event == _WM_do_meta_tempo) {
            // DEBUG
            // fprintf(stderr,"Tempo: %u\r\n",mdi->events[i].event_data.data);
            tempo = mdi->events[i].event_data.data & 0xffffff;
            
            if ((_WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
                bpm_f = (float) (60000000 / tempo) + 0.5f;
            } else {
                bpm_f = (float) (60000000 / tempo);
            }
            /* Slow but needed for accuracy */
            pulses_per_second_f = ((float)divisions * bpm_f) / 60.0;
            samples_per_delta_f = (float)_WM_SampleRate / pulses_per_second_f;
            //DEBUG
            //fprintf(stderr,"\rDEBUG: div %i, tempo %i, bpm %f, pps %f, spd %f\r\n", divisions, tempo, bpm_f, pulses_per_second_f, samples_per_delta_f);
            
            (*out)[out_ofs++] = 0xff;
            (*out)[out_ofs++] = 0x51;
            (*out)[out_ofs++] = 0x03;
            (*out)[out_ofs++] = (tempo & 0xff0000) >> 16;
            (*out)[out_ofs++] = (tempo & 0xff00) >> 8;
            (*out)[out_ofs++] = (tempo & 0xff);
        } else if (mdi->events[i].do_event == _WM_do_meta_timesignature) {
            // DEBUG
            // fprintf(stderr,"Time Signature: %x\r\n",mdi->events[i].event_data.data);
            tempo = mdi->events[i].event_data.data & 0xffffffff;
            (*out)[out_ofs++] = 0xff;
            (*out)[out_ofs++] = 0x58;
            (*out)[out_ofs++] = 0x04;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data & 0xff000000) >> 24;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data & 0xff0000) >> 16;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data & 0xff00) >> 8;
            (*out)[out_ofs++] = (mdi->events[i].event_data.data & 0xff);
        } else {
            // DEBUG
            fprintf(stderr,"Unknown Event %.2x %.4x\n",mdi->events[i].event_data.channel, mdi->events[i].event_data.data);
        }
        
        value_f = (float)mdi->events[i].samples_to_next / samples_per_delta_f;
        value = (uint32_t)(value_f + 0.5);
        
        //DEBUG
        //fprintf(stderr,"\rDEBUG: STN %i, SPD %f, Delta %i\r\n", mdi->events[i].samples_to_next, samples_per_delta_f, value);
        
        if (value > 0x0fffffff)
            (*out)[out_ofs++] = (((value >> 28) &0x7f) | 0x80);
        if (value > 0x1fffff)
            (*out)[out_ofs++] = (((value >> 21) &0x7f) | 0x80);
        if (value > 0x3fff)
            (*out)[out_ofs++] = (((value >> 14) & 0x7f) | 0x80);
        if (value > 0x7f)
            (*out)[out_ofs++] = (((value >> 7) & 0x7f) | 0x80);
        (*out)[out_ofs++] = (value & 0x7f);
    }
    // Write end of track marker
    (*out)[out_ofs++] = 0xff;
    (*out)[out_ofs++] = 0x2f;
    (*out)[out_ofs++] = 0x00;
    
    // Write track length
    (*out)[18] = ((out_ofs - 22)  >> 24) & 0xff;
    (*out)[19] = ((out_ofs - 22) >> 16) & 0xff;
    (*out)[20] = ((out_ofs - 22) >> 8) & 0xff;
    (*out)[21] = (out_ofs - 22) & 0xff;
    
    (*out) = realloc((*out), out_ofs);
    (*outsize) = out_ofs;
    
    return 0;
}
