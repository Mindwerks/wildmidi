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
#include "wm_tty.h"

#if defined(_WIN32)
#include <conio.h>
#ifdef __WATCOMC__
#define _putch putch
#endif
void wm_getch(unsigned char *c) {
    if (_kbhit()) {
        *c = _getch();
        _putch(*c);
    } else  *c = 0;
}
void wm_inittty(void) {}
void wm_resetty(void) {}

#elif defined(__DJGPP__) || defined(__OS2__) || defined(__EMX__)
#ifdef __EMX__
#define INCL_DOS
#define INCL_KBD
#define INCL_VIO
#include <os2.h>
#include <stdlib.h>
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
#endif /* EMX */
#include <conio.h>
void wm_getch(unsigned char *c) {
    if (kbhit()) {
        *c = getch();
        putch(*c);
    } else  *c = 0;
}
void wm_inittty(void) {}
void wm_resetty(void) {}

#elif defined(WILDMIDI_AMIGA)
#include "wildplay.h"			/* for the amiga_xxx prototypes. */
void wm_getch(unsigned char *c) {
    if (amiga_getch (c) <= 0)
        *c = 0;
}
void wm_inittty(void) {}
void wm_resetty(void) {}

#else /* unix case: */

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
static int _res_block = 0;

void wm_inittty(void) {
    if (!isatty(STDIN_FILENO))
        return;

    /* save tty: */
    tcgetattr(STDIN_FILENO, &_tty);
    _res_oflg = _tty.c_oflag;
    _res_lflg = _tty.c_lflag;
    _res_block=fcntl(STDIN_FILENO, F_GETFL, FNONBLOCK);

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

    fcntl(STDIN_FILENO, F_SETFL, _res_block);
}

void wm_getch(unsigned char *c) {
    if (read(STDIN_FILENO, c, 1) != 1)
        *c = 0;
}
#endif
