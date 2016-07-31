/*
 * wm_error.h -- error reporting
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

#ifndef __WM_ERROR_H
#define __WM_ERROR_H

enum {
    WM_ERR_NONE = 0,
    WM_ERR_MEM,
    WM_ERR_STAT,
    WM_ERR_LOAD,
    WM_ERR_OPEN,
    WM_ERR_READ,
    WM_ERR_INVALID,
    WM_ERR_CORUPT,
    WM_ERR_NOT_INIT,
    WM_ERR_INVALID_ARG,
    WM_ERR_ALR_INIT,
    WM_ERR_NOT_MIDI,
    WM_ERR_LONGFIL,
    WM_ERR_NOT_HMP,
    WM_ERR_NOT_HMI,
    WM_ERR_CONVERT,
    WM_ERR_NOT_MUS,
    WM_ERR_NOT_XMI,

    WM_ERR_MAX
};

extern char * _WM_Global_ErrorS;
extern int _WM_Global_ErrorI;

extern void _WM_GLOBAL_ERROR(const char *func, int lne, int wmerno, const char * wmfor, int error);

/* sets the global error string to a custom msg */
extern void _WM_ERROR_NEW(const char * wmfmt, ...)
#ifdef __GNUC__
        __attribute__((format(printf, 1, 2)))
#endif
        ;

/* prints a debug message to stderr */
extern void _WM_DEBUG_MSG(const char * wmfmt, ...)
#ifdef __GNUC__
        __attribute__((format(printf, 1, 2)))
#endif
        ;

#endif /* __WM_ERROR_H */
