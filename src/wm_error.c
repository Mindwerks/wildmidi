/*
 * wm_error.c -- error reporting
 *
 * Copyright (C) WildMIDI Developers  2001-2016
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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "wm_error.h"

void _WM_DEBUG_MSG(const char * wmfmt, ...) {
    va_list args;
    fprintf(stderr, "\r");
    va_start(args, wmfmt);
    vfprintf(stderr, wmfmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static const char *errors[WM_ERR_MAX+1] = {
    "No Error",

    "Unable to obtain memory",
    "Unable to stat",
    "Unable to load",
    "Unable to open",
    "Unable to read",
    "Invalid or Unsupported file format",
    "File corrupt",
    "Library not Initialized",
    "Invalid argument",
    "Library Already Initialized",
    "Not a midi file",
    "Refusing to load unusually long file",
    "Not an hmp file",
    "Not an hmi file",
    "Unable to convert",
    "Not a mus file",
    "Not an xmi file",

    "Invalid error code"
};

#define MAX_ERROR_LEN 255

char * _WM_Global_ErrorS = NULL;
int _WM_Global_ErrorI = 0;

void _WM_GLOBAL_ERROR(const char *func, int lne, int wmerno, const char *wmfor, int error) {

    char *errorstring;

    if (wmerno < 0 || wmerno >= WM_ERR_MAX)
         wmerno = WM_ERR_MAX; /* set to invalid error code. */

    _WM_Global_ErrorI = wmerno;

    if (_WM_Global_ErrorS != NULL) free(_WM_Global_ErrorS);

    errorstring = malloc(MAX_ERROR_LEN+1);

    if (error == 0) {
        if (wmfor == NULL) {
            sprintf(errorstring,"Error (%s:%i) %s",
                    func, lne, errors[wmerno]);
        } else {
            sprintf(errorstring,"Error (%s:%i) %s (%s)",
                    func, lne, wmfor, errors[wmerno]);
        }
    } else {
        if (wmfor == NULL) {
            sprintf(errorstring,"System Error (%s:%i) %s : %s",
                    func, lne, errors[wmerno], strerror(error));
        } else {
            sprintf(errorstring,"System Error (%s:%i) %s (%s) : %s",
                    func, lne, wmfor, errors[wmerno], strerror(error));
        }
    }

    errorstring[MAX_ERROR_LEN] = 0;
    _WM_Global_ErrorS = errorstring;
}

void _WM_ERROR_NEW(const char * wmfmt, ...) {
    char *errorstring;
    va_list args;
    va_start(args, wmfmt);
    errorstring = malloc(MAX_ERROR_LEN+1);
    vsprintf(errorstring, wmfmt, args);
    va_end(args);
    errorstring[MAX_ERROR_LEN] = 0;
    _WM_Global_ErrorS = errorstring;
    _WM_Global_ErrorI = WM_ERR_MAX;/* well, it's a custom error message */
}
