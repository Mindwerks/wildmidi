/*
 * playlist.h -- WildMidi player playlist header
 *
 * Copyright (C) WildMidi Developers 2026
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

#ifndef PLAYLIST_H
#define PLAYLIST_H

typedef struct {
    char **files;       /* owned, NULL when empty */
    unsigned int count; /* entries in use */
    unsigned int alloc; /* entries allocated */
} playlist;

/* Adds an entry to the playlist.  If path names a directory it is scanned
 * recursively and every file with a known midi-ish extension is added,
 * otherwise path is added as-is without looking at its name.
 * Returns PLAYLIST_ADD_FILE or PLAYLIST_ADD_DIR on success, so the caller
 * can tell a directory apart from a plain file even when the directory
 * held a single entry, and PLAYLIST_ADD_ERROR on failure (out of memory,
 * or an unreadable directory).  Failing to add one entry leaves earlier
 * entries intact.  */
#define PLAYLIST_ADD_ERROR (-1)
#define PLAYLIST_ADD_FILE  0
#define PLAYLIST_ADD_DIR   1
extern int playlist_add(playlist *pl, const char *path);

/* Randomizes the order of the entries.  Call playlist_seed_random() once
 * before the first shuffle.  */
extern void playlist_shuffle(playlist *pl);
extern void playlist_seed_random(void);

/* Frees every entry and resets the playlist to empty.  */
extern void playlist_free(playlist *pl);

#endif /* PLAYLIST_H */
