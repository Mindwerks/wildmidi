/*
    wildmidi_lib.c - Midi Wavetable Processing library
    Copyright (C) 2001-2008 Chris Ison

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the players
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

    Email: cisos@bigpond.net.au
    	 wildcode@users.sourceforge.net
 
    $Id: wildmidi_lib.c,v 1.76 2008/06/05 07:01:59 wildcode Exp $
*/

/*
 * =========================
 * External Functions
 * =========================
 */

const char * 
WildMidi_GetString (unsigned short int info) {
	switch (info) {
		case WM_GS_VERSION:
			return WM_Version;
	}
	return NULL;
}

int 
WildMidi_Init (const char * config_file, unsigned short int rate, unsigned short int options) {
	if (WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_ALR_INIT, NULL, 0);
		return -1;
	}

	if (config_file == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL config file pointer)", 0);
		return -1;
	}
	WM_InitPatches();
	if (WM_LoadConfig(config_file) == -1) {
		return -1;
	}

	if (options & 0xFFD8) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)", 0);
		WM_FreePatches();
		return -1;
	}
	WM_MixerOptions = options;

	if ((rate < 11000) || (rate > 65000)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(rate out of bounds, range is 11000 - 65000)", 0);
		WM_FreePatches();
		return -1;
	}
	WM_SampleRate = rate;
	WM_Initialized = 1;
	patch_lock = 0;
	
	init_gauss();
	return 0;
}

int
WildMidi_MasterVolume (unsigned char master_volume) {
	struct _mdi *mdi = NULL;
	struct _hndl * tmp_handle = first_handle;
	int i = 0;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (master_volume > 127) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(master volume out of range, range is 0-127)", 0);
		return -1;
	}
	
	WM_MasterVolume = lin_volume[master_volume];

	if (tmp_handle != NULL) {
		while(tmp_handle != NULL) {
			mdi = (struct _mdi *)tmp_handle->handle;
			for (i = 0; i < 16; i++) {
				do_pan_adjust(mdi, i);
			}
			tmp_handle = tmp_handle->next;
		}
	}
	
	return 0;
}

int
WildMidi_Close (midi * handle) {
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _hndl * tmp_handle;
	struct _sample *tmp_sample;
	unsigned int i;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	if (first_handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(no midi's open)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if (first_handle->handle == handle) {
		tmp_handle = first_handle->next;
		free (first_handle);
		first_handle = tmp_handle;
		if (first_handle != NULL)
			first_handle->prev = NULL;
	} else {
		tmp_handle = first_handle;
		while (tmp_handle->handle != handle) {
			tmp_handle = tmp_handle->next;
			if (tmp_handle == NULL) {
				WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(handle does not exist)", 0);
				return -1;
			}
		}
		tmp_handle->prev->next = tmp_handle->next;
		if (tmp_handle->next != NULL) {
			tmp_handle->next->prev = tmp_handle->prev;
		}
		free (tmp_handle);
	}
	
	if (mdi->patch_count != 0) {
		WM_Lock(&patch_lock);
		for (i = 0; i < mdi->patch_count; i++) {
			mdi->patches[i]->inuse_count--;
			if (mdi->patches[i]->inuse_count == 0) {
				//free samples here
				if (mdi->patches[i]->first_sample != NULL) {
					while (mdi->patches[i]->first_sample != NULL) {
						tmp_sample = mdi->patches[i]->first_sample->next;
						if (mdi->patches[i]->first_sample->data)
							free(mdi->patches[i]->first_sample->data);
						free(mdi->patches[i]->first_sample);
						mdi->patches[i]->first_sample = tmp_sample;
					}
					mdi->patches[i]->loaded = 0;
				}
			}
		}
		WM_Unlock(&patch_lock);
		free (mdi->patches);
	}
	if (mdi->events != NULL) {
		free (mdi->events);
	}
	if (mdi->tmp_info != NULL) {
		free (mdi->tmp_info);
	}
	free_reverb(mdi->reverb);
	free (mdi);
	// no need to unlock cause the struct containing the lock no-longer exists;
	return 0;
}

midi * 
WildMidi_Open (const char *midifile) {
	unsigned char *mididata = NULL;
	unsigned long int midisize = 0;
	
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midifile == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL filename)", 0);
		return NULL;
	}

	if ((mididata = WM_BufferFile(midifile, &midisize)) == NULL) {
		return NULL;
	}
	
	return (void *)WM_ParseNewMidi(mididata,midisize);
}

midi *
WildMidi_OpenBuffer (unsigned char *midibuffer, unsigned long int size) {
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midibuffer == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL midi data buffer)", 0);
		return NULL;
	}

	return (void *)WM_ParseNewMidi(midibuffer,size);
}

int
WildMidi_LoadSamples( midi * handle) {
	return 0;
}

int
WildMidi_FastSeek ( midi * handle, unsigned long int *sample_pos) {
	return 0;
}

int
WildMidi_SampledSeek ( midi * handle, unsigned long int *sample_pos) {
	return 0;
}

int
WildMidi_GetOutput (midi * handle, char * buffer, unsigned long int size) {
	struct _mdi *mdi = (struct _mdi *)handle;

	if (__builtin_expect((!WM_Initialized),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (__builtin_expect((handle == NULL),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	if (__builtin_expect((buffer == NULL),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL buffer pointer)", 0);
		return -1;
	}

	if (__builtin_expect((size == 0),0)) {
		return 0;
	}

	if (__builtin_expect((size % 4),0)) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(size not a multiple of 4)", 0);
		return -1;
	}
	if (mdi->info.mixer_options & WM_MO_ENHANCED_RESAMPLING) {
		return WildMidi_GetOutput_Gauss (handle, buffer,size); 
	} else {
		return WildMidi_GetOutput_Linear (handle, buffer, size); 
	}
}

int 
WildMidi_SetOption (midi * handle, unsigned short int options, unsigned short int setting) {
	struct _mdi *mdi = (struct _mdi *)handle;
	struct _note **note_data = mdi->note;
	int i;
	
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return -1;
	}
	WM_Lock(&mdi->lock);
	if ((!(options & 0x0007)) || (options & 0xFFF8)){
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}
	if (setting & 0xFFF8) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid setting)", 0);
		WM_Unlock(&mdi->lock);
		return -1;
	}

	mdi->info.mixer_options = ((mdi->info.mixer_options & (0x00FF ^ options)) | (options & setting));

	if (options & WM_MO_LOG_VOLUME) {
    	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
    		mdi->amp = (281 * ((mdi->lin_max_vol << 10) / mdi->log_max_vol)) >> 10;
    	} else {
	    	mdi->amp = 281;
    	}
   	    mdi->amp = (mdi->amp * (((lin_volume[127] * lin_volume[127]) << 10) / mdi->lin_max_vol)) >> 10;

		for (i = 0; i < 16; i++) {
			do_pan_adjust(mdi, i);
		}
		if (note_data != mdi->last_note) {
			do {
				(*note_data)->vol_lvl = get_volume(mdi, ((*note_data)->noteid >> 8), *note_data);
				if ((*note_data)->next)
					(*note_data)->next->vol_lvl = get_volume(mdi, ((*note_data)->noteid >> 8), (*note_data)->next);
				note_data++;	
			} while (note_data != mdi->last_note);
		}
	} else if (options & WM_MO_REVERB) {
		reset_reverb(mdi->reverb);
	}

	WM_Unlock(&mdi->lock);
	return 0;
}

struct _WM_Info * 
WildMidi_GetInfo (midi * handle) {
	struct _mdi *mdi = (struct _mdi *)handle;
	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (handle == NULL) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)", 0);
		return NULL;
	}
	WM_Lock(&mdi->lock);
	if (mdi->tmp_info == NULL) {
		mdi->tmp_info = malloc(sizeof(struct _WM_Info));
		if (mdi->tmp_info == NULL) {
			WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set info", 0);
			WM_Unlock(&mdi->lock);
			return NULL;
		}
	}
	mdi->tmp_info->current_sample = mdi->info.current_sample;
	mdi->tmp_info->approx_total_samples = mdi->info.approx_total_samples;
	mdi->tmp_info->mixer_options = mdi->info.mixer_options;
	WM_Unlock(&mdi->lock);
	return mdi->tmp_info;
}

#include "midi.h"

static int WM_Initialized = 0;

struct _hndl {
	struct _mdi * mdi;
	struct _hndl * next
};

struct _hndl * first_handle = NULL;

#ifndef __SRC_0_3_0
char * global_config = NULL;
unsigned short int global_option = 0;
#endif

unsigned short int rate = 0;

int 
#ifndef __SRC_0_3_0
WildMidi_Init (const char * config_file, unsigned short int rate, unsigned short int options) {
#else
WildMidi_Init (unsigned short int rate) {
#endif	
	if (WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_ALR_INIT, NULL, 0);
		return -1;
	}
	
	first_handle = NULL;
	
	WM_Initialized = 1;
	return 0;
}

int
WildMidi_Shutdown ( void ) {
	struct _hndl * tmp_hdle;

	if (!WM_Initialized) {
		WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	
	if (first_handle != NULL) {
		while (first_handle != NULL) {
			tmp_hdle = first_handle->next;
			free (first_handle);
			first_handle = tmp_hdle;			
		}
	}

#ifndef __SRC_0_3_0
	free(global_config);
#endif

	WM_Initialized = 0;
	return 0;
}
