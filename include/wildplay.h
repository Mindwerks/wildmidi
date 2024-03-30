/*
 * wildplay.h -- WildMidi player header
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

#ifndef WILDPLAY_H
#define WILDPLAY_H

#include <stdint.h>

/* Macros to suppress unused variable warnings */
#ifndef __VBCC__
#define WMPLAY_UNUSED(x) (void)(x)
#else
#define WMPLAY_UNUSED(x) /* vbcc emits an annoying warning for (void)(x) */
#endif

/* Macros to suppress -Wimplicit-fallthrough warnings */
/* The following taken from SDL: */
#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000L)
#define WMPLAY_FALLTHROUGH [[fallthrough]]
#else  /**/
#ifdef __has_attribute
#define WMPLAY_HAS_FALLTHROUGH __has_attribute(__fallthrough__)
#else
#define WMPLAY_HAS_FALLTHROUGH 0
#endif /* __has_attribute */
#if WMPLAY_HAS_FALLTHROUGH && \
   ((defined(__GNUC__) && __GNUC__ >= 7) || \
    (defined(__clang_major__) && __clang_major__ >= 10))
#define WMPLAY_FALLTHROUGH __attribute__((__fallthrough__))
#else
#define WMPLAY_FALLTHROUGH do {} while (0) /* fallthrough */
#endif
#undef WMPLAY_HAS_FALLTHROUGH
#endif /**/

typedef struct {
    const char *name;
    const char *description;
    int (* open_out)(const char *);
    int (* send_out)(int8_t *, int);
    void (* close_out)(void);
    void (* pause_out)(void);
    void (* resume_out)(void);
} audiodrv_info;

extern audiodrv_info audiodrv_none;
extern audiodrv_info audiodrv_wave;
extern audiodrv_info audiodrv_coreaudio;
extern audiodrv_info audiodrv_alsa;
extern audiodrv_info audiodrv_sndio;
extern audiodrv_info audiodrv_oss;
extern audiodrv_info audiodrv_ahi;
extern audiodrv_info audiodrv_winmm;
extern audiodrv_info audiodrv_dart;
extern audiodrv_info audiodrv_openal;

extern void msleep(uint32_t msec);

#if defined(WILDMIDI_AMIGA)
extern void amiga_sysinit (void);
extern int amiga_getch (unsigned char *ch);
extern void amiga_usleep (uint32_t usec);
#endif

#endif /* WILDPLAY_H */
