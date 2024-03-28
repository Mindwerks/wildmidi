/* wm_tty.h - unix termios code for player
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

#ifndef wm_tty_h
#define wm_tty_h

void wm_inittty(void);
void wm_resetty(void);

#if defined(_WIN32)||defined(__DJGPP__)||defined(WILDMIDI_AMIGA)||defined(__OS2__)||defined(__EMX__)
#define wm_inittty() do {} while (0)
#define wm_resetty() do {} while (0)
#endif /* _WIN32, __DJGPP__, __OS2__ */

#endif /* wm_tty_h */
