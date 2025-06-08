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
#include "wm_error.h"
#include "internal_midi.h"
#include "lock.h"
#include "patches.h"
#include "sample.h"

struct _patch *_WM_patch[128];
int _WM_patch_lock = 0;

static struct _patch *
_find_matched_patch(uint16_t patchid) {
    struct _patch *ret = NULL;
    struct _patch *search_patch;

    for (search_patch = _WM_patch[patchid & 0x007F];
            search_patch && !ret; search_patch = search_patch->next) {
        if (search_patch->patchid == patchid) {
            ret = search_patch;
        }
    }
    return ret;
}

static struct _patch *
_find_nearest_patch(uint16_t patchid) {
    struct _patch *ret = NULL;
    const uint16_t patchid_low = patchid & 0x7F;
    const uint16_t range_down = patchid_low;
    const uint16_t range_up = 0x7F - patchid_low;
    const uint16_t step_max = range_down < range_up ? range_up : range_down;
    uint16_t step;

    for (step = 0; (!ret) && (step <= step_max); ++step) {
        if (patchid_low - step >= 0) {
            ret = _find_matched_patch(patchid - step);
        }
        if ((patchid_low + step <= 0x7F) && !ret) {
            ret = _find_matched_patch(patchid + step);
        }
    }
    return ret;
}

struct _patch *
_WM_get_patch_data(struct _mdi *mdi, uint16_t patchid) {
    struct _patch *search_patch;

    _WM_Lock(&_WM_patch_lock);
    search_patch = _find_nearest_patch(patchid);
    _WM_Unlock(&_WM_patch_lock);
    return (search_patch);
}

void _WM_load_patch(struct _mdi *mdi, uint16_t patchid) {
    uint32_t i;
    struct _patch *tmp_patch = NULL;
    struct _patch **new_patches;

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
    new_patches = (struct _patch **) realloc(mdi->patches, (sizeof(struct _patch*) * mdi->patch_count));
    if (!new_patches) {
        free(mdi->patches);
        _WM_GLOBAL_ERROR(WM_ERR_MEM, "Unable to reallocate memory.", 0);
        _WM_Unlock(&_WM_patch_lock);
        return;
    }
    mdi->patches = new_patches;
    mdi->patches[mdi->patch_count - 1] = tmp_patch;
    tmp_patch->inuse_count++;
    _WM_Unlock(&_WM_patch_lock);
}
