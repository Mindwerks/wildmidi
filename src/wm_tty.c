/*
 * wm_tty.c - unix termios code for player
 *
 * Copyright (C) Chris Ison 2001-2011
 * Copyright (C) Bret Curtis 2013-2016
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

#if !(defined(_WIN32) || defined(__DJGPP__) || defined(WILDMIDI_AMIGA) || defined(__OS2__) || defined(__EMX__))

#define _XOPEN_SOURCE 600 /* for ONLCR */
#define __BSD_VISIBLE 1 /* for ONLCR in *BSD */

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#ifndef FNONBLOCK
#define FNONBLOCK O_NONBLOCK
#endif

struct termios _tty;
static tcflag_t _res_oflg = 0;
static tcflag_t _res_lflg = 0;

void wm_inittty(void) {
    if (!isatty(STDIN_FILENO))
        return;

    /* save tty: */
    tcgetattr(STDIN_FILENO, &_tty);
    _res_oflg = _tty.c_oflag;
    _res_lflg = _tty.c_lflag;

    /* set raw: */
    _tty.c_lflag &= ~(ICANON | ICRNL | ISIG);
    _tty.c_oflag &= ~ONLCR;
    tcsetattr(STDIN_FILENO, TCSANOW, &_tty);

    fcntl(STDIN_FILENO, F_SETFL, FNONBLOCK);
}

void wm_resetty(void) {
    if (!isatty(STDIN_FILENO))
        return;

    /* reset tty: */
    _tty.c_oflag = _res_oflg;
    _tty.c_lflag = _res_lflg;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &_tty);
}
#endif /* !(_WIN32,__DJGPP__,__OS2__) */
