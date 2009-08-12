/*
    patches.c - manage midi sound samples
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

    $Id: patches.c,v 1.3 2008/05/23 13:22:31 wildcode Exp $
*/

#include "patches.h"
#include "wildmidi_cfg.h"

inline struct _patch_data *
WM_InitPatches(const char *config_file) {
	struct _patch_data *patch_data = NULL;
	struct _patchcfg *wildmidi_cfg = NULL;
	struct _patchcfg *wildmidi_ptr = NULL;
	
	if ((wildmidi_cfg = WM_LoadConfig(config_file)) == NULL) {
		return NULL;
	}
	
	wildmidi_ptr
	
	return patch_data;
}

#if 0

void
WM_InitPatches ( void ) {
	int i;
	for (i = 0; i < 128; i++) {
		patch[i] = NULL;
	}	
}

void
WM_FreePatches ( void ) {
	int i;
	struct _patch * tmp_patch;
	struct _sample * tmp_sample;

	WM_Lock(&patch_lock);
	for (i = 0; i < 128; i++) {
		if (patch[i] != NULL) {
			while (patch[i] != NULL) {
				if (patch[i]->filename != NULL) {
					if (patch[i]->first_sample != NULL) {
						while (patch[i]->first_sample != NULL) {
							tmp_sample = patch[i]->first_sample->next;
							if (patch[i]->first_sample->data != NULL)
								free (patch[i]->first_sample->data);
							free (patch[i]->first_sample);
							patch[i]->first_sample = tmp_sample;
						}
					}
					free (patch[i]->filename);
				}
				tmp_patch = patch[i]->next;
				free(patch[i]);
				patch[i] = tmp_patch;
			}
		}
	}
	WM_Unlock(&patch_lock);
}

struct _patch *
get_patch_data(struct _mdi *mdi, unsigned short patchid) {
	struct _patch *search_patch;
	
	WM_Lock(&patch_lock);
		
	search_patch = patch[patchid & 0x007F];

	if (search_patch == NULL) {
		WM_Unlock(&patch_lock);
		return NULL;
	}

	while(search_patch != NULL) {
		if (search_patch->patchid == patchid) {
			WM_Unlock(&patch_lock);
			return search_patch;
		}
		search_patch = search_patch->next;
	}
	if ((patchid >> 8) != 0) {
		WM_Unlock(&patch_lock);
		return (get_patch_data(mdi, patchid & 0x00FF));
	}
	WM_Unlock(&patch_lock);
	return NULL;
}

void
load_patch (struct _mdi *mdi, unsigned short patchid) {
	unsigned int i;
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
	mdi->patches = realloc(mdi->patches, (sizeof(struct _patch) * mdi->patch_count));
	mdi->patches[mdi->patch_count -1] = tmp_patch;
	tmp_patch->inuse_count++;
	WM_Unlock(&patch_lock);
	return;
}

