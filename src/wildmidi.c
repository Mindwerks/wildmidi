/*
 * wildmidi.c -- Midi Player using the WildMidi Midi Processing Library
 *
 * Copyright (C) WildMidi Developers 2001-2016
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#ifdef AUDIODRV_DOSSB
#include "dossb.h"
#endif

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

#elif defined(WILDMIDI_AMIGA)
extern void amiga_sysinit (void);
extern int amiga_usleep(unsigned long millisec);
#define msleep(s) amiga_usleep((s)*1000)
extern int amiga_getch (unsigned char *ch);
#include <proto/exec.h>
#include <proto/dos.h>
#include "getopt_long.h"
#ifdef AUDIODRV_AHI
#include <devices/ahi.h>
#endif

#else /* unix build */
static int msleep(unsigned long millisec);
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#ifdef AUDIODRV_ALSA
#  include <alsa/asoundlib.h>
#elif defined AUDIODRV_OSS
#   if defined HAVE_SYS_SOUNDCARD_H
#   include <sys/soundcard.h>
#   elif defined HAVE_MACHINE_SOUNDCARD_H
#   include <machine/soundcard.h>
#   elif defined HAVE_SOUNDCARD_H
#   include <soundcard.h> /* less common, but exists. */
#   endif
#elif defined AUDIODRV_OPENAL
#   include <al.h>
#   include <alc.h>
#endif
#endif /* !_WIN32, !__DJGPP__ (unix build) */

#include "wildmidi_lib.h"
#include "wm_tty.h"
#include "filenames.h"

struct _midi_test {
    uint8_t *data;
    uint32_t size;
};

/* scale test from 0 to 127
 * test a
 * offset 18-21 (0x12-0x15) - track size
 * offset 25 (0x1A) = bank number
 * offset 28 (0x1D) = patch number
 */
static uint8_t midi_test_c_scale[] = {
    0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, /* 0x00    */
    0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x4d, 0x54, /* 0x08    */
    0x72, 0x6b, 0x00, 0x00, 0x02, 0x63, 0x00, 0xb0, /* 0x10    */
    0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x90, 0x00, /* 0x18  C */
    0x64, 0x08, 0x80, 0x00, 0x00, 0x08, 0x90, 0x02, /* 0x20  D */
    0x64, 0x08, 0x80, 0x02, 0x00, 0x08, 0x90, 0x04, /* 0x28  E */
    0x64, 0x08, 0x80, 0x04, 0x00, 0x08, 0x90, 0x05, /* 0x30  F */
    0x64, 0x08, 0x80, 0x05, 0x00, 0x08, 0x90, 0x07, /* 0x38  G */
    0x64, 0x08, 0x80, 0x07, 0x00, 0x08, 0x90, 0x09, /* 0x40  A */
    0x64, 0x08, 0x80, 0x09, 0x00, 0x08, 0x90, 0x0b, /* 0x48  B */
    0x64, 0x08, 0x80, 0x0b, 0x00, 0x08, 0x90, 0x0c, /* 0x50  C */
    0x64, 0x08, 0x80, 0x0c, 0x00, 0x08, 0x90, 0x0e, /* 0x58  D */
    0x64, 0x08, 0x80, 0x0e, 0x00, 0x08, 0x90, 0x10, /* 0x60  E */
    0x64, 0x08, 0x80, 0x10, 0x00, 0x08, 0x90, 0x11, /* 0x68  F */
    0x64, 0x08, 0x80, 0x11, 0x00, 0x08, 0x90, 0x13, /* 0x70  G */
    0x64, 0x08, 0x80, 0x13, 0x00, 0x08, 0x90, 0x15, /* 0x78  A */
    0x64, 0x08, 0x80, 0x15, 0x00, 0x08, 0x90, 0x17, /* 0x80  B */
    0x64, 0x08, 0x80, 0x17, 0x00, 0x08, 0x90, 0x18, /* 0x88  C */
    0x64, 0x08, 0x80, 0x18, 0x00, 0x08, 0x90, 0x1a, /* 0x90  D */
    0x64, 0x08, 0x80, 0x1a, 0x00, 0x08, 0x90, 0x1c, /* 0x98  E */
    0x64, 0x08, 0x80, 0x1c, 0x00, 0x08, 0x90, 0x1d, /* 0xA0  F */
    0x64, 0x08, 0x80, 0x1d, 0x00, 0x08, 0x90, 0x1f, /* 0xA8  G */
    0x64, 0x08, 0x80, 0x1f, 0x00, 0x08, 0x90, 0x21, /* 0xB0  A */
    0x64, 0x08, 0x80, 0x21, 0x00, 0x08, 0x90, 0x23, /* 0xB8  B */
    0x64, 0x08, 0x80, 0x23, 0x00, 0x08, 0x90, 0x24, /* 0xC0  C */
    0x64, 0x08, 0x80, 0x24, 0x00, 0x08, 0x90, 0x26, /* 0xC8  D */
    0x64, 0x08, 0x80, 0x26, 0x00, 0x08, 0x90, 0x28, /* 0xD0  E */
    0x64, 0x08, 0x80, 0x28, 0x00, 0x08, 0x90, 0x29, /* 0xD8  F */
    0x64, 0x08, 0x80, 0x29, 0x00, 0x08, 0x90, 0x2b, /* 0xE0  G */
    0x64, 0x08, 0x80, 0x2b, 0x00, 0x08, 0x90, 0x2d, /* 0xE8  A */
    0x64, 0x08, 0x80, 0x2d, 0x00, 0x08, 0x90, 0x2f, /* 0xF0  B */
    0x64, 0x08, 0x80, 0x2f, 0x00, 0x08, 0x90, 0x30, /* 0xF8  C */
    0x64, 0x08, 0x80, 0x30, 0x00, 0x08, 0x90, 0x32, /* 0x100 D */
    0x64, 0x08, 0x80, 0x32, 0x00, 0x08, 0x90, 0x34, /* 0x108 E */
    0x64, 0x08, 0x80, 0x34, 0x00, 0x08, 0x90, 0x35, /* 0x110 F */
    0x64, 0x08, 0x80, 0x35, 0x00, 0x08, 0x90, 0x37, /* 0x118 G */
    0x64, 0x08, 0x80, 0x37, 0x00, 0x08, 0x90, 0x39, /* 0x120 A */
    0x64, 0x08, 0x80, 0x39, 0x00, 0x08, 0x90, 0x3b, /* 0X128 B */
    0x64, 0x08, 0x80, 0x3b, 0x00, 0x08, 0x90, 0x3c, /* 0x130 C */
    0x64, 0x08, 0x80, 0x3c, 0x00, 0x08, 0x90, 0x3e, /* 0x138 D */
    0x64, 0x08, 0x80, 0x3e, 0x00, 0x08, 0x90, 0x40, /* 0X140 E */
    0x64, 0x08, 0x80, 0x40, 0x00, 0x08, 0x90, 0x41, /* 0x148 F */
    0x64, 0x08, 0x80, 0x41, 0x00, 0x08, 0x90, 0x43, /* 0x150 G */
    0x64, 0x08, 0x80, 0x43, 0x00, 0x08, 0x90, 0x45, /* 0x158 A */
    0x64, 0x08, 0x80, 0x45, 0x00, 0x08, 0x90, 0x47, /* 0x160 B */
    0x64, 0x08, 0x80, 0x47, 0x00, 0x08, 0x90, 0x48, /* 0x168 C */
    0x64, 0x08, 0x80, 0x48, 0x00, 0x08, 0x90, 0x4a, /* 0x170 D */
    0x64, 0x08, 0x80, 0x4a, 0x00, 0x08, 0x90, 0x4c, /* 0x178 E */
    0x64, 0x08, 0x80, 0x4c, 0x00, 0x08, 0x90, 0x4d, /* 0x180 F */
    0x64, 0x08, 0x80, 0x4d, 0x00, 0x08, 0x90, 0x4f, /* 0x188 G */
    0x64, 0x08, 0x80, 0x4f, 0x00, 0x08, 0x90, 0x51, /* 0x190 A */
    0x64, 0x08, 0x80, 0x51, 0x00, 0x08, 0x90, 0x53, /* 0x198 B */
    0x64, 0x08, 0x80, 0x53, 0x00, 0x08, 0x90, 0x54, /* 0x1A0 C */
    0x64, 0x08, 0x80, 0x54, 0x00, 0x08, 0x90, 0x56, /* 0x1A8 D */
    0x64, 0x08, 0x80, 0x56, 0x00, 0x08, 0x90, 0x58, /* 0x1B0 E */
    0x64, 0x08, 0x80, 0x58, 0x00, 0x08, 0x90, 0x59, /* 0x1B8 F */
    0x64, 0x08, 0x80, 0x59, 0x00, 0x08, 0x90, 0x5b, /* 0x1C0 G */
    0x64, 0x08, 0x80, 0x5b, 0x00, 0x08, 0x90, 0x5d, /* 0x1C8 A */
    0x64, 0x08, 0x80, 0x5d, 0x00, 0x08, 0x90, 0x5f, /* 0x1D0 B */
    0x64, 0x08, 0x80, 0x5f, 0x00, 0x08, 0x90, 0x60, /* 0x1D8 C */
    0x64, 0x08, 0x80, 0x60, 0x00, 0x08, 0x90, 0x62, /* 0x1E0 D */
    0x64, 0x08, 0x80, 0x62, 0x00, 0x08, 0x90, 0x64, /* 0x1E8 E */
    0x64, 0x08, 0x80, 0x64, 0x00, 0x08, 0x90, 0x65, /* 0x1F0 F */
    0x64, 0x08, 0x80, 0x65, 0x00, 0x08, 0x90, 0x67, /* 0x1F8 G */
    0x64, 0x08, 0x80, 0x67, 0x00, 0x08, 0x90, 0x69, /* 0x200 A */
    0x64, 0x08, 0x80, 0x69, 0x00, 0x08, 0x90, 0x6b, /* 0x208 B */
    0x64, 0x08, 0x80, 0x6b, 0x00, 0x08, 0x90, 0x6c, /* 0x210 C */
    0x64, 0x08, 0x80, 0x6c, 0x00, 0x08, 0x90, 0x6e, /* 0x218 D */
    0x64, 0x08, 0x80, 0x6e, 0x00, 0x08, 0x90, 0x70, /* 0x220 E */
    0x64, 0x08, 0x80, 0x70, 0x00, 0x08, 0x90, 0x71, /* 0x228 F */
    0x64, 0x08, 0x80, 0x71, 0x00, 0x08, 0x90, 0x73, /* 0x230 G */
    0x64, 0x08, 0x80, 0x73, 0x00, 0x08, 0x90, 0x75, /* 0x238 A */
    0x64, 0x08, 0x80, 0x75, 0x00, 0x08, 0x90, 0x77, /* 0x240 B */
    0x64, 0x08, 0x80, 0x77, 0x00, 0x08, 0x90, 0x78, /* 0x248 C */
    0x64, 0x08, 0x80, 0x78, 0x00, 0x08, 0x90, 0x7a, /* 0x250 D */
    0x64, 0x08, 0x80, 0x7a, 0x00, 0x08, 0x90, 0x7c, /* 0x258 E */
    0x64, 0x08, 0x80, 0x7c, 0x00, 0x08, 0x90, 0x7d, /* 0x260 F */
    0x64, 0x08, 0x80, 0x7d, 0x00, 0x08, 0x90, 0x7f, /* 0x268 G */
    0x64, 0x08, 0x80, 0x7f, 0x00, 0x08, 0xff, 0x2f, /* 0x270   */
    0x00                                            /* 0x278   */
};

static struct _midi_test midi_test[] = {
    { midi_test_c_scale, 663 },
    { NULL, 0 }
};

static int midi_test_max = 1;

/*
 ==============================
 Audio Output Functions

 We have two 'drivers': first is the wav file writer which is
 always available. the second, if it is really compiled in,
 is the system audio output driver. only _one of the two_ can
 be active, not both.
 ==============================
 */

static unsigned int rate = 32072;

static int (*send_output)(int8_t *output_data, int output_size);
static void (*close_output)(void);
static void (*pause_output)(void);
static void (*resume_output)(void);

#define wmidi_geterrno() errno /* generic case */
#if defined(_WIN32)
static int audio_fd = -1;
#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1
static inline int wmidi_fileexists (const char *path) {
    return (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES);
}
static inline int wmidi_open_write (const char *path) {
    return _open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
static inline void wmidi_close (int fd) {
    _close(fd);
}
static inline long wmidi_seekset (int fd, long ofs) {
    return _lseek(fd, ofs, SEEK_SET);
}
static inline int wmidi_write (int fd, const void *buf, size_t size) {
    return _write(fd, buf, size);
}

#elif defined(__DJGPP__)
static int audio_fd = -1;
#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1
static inline int wmidi_fileexists (const char *path) {
    struct ffblk f;
    return (findfirst(path, &f, FA_ARCH | FA_RDONLY) == 0);
}
static inline int wmidi_open_write (const char *path) {
    return open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
static inline void wmidi_close (int fd) {
    close(fd);
}
static inline off_t wmidi_seekset (int fd, off_t ofs) {
    return lseek(fd, ofs, SEEK_SET);
}
static inline int wmidi_write (int fd, const void *buf, size_t size) {
    return write(fd, buf, size);
}

#elif defined(__OS2__) || defined(__EMX__)
static int audio_fd = -1;
#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1
static inline int wmidi_fileexists (const char *path) {
    int f = open(path, (O_RDONLY | O_BINARY));
    if (f != -1) { close(f); return 1; } else return 0;
}
static inline int wmidi_open_write (const char *path) {
    return open(path, (O_RDWR | O_CREAT | O_TRUNC | O_BINARY), 0664);
}
static inline void wmidi_close (int fd) {
    close(fd);
}
static inline off_t wmidi_seekset (int fd, off_t ofs) {
    return lseek(fd, ofs, SEEK_SET);
}
static inline int wmidi_write (int fd, const void *buf, size_t size) {
    return write(fd, buf, size);
}

#elif defined(WILDMIDI_AMIGA)
static BPTR audio_fd = 0;
#define WM_IS_BADF(_fd) ((_fd)==0)
#define WM_BADF 0
#undef wmidi_geterrno
static int wmidi_geterrno (void) {
    switch (IoErr()) {
    case ERROR_OBJECT_NOT_FOUND: return ENOENT;
    case ERROR_DISK_FULL: return ENOSPC;
    }
    return EIO; /* better ?? */
}
static inline int wmidi_fileexists (const char *path) {
    BPTR fd = Open((const STRPTR)path, MODE_OLDFILE);
    if (!fd) return 0;
    Close(fd); return 1;
}
static inline BPTR wmidi_open_write (const char *path) {
    return Open((const STRPTR) path, MODE_NEWFILE);
}
static inline LONG wmidi_close (BPTR fd) {
    return Close(fd);
}
static inline LONG wmidi_seekset (BPTR fd, LONG ofs) {
    return Seek(fd, ofs, OFFSET_BEGINNING);
}
static LONG wmidi_write (BPTR fd, /*const*/ void *buf, LONG size) {
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
static int audio_fd = -1;
#define WM_IS_BADF(_fd) ((_fd)<0)
#define WM_BADF -1
static inline int wmidi_fileexists (const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}
static inline int wmidi_open_write (const char *path) {
    return open(path, (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
}
static inline int wmidi_close (int fd) {
    return close(fd);
}
static inline off_t wmidi_seekset (int fd, off_t ofs) {
    return lseek(fd, ofs, SEEK_SET);
}
static inline ssize_t wmidi_write (int fd, const void *buf, size_t size) {
    return write(fd, buf, size);
}
#endif

static void pause_output_nop(void) {
}
static void resume_output_nop(void) {
}

/*
 MIDI Output Functions
 */
static char midi_file[1024];

static int write_midi_output(void *output_data, int output_size) {
    if (midi_file[0] == '\0')
        return (-1);

/*
 * Test if file already exists 
 */
    if (wmidi_fileexists(midi_file)) {
        fprintf(stderr, "\rError: %s already exists\r\n", midi_file);
        return (-1);
    }

    audio_fd = wmidi_open_write(midi_file);
    if (WM_IS_BADF(audio_fd)) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(wmidi_geterrno()));
        return (-1);
    }

    if (wmidi_write(audio_fd, output_data, output_size) < 0) {
        fprintf(stderr, "\nERROR: failed writing midi (%s)\r\n", strerror(wmidi_geterrno()));
        wmidi_close(audio_fd);
        audio_fd = WM_BADF;
        return (-1);
    }

    wmidi_close(audio_fd);
    audio_fd = WM_BADF;
    return (0);
}

/*
 Wav Output Functions
 */

static char wav_file[1024];
static uint32_t wav_size;

static int write_wav_output(int8_t *output_data, int output_size);
static void close_wav_output(void);

static int open_wav_output(void) {
    uint8_t wav_hdr[] = {
        0x52, 0x49, 0x46, 0x46, /* "RIFF"  */
        0x00, 0x00, 0x00, 0x00, /* riffsize: pcm size + 36 (filled when closing.) */
        0x57, 0x41, 0x56, 0x45, /* "WAVE"  */
        0x66, 0x6D, 0x74, 0x20, /* "fmt "  */
        0x10, 0x00, 0x00, 0x00, /* length of this RIFF block: 16  */
        0x01, 0x00,             /* wave format == 1 (WAVE_FORMAT_PCM)  */
        0x02, 0x00,             /* channels == 2  */
        0x00, 0x00, 0x00, 0x00, /* sample rate (filled below)  */
        0x00, 0x00, 0x00, 0x00, /* bytes_per_sec: rate * channels * format bytes  */
        0x04, 0x00,             /* block alignment: channels * format bytes == 4  */
        0x10, 0x00,             /* format bits == 16  */
        0x64, 0x61, 0x74, 0x61, /* "data"  */
        0x00, 0x00, 0x00, 0x00  /* datasize: the pcm size (filled when closing.)  */
    };

    if (wav_file[0] == '\0')
        return (-1);

    audio_fd = wmidi_open_write(wav_file);
    if (WM_IS_BADF(audio_fd)) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(wmidi_geterrno()));
        return (-1);
    } else {
        uint32_t bytes_per_sec;

        wav_hdr[24] = (rate) & 0xFF;
        wav_hdr[25] = (rate >> 8) & 0xFF;

        bytes_per_sec = rate * 4;
        wav_hdr[28] = (bytes_per_sec) & 0xFF;
        wav_hdr[29] = (bytes_per_sec >> 8) & 0xFF;
        wav_hdr[30] = (bytes_per_sec >> 16) & 0xFF;
        wav_hdr[31] = (bytes_per_sec >> 24) & 0xFF;
    }

    if (wmidi_write(audio_fd, wav_hdr, 44) < 0) {
        fprintf(stderr, "ERROR: failed writing wav header (%s)\r\n", strerror(wmidi_geterrno()));
        wmidi_close(audio_fd);
        audio_fd = WM_BADF;
        return (-1);
    }

    wav_size = 0;
    send_output = write_wav_output;
    close_output = close_wav_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;
    return (0);
}

static int write_wav_output(int8_t *output_data, int output_size) {
#ifdef WORDS_BIGENDIAN
/* libWildMidi outputs host-endian, *.wav must have little-endian. */
    uint16_t *swp = (uint16_t *) output_data;
    int i = (output_size / 2) - 1;
    for (; i >= 0; --i) {
        swp[i] = (swp[i] << 8) | (swp[i] >> 8);
    }
#endif
    if (wmidi_write(audio_fd, output_data, output_size) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(wmidi_geterrno()));
        wmidi_close(audio_fd);
        audio_fd = WM_BADF;
        return (-1);
    }

    wav_size += output_size;
    return (0);
}

static void close_wav_output(void) {
    uint8_t wav_count[4];
    if (WM_IS_BADF(audio_fd))
        return;

    printf("Finishing and closing wav output\r");
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    wmidi_seekset(audio_fd, 40);
    if (wmidi_write(audio_fd, wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(wmidi_geterrno()));
        goto end;
    }

    wav_size += 36;
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    wmidi_seekset(audio_fd, 4);
    if (wmidi_write(audio_fd, wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(wmidi_geterrno()));
        goto end;
    }

end:    printf("\n");
    if (!WM_IS_BADF(audio_fd))
        wmidi_close(audio_fd);
    audio_fd = WM_BADF;
}

#if (defined _WIN32) || (defined __CYGWIN__)

static HWAVEOUT hWaveOut = NULL;
static CRITICAL_SECTION waveCriticalSection;

#define open_audio_output open_mm_output
static int write_mm_output (int8_t *output_data, int output_size);
static void close_mm_output (void);

static WAVEHDR *mm_blocks = NULL;
#define MM_BLOCK_SIZE 16384
#define MM_BLOCK_COUNT 3

static DWORD mm_free_blocks = MM_BLOCK_COUNT;
static DWORD mm_current_block = 0;

#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef DWORD DWORD_PTR;
#endif

static void CALLBACK mmOutProc (HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    /* unused params */
    (void)hWaveOut;
    (void)dwParam1;
    (void)dwParam2;

    if(uMsg != WOM_DONE)
        return;
    /* increment mm_free_blocks */
    EnterCriticalSection(&waveCriticalSection);
    (*(DWORD *)dwInstance)++;
    LeaveCriticalSection(&waveCriticalSection);
}

static int
open_mm_output (void) {
    WAVEFORMATEX wfx;
    char *mm_buffer;
    int i;

    InitializeCriticalSection(&waveCriticalSection);

    if((mm_buffer = (char *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((MM_BLOCK_SIZE + sizeof(WAVEHDR)) * MM_BLOCK_COUNT))) == NULL) {
        fprintf(stderr, "Memory allocation error\r\n");
        return -1;
    }

    mm_blocks = (WAVEHDR*)mm_buffer;
    mm_buffer += sizeof(WAVEHDR) * MM_BLOCK_COUNT;

    for(i = 0; i < MM_BLOCK_COUNT; i++) {
        mm_blocks[i].dwBufferLength = MM_BLOCK_SIZE;
        mm_blocks[i].lpData = mm_buffer;
        mm_buffer += MM_BLOCK_SIZE;
    }

    wfx.nSamplesPerSec = rate;
    wfx.wBitsPerSample = 16;
    wfx.nChannels = 2;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)mmOutProc, (DWORD_PTR)&mm_free_blocks, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        fprintf(stderr, "unable to open WAVE_MAPPER device\r\n");
        HeapFree(GetProcessHeap(), 0, mm_blocks);
        hWaveOut = NULL;
        mm_blocks = NULL;
        return -1;
    }

    send_output = write_mm_output;
    close_output = close_mm_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;
    return (0);
}

static int
write_mm_output (int8_t *output_data, int output_size) {
    WAVEHDR* current;
    int free_size = 0;
    int data_read = 0;
    current = &mm_blocks[mm_current_block];

    while (output_size) {
        if(current->dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        free_size = MM_BLOCK_SIZE - current->dwUser;
        if (free_size > output_size)
            free_size = output_size;

        memcpy(current->lpData + current->dwUser, &output_data[data_read], free_size);
        current->dwUser += free_size;
        output_size -= free_size;
        data_read += free_size;

        if (current->dwUser < MM_BLOCK_SIZE) {
            return (0);
        }

        current->dwBufferLength = MM_BLOCK_SIZE;
        waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
        EnterCriticalSection(&waveCriticalSection);
        mm_free_blocks--;
        LeaveCriticalSection(&waveCriticalSection);
        while(!mm_free_blocks)
            Sleep(10);
        mm_current_block++;
        mm_current_block %= MM_BLOCK_COUNT;
        current = &mm_blocks[mm_current_block];
        current->dwUser = 0;
    }
    return (0);
}

static void
close_mm_output (void) {
    int i;

    if (!hWaveOut) return;

    printf("Shutting down sound output\r\n");
    for (i = 0; i < MM_BLOCK_COUNT; i++) {
        while (waveOutUnprepareHeader(hWaveOut, &mm_blocks[i], sizeof(WAVEHDR))
                == WAVERR_STILLPLAYING) {
            Sleep(10);
        }
    }

    waveOutClose (hWaveOut);
    HeapFree(GetProcessHeap(), 0, mm_blocks);
    hWaveOut = NULL;
    mm_blocks = NULL;
}

#elif (defined(__OS2__) || defined(__EMX__)) && defined(AUDIODRV_OS2DART)
/* based on Dart code originally written by Kevin Langman for XMP */

#define open_audio_output open_dart_output
static int write_dart_output (int8_t *output_data, int output_size);
static void close_dart_output (void);

#define BUFFERCOUNT 4

static MCI_MIX_BUFFER MixBuffers[BUFFERCOUNT];
static MCI_MIXSETUP_PARMS MixSetupParms;
static MCI_BUFFER_PARMS BufferParms;
static MCI_GENERIC_PARMS GenericParms;

static ULONG DeviceID = 0;
static ULONG bsize = 16;
static short next = 2;
static short ready = 1;

static HMTX dart_mutex;

/* Buffer update thread (created and called by DART) */
static LONG APIENTRY OS2_Dart_UpdateBuffers
    (ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags) {

    (void) pBuffer;/* unused param */

    if ((ulFlags == MIX_WRITE_COMPLETE) ||
        ((ulFlags == (MIX_WRITE_COMPLETE | MIX_STREAM_ERROR)) &&
         (ulStatus == ERROR_DEVICE_UNDERRUN))) {
        DosRequestMutexSem(dart_mutex, SEM_INDEFINITE_WAIT);
        ready++;
        DosReleaseMutexSem(dart_mutex);
    }
    return (TRUE);
}

static int
open_dart_output(void) {
    int i;
    MCI_AMP_OPEN_PARMS AmpOpenParms;

    if (DosCreateMutexSem(NULL, &dart_mutex, 0, 0) != NO_ERROR) {
        fprintf(stderr, "Failed creating a MutexSem.\r\n");
        return (-1);
    }

    /* compute a size for circa 1/4" of playback. */
    bsize = rate >> 2;
    bsize <<= 1; /* stereo */
    bsize <<= 1; /* 16 bit */
    for (i = 15; i >= 12; i--) {
        if (bsize & (1 << i))
            break;
    }
    bsize = (1 << i);
    /* make sure buffer is not greater than 64 Kb: DART can't handle it. */
    if (bsize > 65536)
        bsize = 65536;

    MixBuffers[0].pBuffer = NULL; /* marker */
    memset(&GenericParms, 0, sizeof(MCI_GENERIC_PARMS));

    /* open AMP device */
    memset(&AmpOpenParms, 0, sizeof(MCI_AMP_OPEN_PARMS));
    AmpOpenParms.usDeviceID = 0;

    AmpOpenParms.pszDeviceType =
        (PSZ) MAKEULONG(MCI_DEVTYPE_AUDIO_AMPMIX, 0); /* 0: default waveaudio device */

    if(mciSendCommand(0, MCI_OPEN, MCI_WAIT|MCI_OPEN_TYPE_ID|MCI_OPEN_SHAREABLE,
                       (PVOID) &AmpOpenParms, 0) != MCIERR_SUCCESS) {
        fprintf(stderr, "Failed opening DART audio device\r\n");
        return (-1);
    }

    DeviceID = AmpOpenParms.usDeviceID;

    /* setup playback parameters */
    memset(&MixSetupParms, 0, sizeof(MCI_MIXSETUP_PARMS));

    MixSetupParms.ulBitsPerSample = 16;
    MixSetupParms.ulFormatTag = MCI_WAVE_FORMAT_PCM;
    MixSetupParms.ulSamplesPerSec = rate;
    MixSetupParms.ulChannels = 2;
    MixSetupParms.ulFormatMode = MCI_PLAY;
    MixSetupParms.ulDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    MixSetupParms.pmixEvent = OS2_Dart_UpdateBuffers;

    if (mciSendCommand(DeviceID, MCI_MIXSETUP,
                       MCI_WAIT | MCI_MIXSETUP_INIT,
                       (PVOID) & MixSetupParms, 0) != MCIERR_SUCCESS) {

        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) & GenericParms, 0);
        fprintf(stderr, "Failed DART mixer setup\r\n");
        return (-1);
    }

    /*bsize = MixSetupParms.ulBufferSize;*/
    /*printf("Dart Buffer Size = %lu\n", bsize);*/

    BufferParms.ulNumBuffers = BUFFERCOUNT;
    BufferParms.ulBufferSize = bsize;
    BufferParms.pBufList = MixBuffers;

    if (mciSendCommand(DeviceID, MCI_BUFFER,
                       MCI_WAIT | MCI_ALLOCATE_MEMORY,
                       (PVOID) & BufferParms, 0) != MCIERR_SUCCESS) {
        fprintf(stderr, "DART Memory allocation error\r\n");
        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) & GenericParms, 0);
        return (-1);
    }

    for (i = 0; i < BUFFERCOUNT; i++) {
        MixBuffers[i].ulBufferLength = bsize;
    }

    /* Start Playback */
    memset(MixBuffers[0].pBuffer, /*32767 */ 0, bsize);
    memset(MixBuffers[1].pBuffer, /*32767 */ 0, bsize);
    MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle, MixBuffers, 2);

    send_output = write_dart_output;
    close_output = close_dart_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;

    return (0);
}

static int
write_dart_output (int8_t *output_data, int output_size) {
    static int idx = 0;

    if (idx + output_size > bsize) {
        do {
            DosRequestMutexSem(dart_mutex, SEM_INDEFINITE_WAIT);
            if (ready != 0) {
                DosReleaseMutexSem(dart_mutex);
                break;
            }
            DosReleaseMutexSem(dart_mutex);
            DosSleep(20);
        } while (TRUE);

        MixBuffers[next].ulBufferLength = idx;
        MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle, &(MixBuffers[next]), 1);
        ready--;
        next++;
        idx = 0;
        if (next == BUFFERCOUNT) {
            next = 0;
        }
    }
    memcpy(&((char *)MixBuffers[next].pBuffer)[idx], output_data, output_size);
    idx += output_size;
    return (0);
}

static void
close_dart_output (void) {
    printf("Shutting down sound output\r\n");
    if (MixBuffers[0].pBuffer) {
        mciSendCommand(DeviceID, MCI_BUFFER,
                       MCI_WAIT | MCI_DEALLOCATE_MEMORY, &BufferParms, 0);
        MixBuffers[0].pBuffer = NULL;
    }
    if (DeviceID) {
        mciSendCommand(DeviceID, MCI_CLOSE, MCI_WAIT,
                       (PVOID) &GenericParms, 0);
        DeviceID = 0;
    }
}

#elif defined(__DJGPP__) && defined(AUDIODRV_DOSSB)
/* SoundBlaster/Pro/16/AWE32 driver for DOS -- adapted from
 * libMikMod,  written by Andrew Zabolotny <bit@eltech.ru>,
 * further fixes by O.Sezer <sezero@users.sourceforge.net>.
 * Timer callback functionality replaced by a push mechanism
 * to keep the wildmidi player changes to a minimum, for now.
 */

/* The last buffer byte filled with sound */
static unsigned int buff_tail = 0;

static int write_sb_output(int8_t *data, unsigned int siz) {
    unsigned int dma_size, dma_pos;
    unsigned int cnt;

    sb_query_dma(&dma_size, &dma_pos);
    /* There isn't much sense in filling less than 256 bytes */
    dma_pos &= ~255;

    /* If nothing to mix, quit */
    if (buff_tail == dma_pos)
        return 0;

    /* If DMA pointer still didn't wrapped around ... */
    if (dma_pos > buff_tail) {
        if ((cnt = dma_pos - buff_tail) > siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        /* If we arrived right to the DMA buffer end, jump to the beginning */
        if (buff_tail >= dma_size)
            buff_tail = 0;
    } else {
        /* If wrapped around, fill first to the end of buffer */
        if ((cnt = dma_size - buff_tail) > siz)
            cnt = siz;
        memcpy(sb.dma_buff->linear + buff_tail, data, cnt);
        buff_tail += cnt;
        siz -= cnt;
        if (!siz) return cnt;

        /* Now fill from buffer beginning to current DMA pointer */
        if (dma_pos > siz) dma_pos = siz;
        data += cnt;
        cnt += dma_pos;

        memcpy(sb.dma_buff->linear, data, dma_pos);
        buff_tail = dma_pos;
    }
    return cnt;
}

static int write_sb_s16stereo(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB16 sint16 stereo */
    int i;
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

static int write_sb_u8stereo(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB uint8 stereo */
    int16_t *src = (int16_t *) data;
    uint8_t *dst = (uint8_t *) data;
    int i = (siz /= 2);
    for (; i >= 0; --i) {
        *dst++ = (*src++ >> 8) + 128;
    }
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

static int write_sb_u8mono(int8_t *data, int siz) {
/* libWildMidi sint16 stereo -> SB uint8 mono */
    int16_t *src = (int16_t *) data;
    uint8_t *dst = (uint8_t *) data;
    int i = (siz /= 4); int val;
    for (; i >= 0; --i) {
    /* do a cheap (left+right)/2 */
        val  = *src++;
        val += *src++;
        *dst++ = (val >> 9) + 128;
    }
    while (1) {
        i = write_sb_output(data, siz);
        if ((siz -= i) <= 0) return 0;
        data += i;
        /*usleep(100);*/
    }
}

static void sb_silence_s16(void) {
    memset(sb.dma_buff->linear, 0, sb.dma_buff->size);
}

static void sb_silence_u8(void) {
    memset(sb.dma_buff->linear, 0x80, sb.dma_buff->size);
}

static void close_sb_output(void)
{
    sb.timer_callback = NULL;
    sb_output(FALSE);
    sb_stop_dma();
    sb_close();
}

#define open_audio_output open_sb_output
static int open_sb_output(void)
{
    if (!sb_open()) {
        fprintf(stderr, "Sound Blaster initialization failed.\n");
        return -1;
    }

    if (rate < 4000) rate = 4000;
    if (sb.caps & SBMODE_STEREO) {
        if (rate > sb.maxfreq_stereo)
            rate = sb.maxfreq_stereo;
    } else {
        if (rate > sb.maxfreq_mono)
            rate = sb.maxfreq_mono;
    }

    /* Enable speaker output */
    sb_output(TRUE);

    /* Set our routine to be called during SB IRQs */
    buff_tail = 0;
    sb.timer_callback = NULL;/* see above  */

    /* Start cyclic DMA transfer */
    if (!sb_start_dma(((sb.caps & SBMODE_16BITS) ? SBMODE_16BITS | SBMODE_SIGNED : 0) |
                            (sb.caps & SBMODE_STEREO), rate)) {
        sb_output(FALSE);
        sb_close();
        fprintf(stderr, "Sound Blaster: DMA start failed.\n");
        return -1;
    }

    if (sb.caps & SBMODE_16BITS) { /* can do stereo, too */
        send_output = write_sb_s16stereo;
        pause_output = sb_silence_s16;
        resume_output = resume_output_nop;
        printf("Sound Blaster 16 or compatible (16 bit, stereo, %u Hz)\n", rate);
    } else if (sb.caps & SBMODE_STEREO) {
        send_output = write_sb_u8stereo;
        pause_output = sb_silence_u8;
        resume_output = resume_output_nop;
        printf("Sound Blaster Pro or compatible (8 bit, stereo, %u Hz)\n", rate);
    } else {
        send_output = write_sb_u8mono;
        pause_output = sb_silence_u8;
        resume_output = resume_output_nop;
        printf("Sound Blaster %c or compatible (8 bit, mono, %u Hz)\n",
               (sb.dspver < SBVER_20)? '1' : '2', rate);
    }
    close_output = close_sb_output;

    return 0;
}

#elif defined(WILDMIDI_AMIGA) && defined(AUDIODRV_AHI)

/* Driver for output to native Amiga AHI device:
 * Written by Szilárd Biró <col.lawrence@gmail.com>, loosely based
 * on an old AOS4 version by Fredrik Wikstrom <fredrik@a500.org>
 */

#define BUFFERSIZE (4 << 10)

static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIReq[2] = { NULL, NULL };
static int active = 0;
static int8_t *AHIBuf[2] = { NULL, NULL };

#define open_audio_output open_ahi_output
static int write_ahi_output(int8_t *output_data, int output_size);
static void close_ahi_output(void);

static int open_ahi_output(void) {
    AHImp = CreateMsgPort();
    if (AHImp) {
        AHIReq[0] = (struct AHIRequest *) CreateIORequest(AHImp, sizeof(struct AHIRequest));
        if (AHIReq[0]) {
            AHIReq[0]->ahir_Version = 4;
            AHIReq[1] = (struct AHIRequest *) AllocVec(sizeof(struct AHIRequest), MEMF_PUBLIC);
            if (AHIReq[1]) {
                if (!OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)AHIReq[0], 0)) {
                    /*AHIReq[0]->ahir_Std.io_Message.mn_Node.ln_Pri = 0;*/
                    AHIReq[0]->ahir_Std.io_Command = CMD_WRITE;
                    AHIReq[0]->ahir_Std.io_Data = NULL;
                    AHIReq[0]->ahir_Std.io_Offset = 0;
                    AHIReq[0]->ahir_Frequency = rate;
                    AHIReq[0]->ahir_Type = AHIST_S16S;/* 16 bit stereo */
                    AHIReq[0]->ahir_Volume = 0x10000;
                    AHIReq[0]->ahir_Position = 0x8000;
                    CopyMem(AHIReq[0], AHIReq[1], sizeof(struct AHIRequest));

                    AHIBuf[0] = (int8_t *) AllocVec(BUFFERSIZE, MEMF_PUBLIC | MEMF_CLEAR);
                    if (AHIBuf[0]) {
                        AHIBuf[1] = (int8_t *) AllocVec(BUFFERSIZE, MEMF_PUBLIC | MEMF_CLEAR);
                        if (AHIBuf[1]) {
                            send_output = write_ahi_output;
                            close_output = close_ahi_output;
                            pause_output = pause_output_nop;
                            resume_output = resume_output_nop;
                            return (0);
                        }
                    }
                }
            }
        }
    }

    close_ahi_output();
    fprintf(stderr, "ERROR: Unable to open AHI output\r\n");
    return (-1);
}

static int write_ahi_output(int8_t *output_data, int output_size) {
    int chunk;
    while (output_size > 0) {
        if (AHIReq[active]->ahir_Std.io_Data) {
            WaitIO((struct IORequest *) AHIReq[active]);
        }
        chunk = (output_size < BUFFERSIZE)? output_size : BUFFERSIZE;
        memcpy(AHIBuf[active], output_data, chunk);
        output_size -= chunk;
        output_data += chunk;

        AHIReq[active]->ahir_Std.io_Data = AHIBuf[active];
        AHIReq[active]->ahir_Std.io_Length = chunk;
        AHIReq[active]->ahir_Link = !CheckIO((struct IORequest *) AHIReq[active ^ 1]) ? AHIReq[active ^ 1] : NULL;
        SendIO((struct IORequest *)AHIReq[active]);
        active ^= 1;
    }
    return (0);
}

static void close_ahi_output(void) {
    if (AHIReq[1]) {
        AHIReq[0]->ahir_Link = NULL; /* in case we are linked to req[0] */
        if (!CheckIO((struct IORequest *) AHIReq[1])) {
            AbortIO((struct IORequest *) AHIReq[1]);
            WaitIO((struct IORequest *) AHIReq[1]);
        }
        FreeVec(AHIReq[1]);
        AHIReq[1] = NULL;
    }
    if (AHIReq[0]) {
        if (!CheckIO((struct IORequest *) AHIReq[0])) {
            AbortIO((struct IORequest *) AHIReq[0]);
            WaitIO((struct IORequest *) AHIReq[0]);
        }
        if (AHIReq[0]->ahir_Std.io_Device) {
            CloseDevice((struct IORequest *) AHIReq[0]);
            AHIReq[0]->ahir_Std.io_Device = NULL;
        }
        DeleteIORequest((struct IORequest *) AHIReq[0]);
        AHIReq[0] = NULL;
    }
    if (AHImp) {
        DeleteMsgPort(AHImp);
        AHImp = NULL;
    }
    if (AHIBuf[0]) {
        FreeVec(AHIBuf[0]);
        AHIBuf[0] = NULL;
    }
    if (AHIBuf[1]) {
        FreeVec(AHIBuf[1]);
        AHIBuf[1] = NULL;
    }
}

#else
#ifdef AUDIODRV_ALSA

static int alsa_first_time = 1;
static snd_pcm_t *pcm = NULL;
static char pcmname[64];

#define open_audio_output open_alsa_output
static int write_alsa_output(int8_t *output_data, int output_size);
static void close_alsa_output(void);

static int open_alsa_output(void) {
    snd_pcm_hw_params_t *hw;
    snd_pcm_sw_params_t *sw;
    int err;
    unsigned int alsa_buffer_time;
    unsigned int alsa_period_time;
    unsigned int r;

    if (!pcmname[0]) {
        strcpy(pcmname, "default");
    }

    if ((err = snd_pcm_open(&pcm, pcmname, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Error: audio open error: %s\r\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_alloca(&hw);

    if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0) {
        fprintf(stderr, "ERROR: No configuration available for playback: %s\r\n",
                snd_strerror(err));
        goto fail;
    }

    if ((err = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "Cannot set access mode: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16) < 0) {
        fprintf(stderr, "ALSA does not support 16bit signed audio for your soundcard\r\n");
        goto fail;
    }

    if (snd_pcm_hw_params_set_channels(pcm, hw, 2) < 0) {
        fprintf(stderr, "ALSA does not support stereo for your soundcard\r\n");
        goto fail;
    }

    r = rate;
    if (snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0) < 0) {
        fprintf(stderr, "ALSA does not support %uHz for your soundcard\r\n", rate);
        goto fail;
    }
    if (r != rate) {
        fprintf(stderr, "ALSA: sample rate set to %uHz instead of %u\r\n", rate, r);
    }

    alsa_buffer_time = 500000;
    alsa_period_time = 50000;

    if ((err = snd_pcm_hw_params_set_buffer_time_near(pcm, hw, &alsa_buffer_time, 0)) < 0) {
        fprintf(stderr, "Set buffer time failed: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if ((err = snd_pcm_hw_params_set_period_time_near(pcm, hw, &alsa_period_time, 0)) < 0) {
        fprintf(stderr, "Set period time failed: %s.\r\n", snd_strerror(err));
        goto fail;
    }

    if (snd_pcm_hw_params(pcm, hw) < 0) {
        fprintf(stderr, "Unable to install hw params\r\n");
        goto fail;
    }

    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(pcm, sw);
    if (snd_pcm_sw_params(pcm, sw) < 0) {
        fprintf(stderr, "Unable to install sw params\r\n");
        goto fail;
    }

    send_output = write_alsa_output;
    close_output = close_alsa_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;
    return (0);

fail:   close_alsa_output();
    return -1;
}

static int write_alsa_output(int8_t *output_data, int output_size) {
    int err;
    snd_pcm_uframes_t frames;

    while (output_size > 0) {
        frames = snd_pcm_bytes_to_frames(pcm, output_size);
        if ((err = snd_pcm_writei(pcm, output_data, frames)) < 0) {
            if (snd_pcm_state(pcm) == SND_PCM_STATE_XRUN) {
                if ((err = snd_pcm_prepare(pcm)) < 0)
                    fprintf(stderr, "\nsnd_pcm_prepare() failed.\r\n");
                alsa_first_time = 1;
                continue;
            }
            return err;
        }

        output_size -= snd_pcm_frames_to_bytes(pcm, err);
        output_data += snd_pcm_frames_to_bytes(pcm, err);
        if (alsa_first_time) {
            alsa_first_time = 0;
            snd_pcm_start(pcm);
        }
    }
    return (0);
}

static void close_alsa_output(void) {
    if (!pcm) return;
    printf("Shutting down sound output\r\n");
    snd_pcm_close(pcm);
    pcm = NULL;
}

#elif defined AUDIODRV_OSS

#if !defined(AFMT_S16_NE)
#ifdef WORDS_BIGENDIAN
#define AFMT_S16_NE AFMT_S16_BE
#else
#define AFMT_S16_NE AFMT_S16_LE
#endif
#endif

#define DEFAULT_FRAGSIZE 14
#define DEFAULT_NUMFRAGS 16

static char pcmname[64];

#define open_audio_output open_oss_output
static int write_oss_output(int8_t *output_data, int output_size);
static void close_oss_output(void);

static void pause_output_oss(void) {
    ioctl(audio_fd, SNDCTL_DSP_POST, 0);
}

static int open_oss_output(void) {
    int tmp;
    unsigned int r;

    if (!pcmname[0]) {
        strcpy(pcmname, "/dev/dsp");
    }

    if ((audio_fd = open(pcmname, O_WRONLY)) < 0) {
        fprintf(stderr, "ERROR: Unable to open dsp (%s)\r\n", strerror(errno));
        return (-1);
    }
    if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) < 0) {
        fprintf(stderr, "ERROR: Unable to reset dsp\r\n");
        goto fail;
    }

    tmp = AFMT_S16_NE;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set 16bit sound format\r\n");
        goto fail;
    }

    tmp = 2;
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set stereo\r\n");
        goto fail;
    }

    r = rate;
    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate) < 0) {
        fprintf(stderr, "ERROR: Unable to set %uHz sample rate\r\n", rate);
        goto fail;
    }
    if (r != rate) {
        fprintf(stderr, "OSS: sample rate set to %uHz instead of %u\r\n", rate, r);
    }

    tmp = (DEFAULT_NUMFRAGS<<16)|DEFAULT_FRAGSIZE;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &tmp) < 0) {
        fprintf(stderr, "ERROR: Unable to set fragment size\r\n");
        goto fail;
    }

    send_output = write_oss_output;
    close_output = close_oss_output;
    pause_output = pause_output_oss;
    resume_output = resume_output_nop;
    return (0);

fail:   close_oss_output();
    return (-1);
}

static int write_oss_output(int8_t *output_data, int output_size) {
    int res = 0;
    while (output_size > 0) {
        res = write(audio_fd, output_data, output_size);
        if (res > 0) {
            output_size -= res;
            output_data += res;
        } else {
            fprintf(stderr, "\nOSS: write failure to dsp: %s.\r\n",
                    strerror(errno));
            return (-1);
        }
    }
    return (0);
}

static void close_oss_output(void) {
    if (audio_fd < 0)
        return;
    printf("Shutting down sound output\r\n");
    ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
    close(audio_fd);
    audio_fd = -1;
}

#elif defined AUDIODRV_OPENAL

#define NUM_BUFFERS 4

static ALCdevice *device;
static ALCcontext *context;
static ALuint sourceId = 0;
static ALuint buffers[NUM_BUFFERS];
static ALuint frames = 0;

#define open_audio_output open_openal_output

static void pause_output_openal(void) {
    alSourcePause(sourceId);
}

static int write_openal_output(int8_t *output_data, int output_size) {
    ALint processed, state;
    ALuint bufid;

    if (frames < NUM_BUFFERS) { /* initial state: fill the buffers */
        alBufferData(buffers[frames], AL_FORMAT_STEREO16, output_data,
                     output_size, rate);

        /* Now queue and start playback! */
        if (++frames == NUM_BUFFERS) {
            alSourceQueueBuffers(sourceId, frames, buffers);
            alSourcePlay(sourceId);
        }
        return 0;
    }

    /* Get relevant source info */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED) { /* resume it, then.. */
        alSourcePlay(sourceId);
        if (alGetError() != AL_NO_ERROR) {
            fprintf(stderr, "\nError restarting playback\r\n");
            return (-1);
        }
    }

    processed = 0;
    while (processed == 0) { /* Wait until we have a processed buffer */
        alGetSourcei(sourceId, AL_BUFFERS_PROCESSED, &processed);
    }

    /* Unqueue and handle each processed buffer */
    alSourceUnqueueBuffers(sourceId, 1, &bufid);

    /* Read the next chunk of data, refill the buffer, and queue it
     * back on the source */
    alBufferData(bufid, AL_FORMAT_STEREO16, output_data, output_size, rate);
    alSourceQueueBuffers(sourceId, 1, &bufid);
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr, "\nError buffering data\r\n");
        return (-1);
    }

    /* Make sure the source hasn't underrun */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    /*printf("STATE: %#08x - %d\n", state, queued);*/
    if (state != AL_PLAYING) {
        ALint queued;

        /* If no buffers are queued, playback is finished */
        alGetSourcei(sourceId, AL_BUFFERS_QUEUED, &queued);
        if (queued == 0) {
            fprintf(stderr, "\nNo buffers queued for playback\r\n");
            return (-1);
        }

        alSourcePlay(sourceId);
    }

    return (0);
}

static void close_openal_output(void) {
    if (!context) return;
    printf("Shutting down sound output\r\n");
    alSourceStop(sourceId);         /* stop playing */
    alSourcei(sourceId, AL_BUFFER, 0);  /* unload buffer from source */
    alDeleteBuffers(NUM_BUFFERS, buffers);
    alDeleteSources(1, &sourceId);
    alcDestroyContext(context);
    alcCloseDevice(device);
    context = NULL;
    device = NULL;
    frames = 0;
}

static int open_openal_output(void) {
    /* setup our audio devices and contexts */
    device = alcOpenDevice(NULL);
    if (!device) {
        fprintf(stderr, "OpenAL: Unable to open default device.\r\n");
        return (-1);
    }

    context = alcCreateContext(device, NULL);
    if (context == NULL || alcMakeContextCurrent(context) == ALC_FALSE) {
        if (context != NULL)
            alcDestroyContext(context);
        alcCloseDevice(device);
        context = NULL;
        device = NULL;
        fprintf(stderr, "OpenAL: Failed to create the default context.\r\n");
        return (-1);
    }

    /* setup our sources and buffers */
    alGenSources(1, &sourceId);
    alGenBuffers(NUM_BUFFERS, buffers);

    send_output = write_openal_output;
    close_output = close_openal_output;
    pause_output = pause_output_openal;
    resume_output = resume_output_nop;
    return (0);
}

#else /* no audio output driver compiled in: */

#define open_audio_output open_noaudio_output
static int open_noaudio_output(void) {
    fprintf(stderr, "No audio output driver was selected at compile time.\r\n");
    return -1;
}

#endif /* AUDIODRV_ALSA */
#endif /* _WIN32 || __CYGWIN__ */

static struct option const long_options[] = {
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { "rate", 1, 0, 'r' },
    { "mastervol", 1, 0, 'm' },
    { "config", 1, 0, 'c' },
    { "wavout", 1, 0, 'o' },
    { "tomidi", 1, 0, 'x' },
    { "convert", 1, 0, 'g' },
    { "frequency", 1, 0, 'f' },
    { "log_vol", 0, 0, 'l' },
    { "reverb", 0, 0, 'b' },
    { "test_midi", 0, 0, 't' },
    { "test_bank", 1, 0, 'k' },
    { "test_patch", 1, 0, 'p' },
    { "enhanced", 0, 0, 'e' },
#if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
    { "device", 1, 0, 'd' },
#endif
    { "roundtempo", 0, 0, 'n' },
    { "skipsilentstart", 0, 0, 's' },
    { "textaslyric", 0, 0, 'a' },
    { "playfrom", 1, 0, 'i'},
    { "playto", 1, 0, 'j'},
    { NULL, 0, NULL, 0 }
};

static void do_help(void) {
    printf("  -v    --version     Display version info and exit\n");
    printf("  -h    --help        Display this help and exit\n");
#if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
    printf("  -d D  --device=D    Use device D for audio output instead of default\n");
#endif
    printf("MIDI Options:\n");
    printf("  -n    --roundtempo  Round tempo to nearest whole number\n");
    printf("  -s    --skipsilentstart Skips any silence at the start of playback\n");
    printf("  -t    --test_midi   Listen to test MIDI\n");
    printf("Non-MIDI Options:\n");
    printf("  -x    --tomidi      Convert file to midi and save to file\n");
    printf("  -g    --convert     Convert XMI: 0 - No Conversion (default)\n");
    printf("                                   1 - MT32 to GM\n");
    printf("                                   2 - MT32 to GS\n");
    printf("  -f F  --frequency=F Use frequency F Hz for playback (MUS)\n");
    printf("Software Wavetable Options:\n");
    printf("  -o W  --wavout=W    Save output to W in 16bit stereo format wav file\n");
    printf("  -l    --log_vol     Use log volume adjustments\n");
    printf("  -r N  --rate=N      Set sample rate to N samples per second (Hz)\n");
    printf("  -c P  --config=P    Point to your wildmidi.cfg config file name/path\n");
    printf("                      defaults to: %s\n", WILDMIDI_CFG);
    printf("  -m V  --mastervol=V Set the master volume (0..127), default is 100\n");
    printf("  -b    --reverb      Enable final output reverb engine\n");
}

static void do_version(void) {
    printf("\nWildMidi %s Open Source Midi Sequencer\n", PACKAGE_VERSION);
    printf("Copyright (C) WildMIDI Developers 2001-2016\n\n");
    printf("WildMidi comes with ABSOLUTELY NO WARRANTY\n");
    printf("This is free software, and you are welcome to redistribute it under\n");
    printf("the terms and conditions of the GNU General Public License version 3.\n");
    printf("For more information see COPYING\n\n");
    printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
    printf("WildMIDI homepage is at %s\n\n", PACKAGE_URL);
}

static void do_syntax(void) {
    printf("Usage: wildmidi [options] filename.mid\n\n");
}

static char config_file[1024];

int main(int argc, char **argv) {
    struct _WM_Info *wm_info;
    int i, res;
    int option_index = 0;
    uint16_t mixer_options = 0;
    void *midi_ptr;
    uint8_t master_volume = 100;
    int8_t *output_buffer;
    uint32_t perc_play;
    uint32_t pro_mins;
    uint32_t pro_secs;
    uint32_t apr_mins;
    uint32_t apr_secs;
    char modes[5];
    uint8_t ch;
    int test_midi = 0;
    int test_count = 0;
    uint8_t *test_data;
    uint8_t test_bank = 0;
    uint8_t test_patch = 0;
    static char spinner[] = "|/-\\";
    static int spinpoint = 0;
    unsigned long int seek_to_sample;
    uint32_t samples = 0;
    int inpause = 0;
    char * ret_err = NULL;
    long libraryver;
    char * lyric = NULL;
    char *last_lyric = NULL;
    size_t last_lyric_length = 0;
    int8_t kareoke = 0;
#define MAX_LYRIC_CHAR 128
    char lyrics[MAX_LYRIC_CHAR + 1];
#define MAX_DISPLAY_LYRICS 29
    char display_lyrics[MAX_DISPLAY_LYRICS + 1];

    unsigned long int play_from = 0;
    unsigned long int play_to = 0;

    memset(lyrics,' ',MAX_LYRIC_CHAR);
    memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

#if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
    pcmname[0] = 0;
#endif
    config_file[0] = 0;
    wav_file[0] = 0;
    midi_file[0] = 0;

    do_version();
    while (1) {
        i = getopt_long(argc, argv, "0vho:tx:g:f:lr:c:m:btak:p:ed:nsi:j:", long_options,
                &option_index);
        if (i == -1)
            break;
        switch (i) {
        case 'v': /* Version */
            return (0);
        case 'h': /* help */
            do_syntax();
            do_help();
            return (0);
        case 'r': /* Sample Rate */
            res = atoi(optarg);
            if (res < 0 || res > 65535) {
                fprintf(stderr, "Error: bad rate %i.\n", res);
                return (1);
            }
            rate = (uint32_t) res;
            break;
        case 'b': /* Reverb */
            mixer_options |= WM_MO_REVERB;
            break;
        case 'm': /* Master Volume */
            master_volume = (uint8_t) atoi(optarg);
            break;
        case 'o': /* Wav Output */
            if (!*optarg) {
                fprintf(stderr, "Error: empty wavfile name.\n");
                return (1);
            }
            strncpy(wav_file, optarg, sizeof(wav_file));
            wav_file[sizeof(wav_file) - 1] = 0;
            break;
        case 'g': /* XMIDI Conversion */
            WildMidi_SetCvtOption(WM_CO_XMI_TYPE, (uint16_t) atoi(optarg));
            break;
        case 'f': /* MIDI-like Conversion */
            WildMidi_SetCvtOption(WM_CO_FREQUENCY, (uint16_t) atoi(optarg));
            break;
        case 'x': /* MIDI Output */
            if (!*optarg) {
                fprintf(stderr, "Error: empty midi name.\n");
                return (1);
            }
            strncpy(midi_file, optarg, sizeof(midi_file));
            midi_file[sizeof(midi_file) - 1] = 0;
            break;
        case 'c': /* Config File */
            if (!*optarg) {
                fprintf(stderr, "Error: empty config name.\n");
                return (1);
            }
            strncpy(config_file, optarg, sizeof(config_file));
            config_file[sizeof(config_file) - 1] = 0;
            break;
#if defined(AUDIODRV_OSS) || defined(AUDIODRV_ALSA)
        case 'd': /* Output device */
            if (!*optarg) {
                fprintf(stderr, "Error: empty device name.\n");
                return (1);
            }
            strncpy(pcmname, optarg, sizeof(pcmname));
            pcmname[sizeof(pcmname) - 1] = 0;
            break;
#endif
        case 'e': /* Enhanced Resampling */
            mixer_options |= WM_MO_ENHANCED_RESAMPLING;
            break;
        case 'l': /* log volume */
            mixer_options |= WM_MO_LOG_VOLUME;
            break;
        case 't': /* play test midis */
            test_midi = 1;
            break;
        case 'k': /* set test bank */
            test_bank = (uint8_t) atoi(optarg);
            break;
        case 'p': /* set test patch */
            test_patch = (uint8_t) atoi(optarg);
            break;
        case 'n': /* whole number tempo */
            mixer_options |= WM_MO_ROUNDTEMPO;
            break;
        case 'a':
            /* Some files have the lyrics in the text meta event.
             * This option reads lyrics from there instead.  */
            mixer_options |= WM_MO_TEXTASLYRIC;
            break;
        case 's': /* strip silence at start */
            mixer_options |= WM_MO_STRIPSILENCE;
            break;
        case '0': /* treat as type 2 midi when writing to file */
            mixer_options |= WM_MO_SAVEASTYPE0;
            break;
        case 'i':
            play_from = (unsigned long int)(atof(optarg) * (double)rate);
            break;
        case 'j':
            play_to = (unsigned long int)(atof(optarg) * (double)rate);
            break;
        default:
            do_syntax();
            return (1);
        }
    }

    if (optind >= argc && !test_midi) {
        fprintf(stderr, "ERROR: No midi file given\r\n");
        do_syntax();
        return (1);
    }

    if (test_midi) {
        if (midi_file[0] != '\0') {
            fprintf(stderr, "--test_midi and --convert cannot be used together.\n");
            return (1);
        }
    }

    /* check if we only need to convert a file to midi */
    if (midi_file[0] != '\0') {
        const char *real_file = FIND_LAST_DIRSEP(argv[optind]);
        uint32_t size;
        uint8_t *data;

        if (!real_file) real_file = argv[optind];
        else real_file++;

        printf("Converting %s\r\n", real_file);
        if (WildMidi_ConvertToMidi(argv[optind], &data, &size) < 0) {
            fprintf(stderr, "Conversion failed: %s.\r\n", WildMidi_GetError());
            WildMidi_ClearError();
            return (1);
        }

        printf("Writing %s: %u bytes.\r\n", midi_file, size);
        write_midi_output(data, size);
        free(data);
        return (0);
    }

    if (!config_file[0]) {
        strncpy(config_file, WILDMIDI_CFG, sizeof(config_file));
        config_file[sizeof(config_file) - 1] = 0;
    }

    printf("Initializing Sound System\n");
    if (wav_file[0] != '\0') {
        if (open_wav_output() == -1) {
            return (1);
        }
    } else {
        if (open_audio_output() == -1) {
            return (1);
        }
    }

    libraryver = WildMidi_GetVersion();
    printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
                        (libraryver>>16) & 255,
                        (libraryver>> 8) & 255,
                        (libraryver    ) & 255);
    if (WildMidi_Init(config_file, rate, mixer_options) == -1) {
        fprintf(stderr, "%s\r\n", WildMidi_GetError());
        WildMidi_ClearError();
        return (1);
    }

    printf(" +  Volume up        e  Better resampling    n  Next Midi\n");
    printf(" -  Volume down      l  Log volume           q  Quit\n");
    printf(" ,  1sec Seek Back   r  Reverb               .  1sec Seek Forward\n");
    printf(" m  save as midi     p  Pause On/Off\n\n");

    output_buffer = (int8_t *) malloc(16384);
    if (output_buffer == NULL) {
        fprintf(stderr, "Not enough memory, exiting\n");
        WildMidi_Shutdown();
        return (1);
    }

    wm_inittty();
#ifdef WILDMIDI_AMIGA
    amiga_sysinit();
#endif

    WildMidi_MasterVolume(master_volume);

    while (optind < argc || test_midi) {
        WildMidi_ClearError();
        if (!test_midi) {
            const char *real_file = FIND_LAST_DIRSEP(argv[optind]);

            if (!real_file) real_file = argv[optind];
            else real_file++;
            printf("\rPlaying %s ", real_file);

            midi_ptr = WildMidi_Open(argv[optind]);
            optind++;
            if (midi_ptr == NULL) {
                ret_err = WildMidi_GetError();
                printf(" Skipping: %s\r\n",ret_err);
                continue;
            }
        } else {
            if (test_count == midi_test_max) {
                break;
            }
            test_data = (uint8_t *) malloc(midi_test[test_count].size);
            memcpy(test_data, midi_test[test_count].data,
                    midi_test[test_count].size);
            test_data[25] = test_bank;
            test_data[28] = test_patch;
            midi_ptr = WildMidi_OpenBuffer(test_data, 633);
            test_count++;
            if (midi_ptr == NULL) {
                fprintf(stderr, "\rFailed loading test midi no. %i\r\n", test_count);
                continue;
            }
            printf("\rPlaying test midi no. %i ", test_count);
        }

        wm_info = WildMidi_GetInfo(midi_ptr);

        apr_mins = wm_info->approx_total_samples / (rate * 60);
        apr_secs = (wm_info->approx_total_samples % (rate * 60)) / rate;
        mixer_options = wm_info->mixer_options;
        modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
        modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
        modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
        modes[3] = ' ';
        modes[4] = '\0';

        printf("\r\n[Approx %2um %2us Total]\r\n", apr_mins, apr_secs);
        fprintf(stderr, "\r");

        memset(lyrics,' ',MAX_LYRIC_CHAR);
        memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

        if (play_from != 0) {
            WildMidi_FastSeek(midi_ptr, &play_from);
            if (play_to < play_from) {
                // Ignore --playto if set less than --playfrom
                play_to = 0;
            }
        }

        while (1) {
            ch = 0;
#ifdef _WIN32
            if (_kbhit()) {
                ch = _getch();
                _putch(ch);
            }
#elif defined(__DJGPP__) || defined(__OS2__) || defined(__EMX__)
            if (kbhit()) {
                ch = getch();
                putch(ch);
            }
#elif defined(WILDMIDI_AMIGA)
            amiga_getch (&ch);
#else
            if (read(STDIN_FILENO, &ch, 1) != 1)
                ch = 0;
#endif
            if (ch) {
                switch (ch) {
                case 'l':
                    WildMidi_SetOption(midi_ptr, WM_MO_LOG_VOLUME,
                                       ((mixer_options & WM_MO_LOG_VOLUME) ^ WM_MO_LOG_VOLUME));
                    mixer_options ^= WM_MO_LOG_VOLUME;
                    modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
                    break;
                case 'r':
                    WildMidi_SetOption(midi_ptr, WM_MO_REVERB,
                                       ((mixer_options & WM_MO_REVERB) ^ WM_MO_REVERB));
                    mixer_options ^= WM_MO_REVERB;
                    modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
                    break;
                case 'e':
                    WildMidi_SetOption(midi_ptr, WM_MO_ENHANCED_RESAMPLING,
                                       ((mixer_options & WM_MO_ENHANCED_RESAMPLING) ^ WM_MO_ENHANCED_RESAMPLING));
                    mixer_options ^= WM_MO_ENHANCED_RESAMPLING;
                    modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
                    break;
                case 'a':
                    WildMidi_SetOption(midi_ptr, WM_MO_TEXTASLYRIC,
                                       ((mixer_options & WM_MO_TEXTASLYRIC) ^ WM_MO_TEXTASLYRIC));
                    mixer_options ^= WM_MO_TEXTASLYRIC;
                    break;
                case 'n':
                    goto NEXTMIDI;
                case 'p':
                    if (inpause) {
                        inpause = 0;
                        fprintf(stderr, "       \r");
                        resume_output();
                    } else {
                        inpause = 1;
                        fprintf(stderr, "Paused \r");
                        pause_output();
                        continue;
                    }
                    break;
                case 'q':
                    printf("\r\n");
                    if (inpause) goto end2;
                    goto end1;
                case '-':
                    if (master_volume > 0) {
                        master_volume--;
                        WildMidi_MasterVolume(master_volume);
                    }
                    break;
                case '+':
                    if (master_volume < 127) {
                        master_volume++;
                        WildMidi_MasterVolume(master_volume);
                    }
                    break;
                case ',': /* fast seek backwards */
                    if (wm_info->current_sample < rate) {
                        seek_to_sample = 0;
                    } else {
                        seek_to_sample = wm_info->current_sample - rate;
                    }
                    WildMidi_FastSeek(midi_ptr, &seek_to_sample);
                    break;
                case '.': /* fast seek forwards */
                    if ((wm_info->approx_total_samples
                            - wm_info->current_sample) < rate) {
                        seek_to_sample = wm_info->approx_total_samples;
                    } else {
                        seek_to_sample = wm_info->current_sample + rate;
                    }
                    WildMidi_FastSeek(midi_ptr, &seek_to_sample);
                    break;
                case '<':
                    WildMidi_SongSeek (midi_ptr, -1);
                    break;
                case '>':
                    WildMidi_SongSeek (midi_ptr, 1);
                    break;
                case '/':
                    WildMidi_SongSeek (midi_ptr, 0);
                    break;
                case 'm': /* save as midi */ {
                    int8_t *getmidibuffer = NULL;
                    uint32_t getmidisize = 0;
                    int32_t getmidiret = 0;

                    getmidiret = WildMidi_GetMidiOutput(midi_ptr, &getmidibuffer, &getmidisize);
                    if (getmidiret == -1) {
                        fprintf(stderr, "\r\n\nFAILED to convert events to midi\r\n");
                        ret_err = WildMidi_GetError();
                        fprintf(stderr, "%s\r\n",ret_err);
                        WildMidi_ClearError();
                    } else {
                        char *real_file = FIND_LAST_DIRSEP(argv[optind-1]);
                        if (!real_file) real_file = argv[optind];
                        else real_file++;

                        strncpy(midi_file, real_file, strlen(real_file));
                        midi_file[strlen(real_file)-4] = '.';
                        midi_file[strlen(real_file)-3] = 'm';
                        midi_file[strlen(real_file)-2] = 'i';
                        midi_file[strlen(real_file)-1] = 'd';

                        printf("\rWriting %s: %u bytes.\r\n", midi_file, getmidisize);
                        write_midi_output(getmidibuffer,getmidisize);
                        free(getmidibuffer);
                    }
                  } break;
                case 'k': /* Kareoke */
                          /* Enables/Disables the display of lyrics */
                    kareoke ^= 1;
                    break;
                default:
                    break;
                }
            }

            if (inpause) {
                wm_info = WildMidi_GetInfo(midi_ptr);
                perc_play = (wm_info->current_sample * 100)
                            / wm_info->approx_total_samples;
                pro_mins = wm_info->current_sample / (rate * 60);
                pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
                fprintf(stderr,
                        "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] P  \r",
                        display_lyrics, modes, (int)master_volume, pro_mins,
                        pro_secs, perc_play);
                msleep(5);
                continue;
            }

            if (play_to != 0) {
                if ((wm_info->current_sample + 4096) <= play_to) {
                    samples = 16384;
                } else {
                    samples = (play_to - wm_info->current_sample) << 2;
                    if (!samples) {
                        // We are at or past where we wanted to play to
                        break;
                    }
                }
            }
            else {
                samples = 16384;
            }
            res = WildMidi_GetOutput(midi_ptr, output_buffer, samples);

            if (res <= 0)
                break;

            wm_info = WildMidi_GetInfo(midi_ptr);
            lyric = WildMidi_GetLyric(midi_ptr);

            memmove(lyrics, &lyrics[1], MAX_LYRIC_CHAR - 1);
            lyrics[MAX_LYRIC_CHAR - 1] = ' ';

            if ((lyric != NULL) && (lyric != last_lyric) && (kareoke)) {
                last_lyric = lyric;
                if (last_lyric_length != 0) {
                    memcpy(lyrics, &lyrics[last_lyric_length], MAX_LYRIC_CHAR - last_lyric_length);
                }
                memcpy(&lyrics[MAX_DISPLAY_LYRICS], lyric, strlen(lyric));
                last_lyric_length = strlen(lyric);
            } else {
                if (last_lyric_length != 0) last_lyric_length--;
            }

            memcpy(display_lyrics,lyrics,MAX_DISPLAY_LYRICS);
            display_lyrics[MAX_DISPLAY_LYRICS] = '\0';

            perc_play = (wm_info->current_sample * 100)
                        / wm_info->approx_total_samples;
            pro_mins = wm_info->current_sample / (rate * 60);
            pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
            fprintf(stderr,
                "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] %c  \r",
                display_lyrics, modes, (int)master_volume, pro_mins,
                pro_secs, perc_play, spinner[spinpoint++ % 4]);

            if (send_output(output_buffer, res) < 0) {
            /* driver prints an error message already. */
                printf("\r");
                goto end2;
            }
        }
        NEXTMIDI: fprintf(stderr, "\r\n");
        if (WildMidi_Close(midi_ptr) == -1) {
            ret_err = WildMidi_GetError();
            fprintf(stderr, "OOPS: failed closing midi handle!\r\n%s\r\n",ret_err);
        }
        memset(output_buffer, 0, 16384);
        send_output(output_buffer, 16384);
    }
end1: memset(output_buffer, 0, 16384);
    send_output(output_buffer, 16384);
    msleep(5);
end2: close_output();
    free(output_buffer);
    if (WildMidi_Shutdown() == -1) {
        ret_err = WildMidi_GetError();
        fprintf(stderr, "OOPS: failure shutting down libWildMidi\r\n%s\r\n", ret_err);
        WildMidi_ClearError();
    }
    wm_resetty();

    printf("\r\n");
    return (0);
}

/* helper / replacement functions: */

#if !(defined(_WIN32) || defined(__DJGPP__) || defined(WILDMIDI_AMIGA) || defined(__OS2__) || defined(__EMX__))
static int msleep(unsigned long milisec) {
    struct timespec req = { 0, 0 };
    time_t sec = (int) (milisec / 1000);
    milisec = milisec - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    while (nanosleep(&req, &req) == -1)
        continue;
    return (1);
}
#endif
