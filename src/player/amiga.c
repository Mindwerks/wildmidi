/* amiga-specific stuff for WildMIDI player
 * Copyright (C) WildMIDI Developers 2016
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

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include <stdio.h>
#include <stdlib.h>

struct timerequest	*timerio;
struct MsgPort		*timerport;
#if defined(__MORPHOS__) || defined(__VBCC__)
struct Library		*TimerBase;
#else
struct Device		*TimerBase;
#endif

static BPTR amiga_stdin, amiga_stdout;
#define MODE_RAW 1
#define MODE_NORMAL 0

static void amiga_atexit (void) {
    if (amiga_stdin)
        SetMode(amiga_stdin, MODE_NORMAL);
    if (TimerBase) {
        WaitIO((struct IORequest *) timerio);
        CloseDevice((struct IORequest *) timerio);
        DeleteIORequest((struct IORequest *) timerio);
        DeleteMsgPort(timerport);
        TimerBase = NULL;
    }
}

void amiga_sysinit (void) {
    if ((timerport = CreateMsgPort())) {
        if ((timerio = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest)))) {
            if (OpenDevice((STRPTR) TIMERNAME, UNIT_MICROHZ, (struct IORequest *) timerio, 0) == 0) {
#if defined(__MORPHOS__) || defined(__VBCC__)
                TimerBase = (struct Library *)timerio->tr_node.io_Device;
#else
                TimerBase = timerio->tr_node.io_Device;
#endif
            }
            else {
                DeleteIORequest((struct IORequest *)timerio);
                DeleteMsgPort(timerport);
            }
        }
        else {
            DeleteMsgPort(timerport);
        }
    }
    if (!TimerBase) {
        fprintf(stderr, "Can't open timer.device\n");
        exit (-1);
    }

    /* 1us wait, for timer cleanup success */
    timerio->tr_node.io_Command = TR_ADDREQUEST;
    timerio->tr_time.tv_secs = 0;
    timerio->tr_time.tv_micro = 1;
    SendIO((struct IORequest *) timerio);
    WaitIO((struct IORequest *) timerio);

    amiga_stdout = Output();
    amiga_stdin = Input();
    SetMode(amiga_stdin, MODE_RAW);

    atexit (amiga_atexit);
}

int amiga_getch (unsigned char *c) {
    if (WaitForChar(amiga_stdin,10)) {
        return Read (amiga_stdin, c, 1);
    }
    return 0;
}

void amiga_usleep(unsigned long timeout) {
    timerio->tr_node.io_Command = TR_ADDREQUEST;
    timerio->tr_time.tv_secs = timeout / 1000000;
    timerio->tr_time.tv_micro = timeout % 1000000;
    SendIO((struct IORequest *) timerio);
    WaitIO((struct IORequest *) timerio);
}
