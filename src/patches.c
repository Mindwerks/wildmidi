/*
 patches.c
 
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
#include <stdlib.h>

#include "internal_midi.h"
#include "lock.h"
#include "patches.h"
#include "sample.h"

int patch_lock = 0;

struct _patch *
get_patch_data(struct _mdi *mdi, uint16_t patchid) {
	struct _patch *search_patch;
    
	WM_Lock(&patch_lock);
    
	search_patch = patch[patchid & 0x007F];
    
	if (search_patch == NULL) {
		WM_Unlock(&patch_lock);
		return (NULL);
	}
    
	while (search_patch) {
		if (search_patch->patchid == patchid) {
			WM_Unlock(&patch_lock);
			return (search_patch);
		}
		search_patch = search_patch->next;
	}
	if ((patchid >> 8) != 0) {
		WM_Unlock(&patch_lock);
		return (get_patch_data(mdi, patchid & 0x00FF));
	}
	WM_Unlock(&patch_lock);
	return (NULL);
}

void load_patch(struct _mdi *mdi, uint16_t patchid) {
	uint32_t i;
	struct _patch *tmp_patch = NULL;
    
	for (i = 0; i < mdi->patch_count; i++) {
		if (mdi->patches[i]->patchid == patchid) {
			return;
		}
	}
    
	tmp_patch = get_patch_data(mdi, patchid);
	if (tmp_patch == NULL) {
		return;
	}
    
	WM_Lock(&patch_lock);
	if (!tmp_patch->loaded) {
		if (load_sample(tmp_patch) == -1) {
			WM_Unlock(&patch_lock);
			return;
		}
	}
    
	if (tmp_patch->first_sample == NULL) {
		WM_Unlock(&patch_lock);
		return;
	}
    
	mdi->patch_count++;
	mdi->patches = realloc(mdi->patches,
                           (sizeof(struct _patch*) * mdi->patch_count));
	mdi->patches[mdi->patch_count - 1] = tmp_patch;
	tmp_patch->inuse_count++;
	WM_Unlock(&patch_lock);
}
