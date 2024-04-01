/*
 * out_sndio.c -- OpenBSD sndio output
 * Copyright (C) WildMidi Developers 2024
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
 *
 * This was based on sndio modules of XMP and MikMod players:
 * Copyright (c) 2009 Thomas Pfaff <tpfaff@tp76.info>
 * Copyright (c) 2009 Jacob Meuser <jakemsr@sdf.lonestar.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#ifdef AUDIODRV_SNDIO

#include <stdio.h>
#include <string.h>
#include <sndio.h>

#include "wildplay.h"

static struct sio_hdl * hdl;
#define DEFAULT_FRAGSIZE 12
static int fragsize = 1 << DEFAULT_FRAGSIZE;

static int open_sndio_output(const char *output, unsigned int *rate)
{
    struct sio_par par;

    WMPLAY_UNUSED(output);

    hdl = sio_open(NULL, SIO_PLAY, 0);
    if (hdl == NULL) {
        fprintf(stderr, "sndio: failed to open audio device\n");
        return -1;
    }

    /* Setup for signed 16 bit output: */
    sio_initpar(&par);
    par.pchan = 2;
    par.rate = *rate;
    par.le = SIO_LE_NATIVE;
    par.bits = 16;
    par.sig = 1;
    par.appbufsz = 4 * fragsize / SIO_BPS(par.bits) / par.pchan;	/* par.rate / 4 */

    if (!sio_setpar(hdl, &par) || !sio_getpar(hdl, &par)) {
        fprintf(stderr, "sndio: failed to set parameters\n");
        goto error;
    }

    if (par.bits != 16 || par.le != SIO_LE_NATIVE || par.sig != 1 || par.pchan != 2) {
        fprintf(stderr, "sndio: parameters not supported\n");
        goto error;
    }

    if (!sio_start(hdl)) {
        fprintf(stderr, "sndio: failed to start audio device\n");
        goto error;
    }

    *rate = par.rate; /* update rate with the received value */
    return 0;

  error:
    sio_close(hdl);
    return -1;
}

static int write_sndio_output(void *buf, int len)
{
    sio_write(hdl, buf, len);
    return 0;
}

static void close_sndio_output(void)
{
    if (!hdl) return;
    sio_close(hdl);
    hdl = NULL;
}

static void pause_sndio_output(void) {}
static void resume_sndio_output(void) {}

audiodrv_info audiodrv_sndio = {
    "sndio",
    "OpenBSD sndio output",
    open_sndio_output,
    write_sndio_output,
    close_sndio_output,
    pause_sndio_output,
    resume_sndio_output
};

#endif /* AUDIODRV_SNDIO */
