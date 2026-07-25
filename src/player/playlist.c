/*
 * playlist.c -- WildMidi player playlist handling
 *
 * Builds the list of files to play from the command line, expanding any
 * directory into the midi files found under it, and optionally shuffles it.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

/* Directory scanning needs a platform backend.  Where we have none, a
 * directory argument is simply passed through as a file name and the
 * library reports that it cannot be opened. */
#if defined(_WIN32)
#define PLAYLIST_HAVE_DIRSCAN 1
#include <windows.h>

#elif defined(WILDMIDI_AMIGA)
#define PLAYLIST_HAVE_DIRSCAN 1
#include <proto/exec.h>
#include <proto/dos.h>
#ifdef __amigaos4__
#include <dos/obsolete.h>
#endif

#elif defined(__OS2__) || defined(__EMX__)
#define PLAYLIST_HAVE_DIRSCAN 1
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#elif defined(__DJGPP__) || defined(_3DS) || defined(GEKKO) || \
      defined(__vita__) || defined(__SWITCH__) || defined(__riscos__) || \
      defined(__unix) || defined(__unix__) || defined(__APPLE__)
#define PLAYLIST_HAVE_DIRSCAN 1
#define PLAYLIST_USE_DIRENT 1
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#include "playlist.h"
#include "filenames.h"

#ifdef PLAYLIST_HAVE_DIRSCAN
/* Extensions we queue when walking a directory.  Files named explicitly on
 * the command line bypass this: the library sniffs content, not names, so a
 * deliberately named file still plays.  */
static const char *const midi_extensions[] = {
    "mid", "midi", "rmi", "kar", "mus", "xmi", "hmp", "hmi", NULL
};

static int has_midi_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    int i;

    if (ext == NULL) return 0;
    ext++;

    for (i = 0; midi_extensions[i] != NULL; i++) {
        const char *a = ext;
        const char *b = midi_extensions[i];
        while (*a && *b) {
            int ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
            if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
            if (ca != cb) break;
            a++; b++;
        }
        if (!*a && !*b) return 1;
    }
    return 0;
}
#endif /* PLAYLIST_HAVE_DIRSCAN */

/* Appends path verbatim, growing the array as needed. */
static int playlist_append(playlist *pl, const char *path) {
    char *copy;

    if (pl->count == pl->alloc) {
        /* Cap growth so neither the doubling nor the byte count can wrap. */
        size_t limit = (size_t)-1 / sizeof(char *);
        unsigned int maxalloc;
        unsigned int newalloc;
        char **grown;

        if (limit > (size_t)(UINT_MAX / 2)) limit = (size_t)(UINT_MAX / 2);
        maxalloc = (unsigned int) limit;
        if (pl->alloc >= maxalloc) {
            fprintf(stderr, "ERROR: playlist is too large\r\n");
            return (-1);
        }
        newalloc = pl->alloc ? pl->alloc * 2 : 64;
        if (newalloc > maxalloc) newalloc = maxalloc;

        grown = (char **) realloc(pl->files, (size_t) newalloc * sizeof(char *));
        if (grown == NULL) {
            fprintf(stderr, "ERROR: out of memory building playlist\r\n");
            return (-1);
        }
        pl->files = grown;
        pl->alloc = newalloc;
    }

    copy = (char *) malloc(strlen(path) + 1);
    if (copy == NULL) {
        fprintf(stderr, "ERROR: out of memory building playlist\r\n");
        return (-1);
    }
    strcpy(copy, path);
    pl->files[pl->count++] = copy;
    return (0);
}

#ifdef PLAYLIST_HAVE_DIRSCAN
/* Joins dir and name into a freshly allocated path. */
static char *join_path(const char *dir, const char *name) {
    size_t dirlen = strlen(dir);
    size_t namelen = strlen(name);
    int need_sep = (dirlen != 0 && !IS_DIR_SEPARATOR(dir[dirlen - 1]));
    char *out = (char *) malloc(dirlen + (need_sep ? 1 : 0) + namelen + 1);

    if (out == NULL) return NULL;
    memcpy(out, dir, dirlen);
    if (need_sep) out[dirlen++] = DIR_SEPARATOR_CHAR;
    memcpy(out + dirlen, name, namelen + 1);
    return out;
}
#endif /* PLAYLIST_HAVE_DIRSCAN */

#if defined(PLAYLIST_HAVE_DIRSCAN) && !defined(WILDMIDI_AMIGA)
/* True for the "." and ".." entries every directory carries.  AmigaDOS
 * does not report them, so its scanner has no need for this. */
static int is_dot_entry(const char *name) {
    return (name[0] == '.' && (name[1] == '\0' ||
           (name[1] == '.' && name[2] == '\0')));
}
#endif

#ifdef PLAYLIST_HAVE_DIRSCAN
/* Recursively adds the playable files under dir.  Returns 0 on success,
 * -1 if dir itself could not be read. */
static int scan_directory(playlist *pl, const char *dir);
#endif

#if defined(PLAYLIST_USE_DIRENT)

static int path_is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* A directory we should descend into.  Symlinks are not followed: a link
 * pointing at one of its own ancestors would otherwise make us recurse
 * until the OS runs out of path, collecting duplicates along the way. */
static int is_walkable_directory(const char *path) {
#ifdef S_ISLNK
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISLNK(st.st_mode)) return 0;
    return S_ISDIR(st.st_mode);
#else
    return path_is_directory(path);
#endif
}

static int scan_directory(playlist *pl, const char *dir) {
    DIR *dh = opendir(dir);
    struct dirent *ent;
    int ret = 0;

    if (dh == NULL) {
        fprintf(stderr, "ERROR: cannot read directory %s\r\n", dir);
        return (-1);
    }

    while ((ent = readdir(dh)) != NULL) {
        char *full;

        if (is_dot_entry(ent->d_name)) continue;

        full = join_path(dir, ent->d_name);
        if (full == NULL) {
            fprintf(stderr, "ERROR: out of memory building playlist\r\n");
            ret = -1;
            break;
        }

        if (is_walkable_directory(full)) {
            /* An unreadable subdirectory is not fatal: keep collecting the
             * rest of the tree.  */
            scan_directory(pl, full);
        } else if (has_midi_extension(ent->d_name)) {
            if (playlist_append(pl, full) < 0) ret = -1;
        }

        free(full);
        if (ret < 0) break;
    }

    closedir(dh);
    return (ret);
}

#elif defined(_WIN32)

static int path_is_directory(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int scan_directory(playlist *pl, const char *dir) {
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char *pattern = join_path(dir, "*");
    int ret = 0;

    if (pattern == NULL) {
        fprintf(stderr, "ERROR: out of memory building playlist\r\n");
        return (-1);
    }

    h = FindFirstFileA(pattern, &fd);
    free(pattern);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "ERROR: cannot read directory %s\r\n", dir);
        return (-1);
    }

    do {
        char *full;

        if (is_dot_entry(fd.cFileName)) continue;

        full = join_path(dir, fd.cFileName);
        if (full == NULL) {
            fprintf(stderr, "ERROR: out of memory building playlist\r\n");
            ret = -1;
            break;
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_directory(pl, full);
        } else if (has_midi_extension(fd.cFileName)) {
            if (playlist_append(pl, full) < 0) ret = -1;
        }

        free(full);
        if (ret < 0) break;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return (ret);
}

#elif defined(__OS2__) || defined(__EMX__)

static int path_is_directory(const char *path) {
    FILESTATUS3 fs;
    if (DosQueryPathInfo((PCSZ)path, FIL_STANDARD, &fs, sizeof(fs)) != NO_ERROR)
        return 0;
    return (fs.attrFile & FILE_DIRECTORY) != 0;
}

static int scan_directory(playlist *pl, const char *dir) {
    FILEFINDBUF3 fb;
    HDIR h = HDIR_CREATE;
    ULONG cnt = 1;
    char *pattern = join_path(dir, "*");
    int ret = 0;

    if (pattern == NULL) {
        fprintf(stderr, "ERROR: out of memory building playlist\r\n");
        return (-1);
    }

    if (DosFindFirst((PCSZ)pattern, &h, FILE_NORMAL | FILE_DIRECTORY,
                     &fb, sizeof(fb), &cnt, FIL_STANDARD) != NO_ERROR) {
        free(pattern);
        fprintf(stderr, "ERROR: cannot read directory %s\r\n", dir);
        return (-1);
    }
    free(pattern);

    do {
        char *full;

        if (is_dot_entry(fb.achName)) continue;

        full = join_path(dir, fb.achName);
        if (full == NULL) {
            fprintf(stderr, "ERROR: out of memory building playlist\r\n");
            ret = -1;
            break;
        }

        if (fb.attrFile & FILE_DIRECTORY) {
            scan_directory(pl, full);
        } else if (has_midi_extension(fb.achName)) {
            if (playlist_append(pl, full) < 0) ret = -1;
        }

        free(full);
        if (ret < 0) break;
        cnt = 1;
    } while (DosFindNext(h, &fb, sizeof(fb), &cnt) == NO_ERROR);

    DosFindClose(h);
    return (ret);
}

#elif defined(WILDMIDI_AMIGA)

static int path_is_directory(const char *path) {
    BPTR lock = Lock((const STRPTR) path, ACCESS_READ);
    int isdir = 0;

    if (lock) {
        struct FileInfoBlock *fib = (struct FileInfoBlock *)
                              AllocDosObject(DOS_FIB, NULL);
        if (fib != NULL) {
            if (Examine(lock, fib))
                isdir = (fib->fib_DirEntryType > 0);
            FreeDosObject(DOS_FIB, fib);
        }
        UnLock(lock);
    }
    return isdir;
}

static int scan_directory(playlist *pl, const char *dir) {
    BPTR lock = Lock((const STRPTR) dir, ACCESS_READ);
    struct FileInfoBlock *fib;
    int ret = 0;

    if (!lock) {
        fprintf(stderr, "ERROR: cannot read directory %s\r\n", dir);
        return (-1);
    }

    fib = (struct FileInfoBlock *) AllocDosObject(DOS_FIB, NULL);
    if (fib == NULL) {
        UnLock(lock);
        fprintf(stderr, "ERROR: out of memory building playlist\r\n");
        return (-1);
    }

    if (!Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        fprintf(stderr, "ERROR: cannot read directory %s\r\n", dir);
        return (-1);
    }

    while (ExNext(lock, fib)) {
        char *full = join_path(dir, (const char *) fib->fib_FileName);

        if (full == NULL) {
            fprintf(stderr, "ERROR: out of memory building playlist\r\n");
            ret = -1;
            break;
        }

        if (fib->fib_DirEntryType > 0) {
            scan_directory(pl, full);
        } else if (has_midi_extension((const char *) fib->fib_FileName)) {
            if (playlist_append(pl, full) < 0) ret = -1;
        }

        free(full);
        if (ret < 0) break;
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return (ret);
}

#endif /* platform directory scanners */

int playlist_add(playlist *pl, const char *path) {
#ifdef PLAYLIST_HAVE_DIRSCAN
    if (path_is_directory(path)) {
        if (scan_directory(pl, path) < 0)
            return PLAYLIST_ADD_ERROR;
        return PLAYLIST_ADD_DIR;
    }
#endif
    if (playlist_append(pl, path) < 0)
        return PLAYLIST_ADD_ERROR;
    return PLAYLIST_ADD_FILE;
}

void playlist_seed_random(void) {
    srand((unsigned int) time(NULL));
}

void playlist_shuffle(playlist *pl) {
    unsigned int i;

    /* Fisher-Yates */
    for (i = pl->count; i > 1; i--) {
        unsigned int j = (unsigned int) (rand() % (int) i);
        char *tmp = pl->files[i - 1];
        pl->files[i - 1] = pl->files[j];
        pl->files[j] = tmp;
    }
}

void playlist_free(playlist *pl) {
    unsigned int i;

    for (i = 0; i < pl->count; i++)
        free(pl->files[i]);
    free(pl->files);
    pl->files = NULL;
    pl->count = 0;
    pl->alloc = 0;
}
