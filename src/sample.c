/*
 sample.c
 
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

#include <stdio.h>
#include <stdint.h>

#include "lock.h"
#include "common.h"
#include "patches.h"
#include "gus_pat.h"
#include "sample.h"


struct _sample *get_sample_data(struct _patch *sample_patch, uint32_t freq) {
	struct _sample *last_sample = NULL;
	struct _sample *return_sample = NULL;
    
	WM_Lock(&patch_lock);
	if (sample_patch == NULL) {
		WM_Unlock(&patch_lock);
		return (NULL);
	}
	if (sample_patch->first_sample == NULL) {
		WM_Unlock(&patch_lock);
		return (NULL);
	}
	if (freq == 0) {
		WM_Unlock(&patch_lock);
		return (sample_patch->first_sample);
	}
    
	return_sample = sample_patch->first_sample;
	last_sample = sample_patch->first_sample;
	while (last_sample) {
		if (freq > last_sample->freq_low) {
			if (freq < last_sample->freq_high) {
				WM_Unlock(&patch_lock);
				return (last_sample);
			} else {
				return_sample = last_sample;
			}
		}
		last_sample = last_sample->next;
	}
	WM_Unlock(&patch_lock);
	return (return_sample);
}

/* sample loading */

int
load_sample(struct _patch *sample_patch) {
	struct _sample *guspat = NULL;
	struct _sample *tmp_sample = NULL;
	uint32_t i = 0;
    
	/* we only want to try loading the guspat once. */
	sample_patch->loaded = 1;
    
	if ((guspat = load_gus_pat(sample_patch->filename, fix_release)) == NULL) {
		return (-1);
	}
    
	if (auto_amp) {
		int16_t tmp_max = 0;
		int16_t tmp_min = 0;
		int16_t samp_max = 0;
		int16_t samp_min = 0;
		tmp_sample = guspat;
		do {
			samp_max = 0;
			samp_min = 0;
			for (i = 0; i < (tmp_sample->data_length >> 10); i++) {
				if (tmp_sample->data[i] > samp_max)
					samp_max = tmp_sample->data[i];
				if (tmp_sample->data[i] < samp_min)
					samp_min = tmp_sample->data[i];
                
			}
			if (samp_max > tmp_max)
				tmp_max = samp_max;
			if (samp_min < tmp_min)
				tmp_min = samp_min;
			tmp_sample = tmp_sample->next;
		} while (tmp_sample);
		if (auto_amp_with_amp) {
			if (tmp_max >= -tmp_min) {
				sample_patch->amp = (sample_patch->amp
                                     * ((32767 << 10) / tmp_max)) >> 10;
			} else {
				sample_patch->amp = (sample_patch->amp
                                     * ((32768 << 10) / -tmp_min)) >> 10;
			}
		} else {
			if (tmp_max >= -tmp_min) {
				sample_patch->amp = (32767 << 10) / tmp_max;
			} else {
				sample_patch->amp = (32768 << 10) / -tmp_min;
			}
		}
	}
    
	sample_patch->first_sample = guspat;
    
	if (sample_patch->patchid & 0x0080) {
		if (!(sample_patch->keep & SAMPLE_LOOP)) {
			do {
				guspat->modes &= 0xFB;
				guspat = guspat->next;
			} while (guspat);
		}
		guspat = sample_patch->first_sample;
		if (!(sample_patch->keep & SAMPLE_ENVELOPE)) {
			do {
				guspat->modes &= 0xBF;
				guspat = guspat->next;
			} while (guspat);
		}
		guspat = sample_patch->first_sample;
	}
    
	if (sample_patch->patchid == 47) {
		do {
			if (!(guspat->modes & SAMPLE_LOOP)) {
				for (i = 3; i < 6; i++) {
					guspat->env_target[i] = guspat->env_target[2];
					guspat->env_rate[i] = guspat->env_rate[2];
				}
			}
			guspat = guspat->next;
		} while (guspat);
		guspat = sample_patch->first_sample;
	}
    
	do {
		if ((sample_patch->remove & SAMPLE_SUSTAIN)
            && (guspat->modes & SAMPLE_SUSTAIN)) {
			guspat->modes ^= SAMPLE_SUSTAIN;
		}
		if ((sample_patch->remove & SAMPLE_CLAMPED)
            && (guspat->modes & SAMPLE_CLAMPED)) {
			guspat->modes ^= SAMPLE_CLAMPED;
		}
		if (sample_patch->keep & SAMPLE_ENVELOPE) {
			guspat->modes |= SAMPLE_ENVELOPE;
		}
        
		for (i = 0; i < 6; i++) {
			if (guspat->modes & SAMPLE_ENVELOPE) {
				if (sample_patch->env[i].set & 0x02) {
					guspat->env_target[i] = 16448
                    * (int32_t) (255.0
                                 * sample_patch->env[i].level);
				}
                
				if (sample_patch->env[i].set & 0x01) {
					guspat->env_rate[i] = (int32_t) (4194303.0
                                                     / ((float) WM_SampleRate
                                                        * (sample_patch->env[i].time / 1000.0)));
				}
			} else {
				guspat->env_target[i] = 4194303;
				guspat->env_rate[i] = (int32_t) (4194303.0
                                                 / ((float) WM_SampleRate * env_time_table[63]));
			}
		}
        
		guspat = guspat->next;
	} while (guspat);
	return (0);
}

