/* Copyright (C) WildMIDI Developers 2016-2024.
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

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
void msleep(uint32_t msec) {
    Sleep(msec);
}

#elif defined(__OS2__)||defined(__EMX__)
#define INCL_DOSPROCESS
#include <os2.h>
void msleep(uint32_t msec) {
    DosSleep(msec);
}

#elif defined(__WATCOMC__) && defined(_DOS)
#include <dos.h>
void msleep(uint32_t msec) {
    delay(msec); /* doesn't seem to use int 15h. */
}

#elif defined(WILDMIDI_AMIGA)
#include "wildplay.h"			/* for the amiga_xxx prototypes. */
void msleep(uint32_t msec) {
    amiga_usleep(msec * 1000);
}

#else /* DJGPP, POSIX... */
#include <unistd.h>
void msleep(uint32_t msec) {
    usleep(msec * 1000);
}
#endif
