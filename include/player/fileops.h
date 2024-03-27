/*
 * fileops.h -- Platform specific file operations
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

#ifndef FILEOPS_H
#define FILEOPS_H

#define wmidi_geterrno() errno /* generic case */

#if defined(__DJGPP__)
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "getopt_long.h"
#include <conio.h>
#define getopt dj_getopt /* hack */
#include <unistd.h>
#undef getopt
#define msleep(s) usleep((s)*1000)
#include <io.h>
#include <dir.h>

#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1

extern int audio_fd;

int wmidi_fileexists (const char *path);
int wmidi_open_write (const char *path);
void wmidi_close (int fd);
off_t wmidi_seekset (int fd, off_t ofs);
int wmidi_write (int fd, const void *buf, size_t size);


#elif (defined _WIN32) || (defined __CYGWIN__)
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#define msleep(s) Sleep((s))
#include <io.h>
#include "getopt_long.h"
#ifdef __WATCOMC__
#define _putch putch
#endif

#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF (-1)

extern int audio_fd;

int wmidi_fileexists (const char *path);
int wmidi_open_write (const char *path);
void wmidi_close (int fd);
long wmidi_seekset (int fd, long ofs);
int wmidi_write (int fd, const void *buf, size_t size);


#elif defined(__OS2__) || defined(__EMX__)
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_OS2MM
#ifdef __EMX__
#define INCL_KBD
#define INCL_VIO
#endif
#include <os2.h>
#include <os2me.h>
#include <conio.h>
#define msleep(s) DosSleep((s))
#include <fcntl.h>
#include <io.h>
#include "getopt_long.h"
#ifdef __EMX__
#include <sys/types.h> /* for off_t typedef */
int putch (int c) {
    char ch = c;
    VioWrtTTY(&ch, 1, 0);
    return c;
}
int kbhit (void) {
    KBDKEYINFO k;
    if (KbdPeek(&k, 0))
        return 0;
    return (k.fbStatus & KBDTRF_FINAL_CHAR_IN);
}
#endif

#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1
extern int audio_fd;

int wmidi_fileexists (const char *path);
int wmidi_open_write (const char *path);
void wmidi_close (int fd);
off_t wmidi_seekset (int fd, off_t ofs);
int wmidi_write (int fd, const void *buf, size_t size);

#elif defined(WILDMIDI_AMIGA)
extern void amiga_sysinit (void);
extern int amiga_usleep(unsigned long millisec);
#define msleep(s) amiga_usleep((s)*1000)
extern int amiga_getch (unsigned char *ch);
#include <proto/exec.h>
#include <proto/dos.h>
#include "getopt_long.h"

#define WM_IS_BADF(_fd) ((_fd)==0)
#define WM_BADF 0

extern BPTR audio_fd;

#undef wmidi_geterrno
#include <errno.h>
int wmidi_geterrno (void);
int wmidi_fileexists (const char *path);
BPTR wmidi_open_write (const char *path);
LONG wmidi_close (BPTR fd);
LONG wmidi_seekset (BPTR fd, LONG ofs);
LONG wmidi_write (BPTR fd, /*const*/ void *buf, LONG size);

#else /* unix build */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF (-1)

extern int audio_fd;

int msleep(unsigned long millisec);
int wmidi_fileexists (const char *path);
int wmidi_open_write (const char *path);
int wmidi_close (int fd);
off_t wmidi_seekset (int fd, off_t ofs);
ssize_t wmidi_write (int fd, const void *buf, size_t size);

#endif /* !_WIN32, !__DJGPP__ (unix build) */

#endif // FILEOPS_H
