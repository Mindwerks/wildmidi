/*
 * fileops.c -- Platform specific file operations
 *
 * Copyright (C) WildMidi Developers 2020
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

#include "fileops.h"

#if defined(_WIN32)
int audio_fd = -1;

int wmidi_fileexists (const char *path) {
    return (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES);
}
int wmidi_open_write (const char *path) {
    return _open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
void wmidi_close (int fd) {
    _close(fd);
}
long wmidi_seekset (int fd, long ofs) {
    return _lseek(fd, ofs, SEEK_SET);
}
int wmidi_write (int fd, const void *buf, size_t size) {
    return _write(fd, buf, size);
}

#elif defined(__DJGPP__)
int audio_fd = -1;

int wmidi_fileexists (const char *path) {
    struct ffblk f;
    return (findfirst(path, &f, FA_ARCH | FA_RDONLY) == 0);
}
int wmidi_open_write (const char *path) {
    return open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
void wmidi_close (int fd) {
    close(fd);
}
off_t wmidi_seekset (int fd, off_t ofs) {
    return lseek(fd, ofs, SEEK_SET);
}
int wmidi_write (int fd, const void *buf, size_t size) {
    return write(fd, buf, size);
}

#elif defined(__OS2__) || defined(__EMX__)
int audio_fd = -1;

int wmidi_fileexists (const char *path) {
    int f = open(path, (O_RDONLY | O_BINARY));
    if (f != -1) {
        close(f);
        return 1;
    } else {
        return 0;
    }
}
int wmidi_open_write (const char *path) {
    return open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
void wmidi_close (int fd) {
    close(fd);
}
off_t wmidi_seekset (int fd, off_t ofs) {
    return lseek(fd, ofs, SEEK_SET);
}
int wmidi_write (int fd, const void *buf, size_t size) {
    return write(fd, buf, size);
}

#elif defined(WILDMIDI_AMIGA)
BPTR audio_fd = 0;

#undef wmidi_geterrno
int wmidi_geterrno (void) {
    switch (IoErr()) {
    case ERROR_OBJECT_NOT_FOUND: return ENOENT;
    case ERROR_DISK_FULL: return ENOSPC;
    }
    return EIO; /* better ?? */
}
int wmidi_fileexists (const char *path) {
    BPTR fd = Open((const STRPTR)path, MODE_OLDFILE);
    if (!fd) return 0;
    Close(fd); return 1;
}
BPTR wmidi_open_write (const char *path) {
    return Open((const STRPTR) path, MODE_NEWFILE);
}
LONG wmidi_close (BPTR fd) {
    return Close(fd);
}
LONG wmidi_seekset (BPTR fd, LONG ofs) {
    return Seek(fd, ofs, OFFSET_BEGINNING);
}
LONG wmidi_write (BPTR fd, /*const*/ void *buf, LONG size) {
    LONG written = 0, result;
    unsigned char *p = (unsigned char *)buf;
    while (written < size) {
        result = Write(fd, p + written, size - written);
        if (result < 0) return result;
        written += result;
    }
    return written;
}

#else /* common posix case */
int audio_fd = -1;

int wmidi_fileexists (const char *path) {
  struct stat st;
  return (stat(path, &st) == 0);
}
int wmidi_open_write (const char *path) {
  return open(path, (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
}
int wmidi_close (int fd) {
  return close(fd);
}
off_t wmidi_seekset (int fd, off_t ofs) {
  return lseek(fd, ofs, SEEK_SET);
}
ssize_t wmidi_write (int fd, const void *buf, size_t size) {
  return write(fd, buf, size);
}

int msleep(unsigned long millisec) {
  struct timespec req = { 0, 0 };
  time_t sec = (int) (millisec / 1000);
  millisec = millisec - (sec * 1000);
  req.tv_sec = sec;
  req.tv_nsec = millisec * 1000000L;
  while (nanosleep(&req, &req) == -1)
    continue;
  return (1);
}

#endif
