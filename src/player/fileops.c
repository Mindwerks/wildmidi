/*
 * fileops.c -- Platform specific file operations
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

#include "config.h"
#include "fileops.h"

#if defined(_WIN32)

#elif defined(__DJGPP__)

#elif defined(__OS2__) || defined(__EMX__)

#elif defined(WILDMIDI_AMIGA)

#else /* common posix case */
int msleep(unsigned long millisec) {
  struct timespec req = { 0, 0 };
  time_t sec = (int) (millisec / 1000);
  millisec = millisec - (sec * 1000);
  req.tv_sec = sec;
  req.tv_nsec = millisec * 1000000L;
  while (nanosleep(&req, &req) == -1)
    continue;
  return (1);
}

#endif
