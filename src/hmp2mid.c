/*
 * HMP2MIDI: HMI Sound Operating System HMP to MIDI Library
 *
 * Copyright (C) WildMIDI Developers 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/* HMP Converter
 *
 * Format info comes from f_hmp.c: an HMP file is a set of chunks (tracks)
 * that play simultaneously, so we emit a type-1 MIDI file.  Each chunk is
 * a stream of standard MIDI events, but the delta times use HMI's own
 * variable length coding: bytes with the high bit CLEAR are continuation
 * bytes carrying 7 bits each least-significant-first, and the final byte
 * has the high bit SET.  The file tempo comes from a BPM field in the
 * header; divisions are fixed at 60.
 */

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hmp2mid.h"
#include "file_io.h"
#include "wm_error.h"

#define HMP_DIVISIONS   60
#define TRK_CHUNKSIZE   8

/* Gremlin Interactive games such as Fatal Racing (aka Whiplash) ship
 * their HMP music "mangled" with a simple byte-oriented compression
 * scheme.  The format was documented by the FatalDecomp project
 * (https://github.com/FatalDecomp/Whiptools); this is an independent
 * implementation written from that format description:
 *
 * The file starts with the decompressed size as a 32 bit little endian
 * integer, followed by a stream of opcode driven commands:
 *
 *   0x00              end of stream
 *   0x01-0x3f         copy the next (op) bytes verbatim
 *   0x40-0x4f         byte ramp: emit (op & 0x0f) + 3 bytes, each the
 *                     previous output byte plus the fixed difference of
 *                     the last two output bytes
 *   0x50-0x5f         word ramp: emit (op & 0x0f) + 2 little endian
 *                     words, each the previous output word plus the
 *                     fixed difference of the last two output words
 *   0x60-0x6f         repeat the last output byte (op & 0x0f) + 3 times
 *   0x70-0x7f         repeat the last output word (op & 0x0f) + 2 times
 *   0x80-0xbf         copy 3 bytes from (op & 0x3f) + 3 bytes back in
 *                     the output
 *   0xc0-0xdf op2     copy ((op >> 2) & 0x07) + 4 bytes from
 *                     (((op & 0x03) << 8) | op2) + 3 bytes back
 *   0xe0-0xff op2 op3 copy (op3) + 5 bytes from
 *                     (((op & 0x1f) << 8) | op2) + 3 bytes back
 *
 * Back references may overlap the bytes they produce and must be copied
 * front to back one byte at a time.
 */

/* A mangled file begins with the 4 byte decompressed size, and the
 * compressors always open with a verbatim copy long enough to cover the
 * signature of the file inside.  So the byte at offset 4 is a literal
 * opcode and the HMP/HMI magic sits at offset 5. */
int _WM_IsMangled(const uint8_t *in, uint32_t insize) {
    uint32_t run;

    if (insize < 13)
        return (0);
    run = in[4];
    if (run < 8 || run > 0x3f)
        return (0);
    if (!memcmp(&in[5], "HMIMIDIP", 8))
        return (1);
    if (run >= 12 && insize >= 17 && !memcmp(&in[5], "HMI-MIDISONG", 12))
        return (1);
    return (0);
}

int _WM_Unmangle(const uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize) {
    uint8_t *dst;
    uint32_t dstsize;
    uint32_t inpos, outpos;
    uint32_t op, count, offset, i;

    *out = NULL;
    *outsize = 0;

    if (insize < 5) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
        return (-1);
    }

    dstsize = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
              ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    if (dstsize == 0 || dstsize > WM_MAXFILESIZE) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(bad size)", 0);
        return (-1);
    }

    dst = (uint8_t *) malloc(dstsize);
    if (dst == NULL) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
        return (-1);
    }

    inpos = 4;
    outpos = 0;

    while (inpos < insize && outpos < dstsize) {
        op = in[inpos++];

        if (op == 0x00) { /* end of stream */
            break;
        }
        else if (op <= 0x3f) { /* verbatim copy */
            count = op;
            if (count > insize - inpos || count > dstsize - outpos)
                goto _bad;
            memcpy(&dst[outpos], &in[inpos], count);
            inpos += count;
            outpos += count;
        }
        else if (op <= 0x4f) { /* byte ramp */
            uint8_t step;
            count = (op & 0x0f) + 3;
            if (outpos < 2 || count > dstsize - outpos)
                goto _bad;
            step = dst[outpos - 1] - dst[outpos - 2];
            for (i = 0; i < count; i++) {
                dst[outpos] = dst[outpos - 1] + step;
                outpos++;
            }
        }
        else if (op <= 0x5f) { /* word ramp */
            uint16_t word, step;
            count = (op & 0x0f) + 2;
            if (outpos < 4 || count > (dstsize - outpos) / 2)
                goto _bad;
            word = dst[outpos - 2] | (dst[outpos - 1] << 8);
            step = word - (dst[outpos - 4] | (dst[outpos - 3] << 8));
            for (i = 0; i < count; i++) {
                word += step;
                dst[outpos++] = word & 0xff;
                dst[outpos++] = (word >> 8) & 0xff;
            }
        }
        else if (op <= 0x6f) { /* byte run */
            count = (op & 0x0f) + 3;
            if (outpos < 1 || count > dstsize - outpos)
                goto _bad;
            memset(&dst[outpos], dst[outpos - 1], count);
            outpos += count;
        }
        else if (op <= 0x7f) { /* word run */
            count = (op & 0x0f) + 2;
            if (outpos < 2 || count > (dstsize - outpos) / 2)
                goto _bad;
            for (i = 0; i < count; i++) {
                dst[outpos] = dst[outpos - 2];
                dst[outpos + 1] = dst[outpos - 1];
                outpos += 2;
            }
        }
        else { /* back reference */
            if (op <= 0xbf) {
                offset = (op & 0x3f) + 3;
                count = 3;
            } else if (op <= 0xdf) {
                if (inpos >= insize)
                    goto _bad;
                offset = (((op & 0x03) << 8) | in[inpos++]) + 3;
                count = ((op >> 2) & 0x07) + 4;
            } else {
                if (insize - inpos < 2)
                    goto _bad;
                offset = (((op & 0x1f) << 8) | in[inpos++]) + 3;
                count = in[inpos++] + 5;
            }
            if (offset > outpos || count > dstsize - outpos)
                goto _bad;
            /* may overlap: copy front to back */
            for (i = 0; i < count; i++) {
                dst[outpos] = dst[outpos - offset];
                outpos++;
            }
        }
    }

    if (outpos != dstsize)
        goto _bad;

    *out = dst;
    *outsize = dstsize;
    return (0);

_bad:
    free(dst);
    _WM_GLOBAL_ERROR(WM_ERR_CORUPT, NULL, 0);
    return (-1);
}

struct hmp_ctx {
    uint8_t *dst, *dst_ptr;
    uint32_t dstsize, dstrem;
    int oom;
};

#define DST_CHUNK 8192
static void resize_dst(struct hmp_ctx *ctx) {
    uint32_t pos = ctx->dst_ptr - ctx->dst;
    uint8_t *newdst;

    if (ctx->oom)
        return;
    newdst = (uint8_t *) realloc(ctx->dst, ctx->dstsize + DST_CHUNK);
    if (newdst == NULL) {
        ctx->oom = 1;
        return;
    }
    ctx->dst = newdst;
    ctx->dstsize += DST_CHUNK;
    ctx->dstrem += DST_CHUNK;
    ctx->dst_ptr = ctx->dst + pos;
}

static void write1(struct hmp_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 1) {
        resize_dst(ctx);
        if (ctx->dstrem < 1)
            return;
    }
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void write2(struct hmp_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 2) {
        resize_dst(ctx);
        if (ctx->dstrem < 2)
            return;
    }
    *ctx->dst_ptr++ = (val>>8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void write4(struct hmp_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 4) {
        resize_dst(ctx);
        if (ctx->dstrem < 4)
            return;
    }
    *ctx->dst_ptr++ = (val>>24)&0xff;
    *ctx->dst_ptr++ = (val>>16)&0xff;
    *ctx->dst_ptr++ = (val>>8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 4;
}

static void writen(struct hmp_ctx *ctx, const uint8_t *data, uint32_t len)
{
    while (ctx->dstrem < len) {
        resize_dst(ctx);
        if (ctx->oom)
            return;
    }
    memcpy(ctx->dst_ptr, data, len);
    ctx->dst_ptr += len;
    ctx->dstrem -= len;
}

static void seekdst(struct hmp_ctx *ctx, uint32_t pos) {
    while (ctx->dstsize < pos) {
        resize_dst(ctx);
        if (ctx->oom)
            return;
    }
    ctx->dst_ptr = ctx->dst + pos;
    ctx->dstrem = ctx->dstsize - pos;
}

static void skipdst(struct hmp_ctx *ctx, int32_t pos) {
    size_t newpos = (ctx->dst_ptr - ctx->dst) + pos;
    while (ctx->dstsize < newpos) {
        resize_dst(ctx);
        if (ctx->oom)
            return;
    }
    ctx->dst_ptr = ctx->dst + newpos;
    ctx->dstrem = ctx->dstsize - newpos;
}

static uint32_t getdstpos(struct hmp_ctx *ctx) {
    return (ctx->dst_ptr - ctx->dst);
}

/* Conventional MIDI variable length quantity */
static void putvlq(struct hmp_ctx *ctx, uint32_t value) {
    uint32_t buffer;

    buffer = value & 0x7F;
    while (value >>= 7) {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
    }
    while (1) {
        write1(ctx, buffer & 0xFF);
        if (buffer & 0x80)
            buffer >>= 8;
        else
            break;
    }
}

/* HMP delta: high bit clear = continuation (LSB first), high bit set = last */
static int get_hmp_vlq(const uint8_t **pp, uint32_t *remaining, uint32_t *quant) {
    const uint8_t *p = *pp;
    uint32_t shift = 0;

    *quant = 0;
    while (*remaining) {
        (*remaining)--;
        if (*p & 0x80) {
            *quant |= (uint32_t)(*p++ & 0x7F) << shift;
            *pp = p;
            return (0);
        }
        *quant |= (uint32_t)(*p++ & 0x7F) << shift;
        shift += 7;
    }
    return (-1);
}

/* Conventional MIDI variable length quantity */
static int get_vlq(const uint8_t **pp, uint32_t *remaining, uint32_t *quant) {
    const uint8_t *p = *pp;

    *quant = 0;
    while (*remaining) {
        (*remaining)--;
        *quant = (*quant << 7) | (*p & 0x7F);
        if (!(*p++ & 0x80)) {
            *pp = p;
            return (0);
        }
    }
    return (-1);
}

#define READ_INT32LE(b) ((uint32_t)((b)[0] | ((b)[1] << 8) | ((b)[2] << 16) | ((b)[3] << 24)))

int _WM_hmp2midi(const uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize) {
    struct hmp_ctx ctx;
    uint8_t is_hmp2 = 0;
    uint32_t zero_cnt;
    uint32_t i;
    uint32_t hmp_chunks;
    uint32_t hmp_bpm;
    uint32_t tempo;
    const uint8_t *p;
    uint32_t remaining;
    int ret = -1;

    memset(&ctx, 0, sizeof(struct hmp_ctx));

    if (insize < 776) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
        return (-1);
    }

    if (memcmp(in, "HMIMIDIP", 8)) {
        _WM_GLOBAL_ERROR(WM_ERR_NOT_HMP, NULL, 0);
        return (-1);
    }
    p = in + 8;
    remaining = insize - 8;

    if (!memcmp(p, "013195", 6)) {
        if (remaining < 896) {
            _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
            return (-1);
        }
        p += 6;
        remaining -= 6;
        is_hmp2 = 1;
    }

    /* should be a bunch of \0's */
    zero_cnt = is_hmp2 ? 18 : 24;
    for (i = 0; i < zero_cnt; i++) {
        if (p[i] != 0) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_HMP, NULL, 0);
            return (-1);
        }
    }
    p += zero_cnt;
    remaining -= zero_cnt;

    /* file length, next 12 bytes normally \0: skip over them */
    p += 4 + 12;
    remaining -= 4 + 12;

    hmp_chunks = READ_INT32LE(p);
    p += 4;
    remaining -= 4;

    if (!hmp_chunks) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(no tracks)", 0);
        return (-1);
    }

    /* unknown */
    p += 4;
    remaining -= 4;

    /* beats per minute */
    hmp_bpm = READ_INT32LE(p);
    p += 4;
    remaining -= 4;

    if (!hmp_bpm) {
        _WM_GLOBAL_ERROR(WM_ERR_INVALID, "(bad bpm)", 0);
        return (-1);
    }
    tempo = 60000000 / hmp_bpm;

    /* song time, then the remainder of the header */
    p += 4;
    remaining -= 4;
    if (is_hmp2) {
        p += 840;
        remaining -= 840;
    } else {
        p += 712;
        remaining -= 712;
    }

    ctx.dst = (uint8_t *) malloc(DST_CHUNK);
    if (ctx.dst == NULL) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
        return (-1);
    }
    ctx.dst_ptr = ctx.dst;
    ctx.dstsize = DST_CHUNK;
    ctx.dstrem = DST_CHUNK;

    /* MIDI header; +1 track for the tempo track */
    write1(&ctx, 'M');
    write1(&ctx, 'T');
    write1(&ctx, 'h');
    write1(&ctx, 'd');
    write4(&ctx, 6);
    write2(&ctx, 1);                /* type 1: tracks play simultaneously */
    write2(&ctx, hmp_chunks + 1);
    write2(&ctx, HMP_DIVISIONS);

    /* tempo track */
    write1(&ctx, 'M');
    write1(&ctx, 'T');
    write1(&ctx, 'r');
    write1(&ctx, 'k');
    write4(&ctx, 11);
    write1(&ctx, 0x00); /* delta time */
    write1(&ctx, 0xff); /* set tempo */
    write2(&ctx, 0x5103);
    write1(&ctx, (tempo & 0x00ff0000) >> 16);
    write1(&ctx, (tempo & 0x0000ff00) >> 8);
    write1(&ctx, tempo & 0x000000ff);
    write1(&ctx, 0x00); /* delta time */
    write1(&ctx, 0xff); /* end of track */
    write2(&ctx, 0x2f00);

    for (i = 0; i < hmp_chunks; i++) {
        const uint8_t *chunk;
        uint32_t chunk_length;
        uint32_t track_size_pos, begin_track_pos, current_pos;
        uint32_t delta = 0;
        uint32_t pending_delta;
        int track_end = 0;

        if (remaining < 12) {
            goto tooshort;
        }

        chunk = p;
        chunk_length = READ_INT32LE(&p[4]);

        if (chunk_length < 12 || chunk_length > remaining) {
            goto tooshort;
        }
        remaining -= chunk_length;

        /* chunk number, chunk length and track number */
        p += 12;
        chunk_length -= 12;

        begin_track_pos = getdstpos(&ctx);
        write1(&ctx, 'M');
        write1(&ctx, 'T');
        write1(&ctx, 'r');
        write1(&ctx, 'k');
        track_size_pos = getdstpos(&ctx);
        skipdst(&ctx, 4);

        if (get_hmp_vlq(&p, &chunk_length, &delta) < 0) {
            goto tooshort;
        }
        pending_delta = delta;

        while (!track_end) {
            uint32_t event_size;
            uint8_t status;

            if (!chunk_length) {
                goto tooshort;
            }
            status = p[0];

            if (((status & 0xf0) == 0xb0) && (chunk_length >= 3)
                    && ((p[1] == 110) || (p[1] == 111)) && (p[2] > 0x7f)) {
                /* Reserved for loop markers: drop them but keep the delta */
                p += 3;
                chunk_length -= 3;
            } else {
                switch (status >> 4) {
                case 0x8:
                case 0x9:
                case 0xa:
                case 0xb:
                case 0xe:
                    event_size = 3;
                    break;
                case 0xc:
                case 0xd:
                    event_size = 2;
                    break;
                case 0xf:
                    if (status == 0xff) {
                        uint32_t meta_length = 0;
                        const uint8_t *meta = p + 2;
                        uint32_t meta_remaining;

                        if (chunk_length < 2) {
                            goto tooshort;
                        }
                        meta_remaining = chunk_length - 2;
                        if (get_vlq(&meta, &meta_remaining, &meta_length) < 0) {
                            goto tooshort;
                        }
                        if (meta_length > meta_remaining) {
                            goto tooshort;
                        }
                        event_size = (meta - p) + meta_length;
                        if (p[1] == 0x2f) {
                            track_end = 1;
                        }
                    } else if ((status == 0xf0) || (status == 0xf7)) {
                        uint32_t sysex_length = 0;
                        const uint8_t *sysex = p + 1;
                        uint32_t sysex_remaining;

                        if (chunk_length < 1) {
                            goto tooshort;
                        }
                        sysex_remaining = chunk_length - 1;
                        if (get_vlq(&sysex, &sysex_remaining, &sysex_length) < 0) {
                            goto tooshort;
                        }
                        if (sysex_length > sysex_remaining) {
                            goto tooshort;
                        }
                        event_size = (sysex - p) + sysex_length;
                    } else {
                        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(unrecognized event)", 0);
                        goto _end;
                    }
                    break;
                default:
                    /* HMP events always carry their status byte */
                    _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(unrecognized event)", 0);
                    goto _end;
                }

                if (event_size > chunk_length) {
                    goto tooshort;
                }
                putvlq(&ctx, pending_delta);
                writen(&ctx, p, event_size);
                pending_delta = 0;
                p += event_size;
                chunk_length -= event_size;
            }

            if (track_end) {
                break;
            }
            if (get_hmp_vlq(&p, &chunk_length, &delta) < 0) {
                goto tooshort;
            }
            pending_delta += delta;
        }

        /* write out track length */
        current_pos = getdstpos(&ctx);
        seekdst(&ctx, track_size_pos);
        write4(&ctx, current_pos - begin_track_pos - TRK_CHUNKSIZE);
        seekdst(&ctx, current_pos);

        /* goto start of next chunk */
        p = chunk + READ_INT32LE(&chunk[4]);

        if (ctx.oom) {
            _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
            goto _end;
        }
    }

    *out = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    return (0);

tooshort:
    _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
_end:
    free(ctx.dst);
    *out = NULL;
    *outsize = 0;
    return (ret);
}
