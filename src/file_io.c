/*
 * file_io.c -- file handling
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

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef WILDMIDI_AMIGA
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#undef close
#define close _close
#undef open
#define open _open
#undef read
#define read _read
#elif defined(__DJGPP__)
#include <io.h>
#include <dir.h>
#include <unistd.h>
#elif defined(__OS2__) || defined(__EMX__)
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <fcntl.h>
#include <io.h>
#elif defined(WILDMIDI_AMIGA)
#include <proto/exec.h>
#include <proto/dos.h>
#else
#include <pwd.h>
#include <strings.h>
#include <unistd.h>
#endif

#if !defined(O_BINARY)
# if defined(_O_BINARY)
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY  0
# endif
#endif

#include "wm_error.h"
#include "file_io.h"

#ifdef WILDMIDI_AMIGA
static long AMIGA_filesize (const char *path) {
    long size = -1;
    BPTR fh = Open((const STRPTR) path, MODE_OLDFILE);
    if (fh) {
        struct FileInfoBlock *fib = (struct FileInfoBlock*)
                              AllocDosObject(DOS_FIB, NULL);
        if (fib != NULL) {
            if (ExamineFH(fh, fib))
                size = fib->fib_Size;
            FreeDosObject(DOS_FIB, fib);
        }
        Close(fh);
    }
    return size;
}

static long AMIGA_read (BPTR fd, unsigned char *buf, long size) {
    long bytes_read = 0, result;
    while (bytes_read < size) {
        result = Read(fd, buf + bytes_read, size - bytes_read);
        if (result < 0) return result;
        if (result == 0) break;
        bytes_read += result;
    }
    return bytes_read;
}

static BPTR AMIGA_open (const char *path) {
    return Open((const STRPTR) path, MODE_OLDFILE);
}

static void AMIGA_close (BPTR fd) {
    Close(fd);
}
#endif

void *_WM_BufferFile(const char *filename, uint32_t *size) {
    char *buffer_file = NULL;
    uint8_t *data;
#ifdef __DJGPP__
    int buffer_fd;
    struct ffblk f;
#elif defined(_WIN32)
    int buffer_fd;
    HANDLE h;
    WIN32_FIND_DATAA wfd;
#elif defined(__OS2__) || defined(__EMX__)
    int buffer_fd;
    HDIR h = HDIR_CREATE;
    FILEFINDBUF3 fb = {0};
    ULONG cnt = 1;
#elif defined(WILDMIDI_AMIGA)
    BPTR buffer_fd;
    long filsize;
#elif defined(_3DS) || defined(GEKKO)
    int buffer_fd;
    struct stat buffer_stat;
#else /* unix builds */
    int buffer_fd;
    struct stat buffer_stat;

/* for basedir of filename: */
    const char *home = NULL;
    struct passwd *pwd_ent;
    char buffer_dir[1024];

    if (strncmp(filename, "~/", 2) == 0) {
        pwd_ent = getpwuid(getuid());
        if (pwd_ent) {
            home = pwd_ent->pw_dir;
        } else {
            home = getenv("HOME");
        }
        if (home) {
            buffer_file = (char *) malloc(strlen(filename) + strlen(home) + 1);
            if (buffer_file == NULL) {
                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, filename, errno);
                return NULL;
            }
            strcpy(buffer_file, home);
            strcat(buffer_file, filename + 1);
        }
    } else if (filename[0] != '/') {
        char* cwdresult = getcwd(buffer_dir, 1024);
        if (cwdresult != NULL)
            buffer_file = (char *) malloc(strlen(filename) + strlen(buffer_dir) + 2);
        if (buffer_file == NULL || cwdresult == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, filename, errno);
            return NULL;
        }
        strcpy(buffer_file, buffer_dir);
        if (buffer_dir[strlen(buffer_dir) - 1] != '/')
            strcat(buffer_file, "/");
        strcat(buffer_file, filename);
    }
#endif /* unix builds */

    if (buffer_file == NULL) {
        buffer_file = (char *) malloc(strlen(filename) + 1);
        if (buffer_file == NULL) {
            _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, filename, errno);
            return NULL;
        }
        strcpy(buffer_file, filename);
    }

#ifdef __DJGPP__
    if (findfirst(buffer_file, &f, FA_ARCH | FA_RDONLY) != 0) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
        free(buffer_file);
        return NULL;
    }
    *size = f.ff_fsize;
#elif defined(_WIN32)
    if ((h = FindFirstFileA(buffer_file, &wfd)) == INVALID_HANDLE_VALUE) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, ENOENT);
        free(buffer_file);
        return NULL;
    }
    FindClose(h);
    if (wfd.nFileSizeHigh != 0) /* too big */
        *size = 0xffffffff;
    else *size = wfd.nFileSizeLow;
#elif defined(__OS2__) || defined(__EMX__)
    if (DosFindFirst(buffer_file, &h, FILE_NORMAL, &fb, sizeof(fb), &cnt, FIL_STANDARD) != NO_ERROR) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, ENOENT);
        free(buffer_file);
        return NULL;
    }
    DosFindClose(h);
    *size = fb.cbFile;
#elif defined(WILDMIDI_AMIGA)
    if ((filsize = AMIGA_filesize(buffer_file)) < 0) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, ENOENT /* do better!! */);
        free(buffer_file);
        return NULL;
    }
    *size = filsize;
#else
    if (stat(buffer_file, &buffer_stat)) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_STAT, filename, errno);
        free(buffer_file);
        return NULL;
    }
    /* st_size can be sint32 or int64. */
    if (buffer_stat.st_size > WM_MAXFILESIZE) /* too big */
        *size = 0xffffffff;
    else *size = buffer_stat.st_size;
#endif

    if (__builtin_expect((*size > WM_MAXFILESIZE), 0)) {
        /* don't bother loading suspiciously long files */
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_LONGFIL, filename, 0);
        free(buffer_file);
        return NULL;
    }

    /* +1 needed for parsing text files without a newline at the end */
    data = (uint8_t *) malloc(*size + 1);
    if (data == NULL) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, filename, errno);
        free(buffer_file);
        return NULL;
    }

#if defined(WILDMIDI_AMIGA)
    if (!(buffer_fd = AMIGA_open(buffer_file))) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_OPEN, filename, ENOENT /* do better!! */);
        free(buffer_file);
        free(data);
        return NULL;
    }
    if (AMIGA_read(buffer_fd, data, filsize) != filsize) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_READ, filename, EIO /* do better!! */);
        free(buffer_file);
        free(data);
        AMIGA_close(buffer_fd);
        return NULL;
    }
    AMIGA_close(buffer_fd);
    free(buffer_file);
#else
    if ((buffer_fd = open(buffer_file,(O_RDONLY | O_BINARY))) == -1) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_OPEN, filename, errno);
        free(buffer_file);
        free(data);
        return NULL;
    }
    if (read(buffer_fd, data, *size) != (long) *size) {
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_READ, filename, errno);
        free(buffer_file);
        free(data);
        close(buffer_fd);
        return NULL;
    }
    close(buffer_fd);
    free(buffer_file);
#endif

    data[*size] = '\0';
    return data;
}
