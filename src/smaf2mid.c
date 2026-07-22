/*
 * SMAF2MIDI: Yamaha SMAF (MMF) to MIDI Library
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

/*
 * Converts Yamaha SMAF ("MMF") Mobile Standard score tracks into a Standard
 * MIDI File.  Only the score track SEQUENCE (Mtsq) is converted; embedded
 * PCM/ADPCM audio (Mtsp/ATR) and custom FM voice banks (Mtsu) are ignored, so
 * playback falls back to the General MIDI patch set.
 *
 * See docs/formats/SmafFileFormat.txt for the format description.
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "smaf2mid.h"
#include "wm_error.h"

/* SMAF ticks are given in milliseconds; we render the MIDI at a fixed 1 tick =
 * 1 ms by choosing a tempo of 1000 us/quarter-note against a division that
 * makes a quarter note 1000 ticks.  In practice we set the division to a
 * convenient PPQN and a matching tempo so that one SMAF millisecond maps to one
 * MIDI delta tick.  Using 1000 us per quarter and division 1000 gives exactly
 * 1 tick == 1 microsecond-quarter... instead we keep it simple: 1 MIDI tick ==
 * 1 ms via tempo 1,000,000 us/qn and division 1000 (1000 ticks per second). */
#define SMAF_DIVISION   1000            /* ticks per quarter note */
#define SMAF_TEMPO      1000000         /* microseconds per quarter note */
/* => 1000 ticks per quarter note, 1 quarter note per second => 1 tick == 1 ms */

#define MIDI_MAXCHANNELS 16

/* A note scheduled to be turned off at an absolute time (in ms). */
struct pending_off {
    uint32_t at_ms;     /* absolute time to send the note off */
    uint8_t channel;
    uint8_t note;
};

struct smaf_ctx {
    const uint8_t *src;
    uint32_t srcsize;

    uint8_t *dst, *dst_ptr;
    uint32_t dstsize, dstrem;

    /* pending note-offs, kept sorted by at_ms */
    struct pending_off *offs;
    uint32_t offs_count;
    uint32_t offs_alloc;

    uint32_t last_event_ms;     /* absolute time of the last written event */
};

#define DST_CHUNK 8192
static int resize_dst(struct smaf_ctx *ctx) {
    uint32_t pos = (uint32_t)(ctx->dst_ptr - ctx->dst);
    uint8_t *n = (uint8_t *) realloc(ctx->dst, ctx->dstsize + DST_CHUNK);
    if (!n) return -1;
    ctx->dst = n;
    ctx->dstsize += DST_CHUNK;
    ctx->dstrem += DST_CHUNK;
    ctx->dst_ptr = ctx->dst + pos;
    return 0;
}

static void write1(struct smaf_ctx *ctx, uint32_t val) {
    if (ctx->dstrem < 1) { if (resize_dst(ctx)) return; }
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void write2(struct smaf_ctx *ctx, uint32_t val) {
    if (ctx->dstrem < 2) { if (resize_dst(ctx)) return; }
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void write4(struct smaf_ctx *ctx, uint32_t val) {
    if (ctx->dstrem < 4) { if (resize_dst(ctx)) return; }
    *ctx->dst_ptr++ = (val >> 24) & 0xff;
    *ctx->dst_ptr++ = (val >> 16) & 0xff;
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 4;
}

/* write a MIDI variable-length delta time */
static void write_varlen(struct smaf_ctx *ctx, uint32_t value) {
    uint8_t buf[5];
    int n = 0;
    buf[n++] = value & 0x7f;
    while ((value >>= 7)) {
        buf[n++] = 0x80 | (value & 0x7f);
    }
    /* buf holds least-significant chunk first; emit most-significant first */
    while (n--) {
        write1(ctx, buf[n]);
    }
}

static uint32_t getdstpos(struct smaf_ctx *ctx) {
    return (uint32_t)(ctx->dst_ptr - ctx->dst);
}

static void seekdst(struct smaf_ctx *ctx, uint32_t pos) {
    ctx->dst_ptr = ctx->dst + pos;
    while (ctx->dstsize < pos)
        if (resize_dst(ctx)) return;
    ctx->dstrem = ctx->dstsize - pos;
}

/* ------------------------------------------------------------------------- */

/* big-endian 32-bit read */
#define BE32(p) (((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | \
                 ((uint32_t)(p)[2] << 8)  |  (uint32_t)(p)[3])

/* SMAF variable-length quantity (MIDI-style: 7 bits/byte, high bit = more).
 * Reads from seq[*pp], advancing *pp, never past end. */
static uint32_t read_vlq(const uint8_t *seq, uint32_t *pp, uint32_t end) {
    uint32_t val = 0;
    uint32_t p = *pp;
    while (p < end) {
        uint8_t b = seq[p++];
        val = (val << 7) | (b & 0x7f);
        if (!(b & 0x80)) break;
    }
    *pp = p;
    return val;
}

/* timebase code -> milliseconds per tick */
static uint32_t timebase_ms(uint8_t code) {
    switch (code) {
    case 0x00: case 0x10: return 1;
    case 0x01: case 0x11: return 2;
    case 0x02: case 0x12: return 4;
    case 0x03: case 0x13: return 5;
    default:              return 4;   /* sensible default */
    }
}

/* ------------------------------------------------------------------------- */

/* Write a channel-voice event at absolute time at_ms, emitting the delta. */
static void write_event(struct smaf_ctx *ctx, uint32_t at_ms,
                        uint8_t status, uint8_t d1, uint8_t d2, int have_d2) {
    uint32_t delta = at_ms - ctx->last_event_ms;
    write_varlen(ctx, delta);
    write1(ctx, status);
    write1(ctx, d1);
    if (have_d2) write1(ctx, d2);
    ctx->last_event_ms = at_ms;
}

/* Queue a note-off to be flushed when the timeline reaches off_ms. */
static int schedule_off(struct smaf_ctx *ctx, uint32_t off_ms,
                        uint8_t channel, uint8_t note) {
    uint32_t i;
    if (ctx->offs_count == ctx->offs_alloc) {
        uint32_t na = ctx->offs_alloc ? ctx->offs_alloc * 2 : 64;
        struct pending_off *n = (struct pending_off *)
            realloc(ctx->offs, na * sizeof(struct pending_off));
        if (!n) return -1;
        ctx->offs = n;
        ctx->offs_alloc = na;
    }
    /* insert keeping the list sorted by at_ms (ascending) */
    i = ctx->offs_count;
    while (i > 0 && ctx->offs[i - 1].at_ms > off_ms) {
        ctx->offs[i] = ctx->offs[i - 1];
        i--;
    }
    ctx->offs[i].at_ms = off_ms;
    ctx->offs[i].channel = channel;
    ctx->offs[i].note = note;
    ctx->offs_count++;
    return 0;
}

/* Flush every pending note-off whose time is <= up_to_ms. */
static void flush_offs(struct smaf_ctx *ctx, uint32_t up_to_ms) {
    uint32_t i = 0;
    while (i < ctx->offs_count && ctx->offs[i].at_ms <= up_to_ms) {
        write_event(ctx, ctx->offs[i].at_ms,
                    0x80 | (ctx->offs[i].channel & 0x0f),
                    ctx->offs[i].note & 0x7f, 0x40, 1);
        i++;
    }
    if (i) {
        memmove(ctx->offs, ctx->offs + i,
                (ctx->offs_count - i) * sizeof(struct pending_off));
        ctx->offs_count -= i;
    }
}

/* Flush all remaining note-offs regardless of time. */
static void flush_all_offs(struct smaf_ctx *ctx) {
    uint32_t i;
    for (i = 0; i < ctx->offs_count; i++) {
        write_event(ctx, ctx->offs[i].at_ms,
                    0x80 | (ctx->offs[i].channel & 0x0f),
                    ctx->offs[i].note & 0x7f, 0x40, 1);
    }
    ctx->offs_count = 0;
}

/* ------------------------------------------------------------------------- */

/* Locate the first score track (MTR*) and its Mtsq sequence chunk.
 * Returns 0 on success and fills *seq / *seqlen / *tb_dur / *tb_gate. */
static int find_sequence(const uint8_t *in, uint32_t insize,
                         const uint8_t **seq, uint32_t *seqlen,
                         uint8_t *tb_dur, uint8_t *tb_gate, uint8_t *fmt_out) {
    uint32_t pos = 8;               /* skip MMMD + size */
    uint32_t end = insize;

    if (insize >= 8) {
        uint32_t declared = BE32(in + 4);
        /* payload excludes the CRC trailer; never trust it past the buffer */
        if ((uint64_t)8 + declared <= insize)
            end = 8 + declared;
    }

    while (pos + 8 <= end) {
        const uint8_t *c = in + pos;
        uint32_t sz = BE32(c + 4);
        uint32_t body = pos + 8;
        uint8_t fmt, chlen, hdr;
        uint32_t p, tend;

        if ((uint64_t)body + sz > insize)
            sz = insize - body;         /* clamp to real bytes */

        if (memcmp(c, "MTR", 3) != 0) {
            pos = body + sz;
            if (sz == 0) pos++;         /* never spin on a zero-size chunk */
            continue;
        }

        /* score track: parse the fixed header, then find Mtsq */
        if (sz < 4) return -1;
        fmt = c[8];
        *tb_dur = c[10];
        *tb_gate = c[11];
        chlen = (fmt == 0x00) ? 2 : (fmt == 0x03) ? 32 : 16;
        hdr = 4 + chlen;
        if (hdr > sz) return -1;

        p = body + hdr;
        tend = body + sz;
        while (p + 8 <= tend) {
            const uint8_t *s = in + p;
            uint32_t ssz = BE32(s + 4);
            uint32_t sbody = p + 8;
            if ((uint64_t)sbody + ssz > insize)
                ssz = insize - sbody;
            if (memcmp(s, "Mtsq", 4) == 0) {
                *seq = in + sbody;
                *seqlen = ssz;
                *fmt_out = fmt;
                return 0;
            }
            p = sbody + ssz;
            if (ssz == 0) p++;
        }
        /* MTR without an Mtsq: keep scanning for another track */
        pos = body + sz;
        if (sz == 0) pos++;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */

int _WM_smaf2midi(const uint8_t *in, uint32_t insize,
                  uint8_t **out, uint32_t *outsize) {
    struct smaf_ctx ctx;
    const uint8_t *seq = NULL;
    uint32_t seqlen = 0;
    uint8_t tb_dur = 0x02, tb_gate = 0x02;
    uint32_t ms_dur, ms_gate;
    uint32_t p, cur_ms;
    uint32_t track_size_pos, begin_track_pos, current_pos;
    uint8_t run_vel[MIDI_MAXCHANNELS];
    uint8_t run_status = 0;
    uint8_t fmt = 0x02;
    int i, ret = -1;

    if (!out || !outsize) {
        _WM_GLOBAL_ERROR(WM_ERR_INVALID_ARG, "(NULL params)", 0);
        return -1;
    }
    *out = NULL;
    *outsize = 0;

    if (insize < 8 || memcmp(in, "MMMD", 4) != 0) {
        _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, NULL, 0);
        return -1;
    }

    if (find_sequence(in, insize, &seq, &seqlen, &tb_dur, &tb_gate, &fmt) < 0) {
        _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(no score track)", 0);
        return -1;
    }

    /* Only Mobile Standard sequences (format 0x01/0x02) are supported.  The
     * MA-7 "Compress" variant (0x03) and HandyPhone Standard (0x00) use
     * different event encodings that are not yet implemented; decline them
     * rather than emit a mangled conversion. */
    if (fmt != 0x01 && fmt != 0x02) {
        _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(unsupported SMAF track format)", 0);
        return -1;
    }

    ms_dur = timebase_ms(tb_dur);
    ms_gate = timebase_ms(tb_gate);

    memset(&ctx, 0, sizeof(ctx));
    ctx.src = in;
    ctx.srcsize = insize;
    ctx.dst = (uint8_t *) calloc(DST_CHUNK, 1);
    if (!ctx.dst) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
        return -1;
    }
    ctx.dst_ptr = ctx.dst;
    ctx.dstsize = DST_CHUNK;
    ctx.dstrem = DST_CHUNK;

    for (i = 0; i < MIDI_MAXCHANNELS; i++)
        run_vel[i] = 64;

    /* MThd */
    write1(&ctx, 'M'); write1(&ctx, 'T'); write1(&ctx, 'h'); write1(&ctx, 'd');
    write4(&ctx, 6);
    write2(&ctx, 0);                /* format 0 */
    write2(&ctx, 1);                /* one track */
    write2(&ctx, SMAF_DIVISION);

    /* MTrk */
    begin_track_pos = getdstpos(&ctx);
    write1(&ctx, 'M'); write1(&ctx, 'T'); write1(&ctx, 'r'); write1(&ctx, 'k');
    track_size_pos = getdstpos(&ctx);
    write4(&ctx, 0);                /* placeholder; patched at the end */

    /* tempo meta so that 1 tick == 1 ms */
    write1(&ctx, 0x00);
    write1(&ctx, 0xff);
    write1(&ctx, 0x51);
    write1(&ctx, 0x03);
    write1(&ctx, (SMAF_TEMPO >> 16) & 0xff);
    write1(&ctx, (SMAF_TEMPO >> 8) & 0xff);
    write1(&ctx, SMAF_TEMPO & 0xff);

    /* ---- decode the Mobile Standard sequence ---- */
    p = 0;
    cur_ms = 0;
    while (p < seqlen) {
        uint32_t dur;
        uint8_t s, ch;

        /* A duration is a VLQ, whose bytes are always < 0x80 except for the
         * "more" continuation bit.  A real 0xFF here is not a duration but the
         * meta/NOP/End-Of-Sequence trailer that follows the last event
         * (e.g. "ff 00 00  ff 2f 00"): stop rather than misread it as a huge
         * delay.  Seen in MA-7 (format 0x03) files. */
        if (seq[p] == 0xff)
            break;

        dur = read_vlq(seq, &p, seqlen);
        cur_ms += dur * ms_dur;
        if (p >= seqlen) break;

        /* emit any note-offs that fall due before this event */
        flush_offs(&ctx, cur_ms);

        s = seq[p];
        if (s & 0x80) {
            /* explicit status byte */
            p++;
            run_status = s;
        } else if (run_status) {
            /* running status: reuse the previous status byte, the current byte
             * is the first data byte. */
            s = run_status;
        } else {
            /* no status established yet: genuinely stray data */
            p++;
            continue;
        }
        ch = s & 0x0f;

        switch (s & 0xf0) {
        case 0x80:                  /* note, reuse running velocity */
        case 0x90: {                /* note, explicit velocity */
            uint8_t note, vel;
            uint32_t gate;
            if (p >= seqlen) { p = seqlen; break; }
            note = seq[p++];
            if ((s & 0xf0) == 0x90) {
                if (p >= seqlen) { p = seqlen; break; }
                vel = seq[p++] & 0x7f;
                run_vel[ch] = vel;
            } else {
                vel = run_vel[ch];
            }
            gate = read_vlq(seq, &p, seqlen);
            if (gate == 0)
                break;              /* zero-gate note: skip per spec */
            write_event(&ctx, cur_ms, 0x90 | ch, note & 0x7f, vel, 1);
            if (schedule_off(&ctx, cur_ms + gate * ms_gate, ch, note) < 0) {
                _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
                goto _end;
            }
        } break;

        case 0xa0:                  /* reserved: 2 data bytes */
            p += 2;
            break;

        case 0xb0: {                /* control change */
            uint8_t cc, val;
            if (p + 1 >= seqlen) { p = seqlen; break; }
            cc = seq[p++];
            val = seq[p++];
            switch (cc) {
            case 0x00:              /* bank MSB */
            case 0x20:              /* bank LSB */
                /* SMAF bank-select values (e.g. 0x7C) address Yamaha's own
                 * internal/custom voice banks, NOT General MIDI banks.  A real
                 * SMAF player uses them to bind the file's Mtsu voice table; a
                 * GM synth given bank 0x7C finds no instrument and goes silent.
                 * Since we target GM, drop bank-select entirely and let the
                 * following Program Change pick the GM patch. */
                break;
            case 0x07:              /* volume */
            case 0x0a:              /* pan */
            case 0x0b:              /* expression */
            case 0x01:              /* modulation */
                write_event(&ctx, cur_ms, 0xb0 | ch, cc, val & 0x7f, 1);
                break;
            default:
                break;              /* consume + ignore */
            }
        } break;

        case 0xc0: {                /* program change */
            uint8_t pc;
            if (p >= seqlen) { p = seqlen; break; }
            pc = seq[p++];
            write_event(&ctx, cur_ms, 0xc0 | ch, pc & 0x7f, 0, 0);
        } break;

        case 0xd0:                  /* reserved: 1 data byte */
            p += 1;
            break;

        case 0xe0: {                /* pitch bend: lsb, msb */
            uint8_t lsb, msb;
            if (p + 1 >= seqlen) { p = seqlen; break; }
            lsb = seq[p++];
            msb = seq[p++];
            write_event(&ctx, cur_ms, 0xe0 | ch, lsb & 0x7f, msb & 0x7f, 1);
        } break;

        case 0xf0:
            run_status = 0;         /* system messages cancel running status */
            if (s == 0xf0) {        /* exclusive: skip len bytes */
                uint32_t len = read_vlq(seq, &p, seqlen);
                if ((uint64_t)p + len > seqlen) { p = seqlen; break; }
                p += len;
            } else {                /* 0xff: meta */
                uint8_t m;
                if (p >= seqlen) { p = seqlen; break; }
                m = seq[p++];
                if (m == 0x00)
                    break;          /* NOP */
                if (m == 0x2f) {    /* end of sequence */
                    p = seqlen;
                    break;
                }
                /* other meta: length byte + skip */
                if (p < seqlen) {
                    uint8_t len = seq[p++];
                    if ((uint32_t)p + len > seqlen) p = seqlen;
                    else p += len;
                }
            }
            break;

        default:
            break;
        }
    }

    /* flush any notes still held down */
    flush_all_offs(&ctx);

    /* End Of Track */
    write1(&ctx, 0x00);
    write1(&ctx, 0xff);
    write1(&ctx, 0x2f);
    write1(&ctx, 0x00);

    /* patch the MTrk length */
    current_pos = getdstpos(&ctx);
    seekdst(&ctx, track_size_pos);
    write4(&ctx, current_pos - begin_track_pos - 8);
    seekdst(&ctx, current_pos);

    *out = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    ctx.dst = NULL;                 /* ownership transferred */
    ret = 0;

_end:
    free(ctx.offs);
    if (ret < 0) {
        free(ctx.dst);
        *out = NULL;
        *outsize = 0;
    }
    return ret;
}
