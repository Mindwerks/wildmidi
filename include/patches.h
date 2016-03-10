/*
 * patches.h -- Midi Wavetable Processing library
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

#ifndef __PATCHES_H
#define __PATCHES_H

struct _env {
    float time;
    float level;
    uint8_t set;
};

struct _sample;
struct _mdi;

struct _patch {
    uint16_t patchid;
    uint8_t loaded;
    char *filename;
    int16_t amp;
    uint8_t keep;
    uint8_t remove;
    struct _env env[6];
    uint8_t  note;
    uint32_t inuse_count;
    struct _sample *first_sample;
    struct _patch *next;
};

extern struct _patch *_WM_patch[128];

extern int _WM_patch_lock;

extern struct _patch *_WM_get_patch_data(struct _mdi *mdi, uint16_t patchid);
extern void _WM_load_patch(struct _mdi *mdi, uint16_t patchid);

#endif /* __PATCHES_H */
