/*
 * patches.c -- Midi Wavetable Processing library
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "lock.h"
#include "patches.h"
#include "sample.h"

struct _patch *_WM_patch[128];
int _WM_patch_lock = 0;

struct _patch *
_WM_get_patch_data(struct _mdi *mdi, uint16_t patchid) {
    struct _patch *search_patch;

    _WM_Lock(&_WM_patch_lock);

    search_patch = _WM_patch[patchid & 0x007F];

    if (search_patch == NULL) {
        _WM_Unlock(&_WM_patch_lock);
        return (NULL);
    }

    while (search_patch) {
        if (search_patch->patchid == patchid) {
            _WM_Unlock(&_WM_patch_lock);
            return (search_patch);
        }
        search_patch = search_patch->next;
    }
    if ((patchid >> 8) != 0) {
        _WM_Unlock(&_WM_patch_lock);
        return (_WM_get_patch_data(mdi, patchid & 0x00FF));
    }
    _WM_Unlock(&_WM_patch_lock);
    return (NULL);
}

void _WM_load_patch(struct _mdi *mdi, uint16_t patchid) {
    uint32_t i;
    struct _patch *tmp_patch = NULL;

    for (i = 0; i < mdi->patch_count; i++) {
        if (mdi->patches[i]->patchid == patchid) {
            return;
        }
    }

    tmp_patch = _WM_get_patch_data(mdi, patchid);
    if (tmp_patch == NULL) {
        return;
    }

    _WM_Lock(&_WM_patch_lock);
    if (!tmp_patch->loaded) {
        if (_WM_load_sample(tmp_patch) == -1) {
            _WM_Unlock(&_WM_patch_lock);
            return;
        }
    }

    if (tmp_patch->first_sample == NULL) {
        _WM_Unlock(&_WM_patch_lock);
        return;
    }

    mdi->patch_count++;
    mdi->patches = realloc(mdi->patches,
                           (sizeof(struct _patch*) * mdi->patch_count));
    mdi->patches[mdi->patch_count - 1] = tmp_patch;
    tmp_patch->inuse_count++;
    _WM_Unlock(&_WM_patch_lock);
}
