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

#elif defined(__OS2__) || defined(__EMX__)
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_OS2MM
#ifdef __EMX__
#define INCL_KBD
#define INCL_VIO
#endif
#include <os2.h>
#include <conio.h>
#define msleep(s) DosSleep((s))
#include <fcntl.h>
#include <io.h>
#include "getopt_long.h"
#ifdef __EMX__
#include <sys/types.h> /* for off_t typedef */
#endif

#elif defined(WILDMIDI_AMIGA)
extern void amiga_sysinit (void);
extern int amiga_usleep(unsigned long millisec);
#define msleep(s) amiga_usleep((s)*1000)
extern int amiga_getch (unsigned char *ch);
#include <proto/exec.h>
#include <proto/dos.h>
#include "getopt_long.h"

#else /* unix build */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

int msleep(unsigned long millisec);

#endif /* !_WIN32, !__DJGPP__ (unix build) */

#endif // FILEOPS_H
