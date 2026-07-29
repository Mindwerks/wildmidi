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

#define __STDC_LIMIT_MACROS
#include "config.h"

#include <stddef.h>
#include <stdint.h>
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

    /* Sticky out-of-memory flag.  The write helpers below return void, so a
     * failed resize_dst() would otherwise silently drop output and leave the
     * caller reporting success with a truncated (invalid) MIDI buffer. */
    int oom;
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
    if (ctx->dstrem < 1) { if (resize_dst(ctx)) { ctx->oom = 1; return; } }
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void write2(struct smaf_ctx *ctx, uint32_t val) {
    if (ctx->dstrem < 2) { if (resize_dst(ctx)) { ctx->oom = 1; return; } }
    *ctx->dst_ptr++ = (val >> 8) & 0xff;
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem -= 2;
}

static void write4(struct smaf_ctx *ctx, uint32_t val) {
    if (ctx->dstrem < 4) { if (resize_dst(ctx)) { ctx->oom = 1; return; } }
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
    /* Grow first: resize_dst() reallocs, so dst_ptr must only be derived from
     * ctx->dst once the buffer is final.  Setting it up front would also form
     * an out-of-bounds pointer whenever pos is past the current end - which is
     * exactly the case that needs the growth. */
    while (ctx->dstsize < pos)
        if (resize_dst(ctx)) { ctx->oom = 1; return; }
    ctx->dst_ptr = ctx->dst + pos;
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
    uint32_t delta;
    /* A malformed file can wrap cur_ms (durations and gate times are
     * attacker-controlled VLQs), putting an event before the previous one.
     * The unsigned subtraction would then yield a delta near UINT32_MAX -
     * about 46 days of silence at 1 tick == 1 ms.  Clamp instead. */
    if (at_ms < ctx->last_event_ms) at_ms = ctx->last_event_ms;
    delta = at_ms - ctx->last_event_ms;
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
        uint32_t na;
        struct pending_off *n;
        if (ctx->offs_alloc > (UINT32_MAX / (2 * sizeof(struct pending_off))))
            return -1;
        na = ctx->offs_alloc ? ctx->offs_alloc * 2 : 64;
        n = (struct pending_off *)
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
/* Mobile Standard Huffman decompression (format_type 0x01).
 *
 * A compressed Mtsq body is: 4-byte big-endian uncompressed length, then a
 * bit stream (MSB first within each byte) carrying an Okumura-style prefix-
 * coded tree and the encoded bytes.
 *
 * Tree pass: each bit reads a node.
 *     bit 1 -> internal node, recursively read left then right subtrees.
 *     bit 0 -> leaf, read the next 8 bits (MSB first) as a literal byte.
 * The tree uses at most 2*256 - 1 = 511 slots; the lower 256 are the
 * possible leaf byte values (0..255), the higher slots are internal nodes
 * numbered from 256 upward.
 *
 * Data pass: for each output byte, walk from the root; 0 goes left, 1 goes
 * right, until reaching a node index < 256 which is the byte to emit.  Repeat
 * until the declared uncompressed length is reached. */
#define HUFF_MAX_NODES (2 * 256 - 1)

struct huff_state {
    const uint8_t *in;
    uint32_t       in_len;
    uint32_t       in_pos;
    uint8_t        bit_buf;
    uint8_t        bit_n;
    int            oom;
    int            avail;                   /* next internal-node slot to fill */
    int16_t        left [HUFF_MAX_NODES];
    int16_t        right[HUFF_MAX_NODES];
};

/* Read one bit MSB-first, or return -1 on end-of-stream. */
static int huff_bit(struct huff_state *h) {
    int b;
    if (h->bit_n == 0) {
        if (h->in_pos >= h->in_len) return -1;
        h->bit_buf = h->in[h->in_pos++];
        h->bit_n = 8;
    }
    b = (h->bit_buf >> 7) & 1;
    h->bit_buf = (uint8_t)(h->bit_buf << 1);
    h->bit_n--;
    return b;
}

/* Read the next 8 bits as a byte, or return -1 on end-of-stream. */
static int huff_byte(struct huff_state *h) {
    int v = 0, i, b;
    for (i = 0; i < 8; i++) {
        b = huff_bit(h);
        if (b < 0) return -1;
        v = (v << 1) | b;
    }
    return v;
}

/* Read the tree.  Returns the node index (0..510) or -1 on error. */
static int huff_read_tree(struct huff_state *h) {
    int b = huff_bit(h);
    if (b < 0) return -1;
    if (b) {
        int i, l, r;
        if (h->avail >= HUFF_MAX_NODES) return -1;
        i = h->avail++;
        l = huff_read_tree(h);
        if (l < 0) return -1;
        r = huff_read_tree(h);
        if (r < 0) return -1;
        h->left [i] = (int16_t) l;
        h->right[i] = (int16_t) r;
        return i;
    }
    return huff_byte(h);                    /* leaf: 8-bit literal */
}

/* Decompress `in` (compressed body without the 4-byte length prefix) into a
 * newly-allocated buffer of exactly `want` bytes.  Returns NULL on any error
 * (bad tree, truncated stream, allocation failure); caller frees the buffer. */
static uint8_t *huff_decompress(const uint8_t *in, uint32_t in_len,
                                uint32_t want) {
    struct huff_state h;
    uint8_t *out;
    uint32_t o;
    int root, j, b;

    memset(&h, 0, sizeof(h));
    h.in = in;
    h.in_len = in_len;
    h.avail = 256;

    root = huff_read_tree(&h);
    if (root < 0) return NULL;

    out = (uint8_t *) malloc(want ? want : 1);
    if (!out) return NULL;

    for (o = 0; o < want; o++) {
        j = root;
        while (j >= 256) {
            b = huff_bit(&h);
            if (b < 0) { free(out); return NULL; }
            j = b ? h.right[j] : h.left[j];
            if (j < 0 || j >= HUFF_MAX_NODES) { free(out); return NULL; }
        }
        out[o] = (uint8_t) j;
    }
    return out;
}

/* ------------------------------------------------------------------------- */

/* True if the container has an ATR audio track (a top-level "ATR*" chunk).
 * Used to keep audio-only SMAF files (CNTI + ATR, no MTR) from being rejected
 * as "not a SMAF file"; those play through MAFM's ATR wave-trigger path with
 * a minimal placeholder MIDI standing in for the score.  See the audio-only
 * emit block near the bottom of _WM_smaf2midi. */
static int has_audio_track(const uint8_t *in, uint32_t insize) {
    uint32_t pos = 8, end = insize;
    if (insize >= 8) {
        uint32_t declared = BE32(in + 4);
        if ((uint64_t) 8 + declared <= insize) end = 8 + declared;
    }
    while (pos + 8 <= end) {
        const uint8_t *c = in + pos;
        uint32_t sz = BE32(c + 4);
        uint32_t body = pos + 8;
        if ((uint64_t) body + sz > insize) sz = insize - body;
        if (memcmp(c, "ATR", 3) == 0) return 1;
        pos = body + sz;
        if (sz == 0) pos++;
    }
    return 0;
}

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

/* Decode a Mobile Standard (format 0x01/0x02) sequence into MIDI events.
 *
 * With sequ != 0, decode the MA-7 "SEQU" variant (format 0x03) instead.  SEQU
 * is the same record stream; only the status byte is packed differently, to
 * address 32 channels instead of 16:
 *
 *      bit 7    : channel bank  (0 -> channels 0-15, 1 -> channels 16-31)
 *      bits 6-4 : event type    (the low 3 bits of the MIDI status nibble,
 *                                i.e. type n means MIDI status 0x8+n)
 *      bits 3-0 : channel, low nibble
 *
 * so 0x1n is a note-with-velocity (MIDI 0x9n), 0x3n a control change (0xBn),
 * 0x4n a program change (0xCn), 0x6n a pitch bend (0xEn), and so on.  0xF0 and
 * 0xFF keep their literal Mobile meaning.  Because ordinary status bytes are
 * below 0x80 there is no running status in SEQU: every event carries one.
 *
 * Returns 0 on success, -1 on allocation failure. */
static int decode_mobile(struct smaf_ctx *ctx, const uint8_t *seq,
                         uint32_t seqlen, uint32_t ms_dur, uint32_t ms_gate,
                         int sequ) {
    uint8_t run_vel[MIDI_MAXCHANNELS];
    uint8_t run_status = 0;
    uint32_t p = 0, cur_ms = 0;
    int i;

    for (i = 0; i < MIDI_MAXCHANNELS; i++)
        run_vel[i] = 64;

    while (p < seqlen) {
        uint32_t dur;
        uint8_t s, ch;

        /* A duration is a VLQ, whose bytes are always < 0x80 except for the
         * "more" continuation bit.  A real 0xFF here is not a duration but the
         * meta/NOP/End-Of-Sequence trailer that follows the last event
         * (e.g. "ff 00 00  ff 2f 00"): stop rather than misread it as a huge
         * delay. */
        if (seq[p] == 0xff)
            break;

        dur = read_vlq(seq, &p, seqlen);
        cur_ms += dur * ms_dur;
        if (p >= seqlen) break;

        /* emit any note-offs that fall due before this event */
        flush_offs(ctx, cur_ms);

        s = seq[p];
        if (sequ) {
            p++;
            if (s != 0xf0 && s != 0xff) {
                /* ponytail: fold the 32 SEQU channels onto MIDI's 16.  Every
                 * MA-7 file seen so far plays only on channels 0-15 and leaves
                 * 16-31 at their setup defaults, so the fold never collides;
                 * carrying the upper bank properly would need a second MIDI
                 * port, which the converter has no way to express. */
                s = (uint8_t)(0x80 | (((s >> 4) & 7) << 4) | (s & 0x0f));
            }
        } else if (s & 0x80) {
            p++;
            run_status = s;
        } else if (run_status) {
            /* running status: reuse the previous status byte */
            s = run_status;
        } else {
            p++;
            continue;               /* no status established: stray data */
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
                break;
            write_event(ctx, cur_ms, 0x90 | ch, note & 0x7f, vel, 1);
            if (schedule_off(ctx, cur_ms + gate * ms_gate, ch, note) < 0)
                return -1;
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
                /* Voice-exclusives in Mtsu key on bank_lsb (see mafm.c
                 * mafm_parse_mtsu -> mafm_bank_entry.bank).  wildmidi's
                 * MIDI parser only tracks one bank byte via CC 0x00, so
                 * forward both SMAF bank bytes there; the LSB arriving
                 * after the MSB wins and MAFM matches the voice. */
                write_event(ctx, cur_ms, 0xb0 | ch, 0x00, val & 0x7f, 1);
                break;
            case 0x07:              /* volume */
            case 0x0a:              /* pan */
            case 0x0b:              /* expression */
            case 0x01:              /* modulation */
                write_event(ctx, cur_ms, 0xb0 | ch, cc, val & 0x7f, 1);
                break;
            default:
                break;
            }
        } break;

        case 0xc0: {                /* program change */
            uint8_t pc;
            if (p >= seqlen) { p = seqlen; break; }
            pc = seq[p++];
            write_event(ctx, cur_ms, 0xc0 | ch, pc & 0x7f, 0, 0);
        } break;

        case 0xd0:                  /* reserved: 1 data byte */
            p += 1;
            break;

        case 0xe0: {                /* pitch bend: lsb, msb */
            uint8_t lsb, msb;
            if (p + 1 >= seqlen) { p = seqlen; break; }
            lsb = seq[p++];
            msb = seq[p++];
            write_event(ctx, cur_ms, 0xe0 | ch, lsb & 0x7f, msb & 0x7f, 1);
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
    return 0;
}

/* ------------------------------------------------------------------------- */
/* HandyPhone Standard (format_type 0x00)                                    */
/*                                                                           */
/* HandyPhone score data differs from Mobile Standard: it uses a 1-or-2 byte */
/* VLQ, packs notes as channel/octave/note nibbles with no velocity, and     */
/* splits a song across up to four MTR* tracks of four channels each.  Those  */
/* tracks share one timeline, so we cannot stream each straight into the      */
/* output the way decode_mobile() does (that requires monotonically          */
/* increasing event times).  Instead every track is decoded into a shared    */
/* list of absolute-timed MIDI events, which is then sorted and emitted.      */
/* See docs/formats/SmafFileFormat.txt for the encoding.                     */

/* One fully-formed MIDI channel event at an absolute time (in ms). */
struct hp_event {
    uint32_t at_ms;
    uint32_t seq;               /* insertion order, for a stable sort */
    uint8_t status, d1, d2;
    uint8_t have_d2;
};

struct hp_events {
    struct hp_event *ev;
    uint32_t count, alloc;
};

static int hp_push(struct hp_events *e, uint32_t at_ms, uint8_t status,
                   uint8_t d1, uint8_t d2, int have_d2) {
    if (e->count == e->alloc) {
        uint32_t na;
        struct hp_event *n;
        if (e->alloc > (UINT32_MAX / (2 * sizeof(struct hp_event))))
            return -1;
        na = e->alloc ? e->alloc * 2 : 256;
        n = (struct hp_event *)
            realloc(e->ev, na * sizeof(struct hp_event));
        if (!n) return -1;
        e->ev = n;
        e->alloc = na;
    }
    e->ev[e->count].at_ms = at_ms;
    e->ev[e->count].seq = e->count;
    e->ev[e->count].status = status;
    e->ev[e->count].d1 = d1;
    e->ev[e->count].d2 = d2;
    e->ev[e->count].have_d2 = (uint8_t)(have_d2 != 0);
    e->count++;
    return 0;
}

/* Order by time, then by insertion order so a note-on never sorts after a
 * note-off queued at the same millisecond. */
static int hp_cmp(const void *a, const void *b) {
    const struct hp_event *x = (const struct hp_event *)a;
    const struct hp_event *y = (const struct hp_event *)b;
    if (x->at_ms != y->at_ms) return x->at_ms < y->at_ms ? -1 : 1;
    if (x->seq != y->seq)     return x->seq   < y->seq   ? -1 : 1;
    return 0;
}

/* HandyPhone VLQ: one byte < 0x80 is the value; otherwise the value is
 * (((b & 0x7F) + 1) << 7) | next.  Distinct from the Mobile-standard VLQ. */
static uint32_t hp_read_vlq(const uint8_t *seq, uint32_t *pp, uint32_t end) {
    uint32_t p = *pp;
    uint8_t b;
    uint32_t val;
    if (p >= end) { *pp = end; return 0; }
    b = seq[p++];
    if (b & 0x80) {
        if (p >= end) { *pp = end; return 0; }
        val = (((uint32_t)(b & 0x7f) + 1) << 7) | seq[p++];
    } else {
        val = b;
    }
    *pp = p;
    return val;
}

/* Map a HandyPhone note (octave/note nibbles) to a GM pitch, honouring the
 * per-channel octave shift.  Mirrors vavi-sound MidiContext.retrievePitch:
 * a +36 base, then the octave-shift offset. */
static int hp_pitch(uint8_t octave, uint8_t note, uint8_t oct_shift) {
    int pitch = (int)note + (int)octave * 12 + 36;
    switch (oct_shift) {
    case 1: pitch += 12; break;
    case 2: pitch += 24; break;
    case 3: pitch += 36; break;
    case 4: pitch += 48; break;
    case 0x81: pitch -= 12; break;
    case 0x82: pitch -= 24; break;
    case 0x83: pitch -= 36; break;
    case 0x84: pitch -= 48; break;
    default: break;
    }
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    return pitch;
}

/* Default velocity for HandyPhone notes, which carry no velocity byte. */
#define HP_NOTE_VELOCITY 100

/* Map a track's SMAF channel to a MIDI channel for a MELODIC voice.
 * MIDI channel 9 is the GM rhythm channel, so a melodic voice landing there
 * (track 2 channel 1 with base_ch 8, for instance) would be rendered as drums
 * and would also collide with the percussion reroute target.  Skip over 9;
 * four tracks of four channels still fit in the remaining 15 channels. */
static uint8_t hp_melodic_channel(uint8_t base_ch, uint8_t ch) {
    uint8_t c = (uint8_t)(base_ch + ch);
    return (uint8_t)((c >= 9) ? ((c + 1) & 0x0f) : c);
}

/* Decode one HandyPhone track's Mtsq into the shared event list.
 *   base_ch  : MIDI channel for this track's SMAF channel 0 (0,4,8,12).
 *   perc_mask: bit c set => SMAF channel c is a percussion channel and is
 *              rerouted to MIDI channel 9 (its notes taken from the program). */
/* F-Number/block -> MIDI pitch, for the register-write form below.
 *
 * The registers follow the classic OPL layout, so the sounding frequency is
 *      f = fnum * FS / 2^(20 - block)
 * with FS the chip's sample rate.  Yamaha's MA-1/MA-2 clock is not documented
 * here, so FS is taken as OPL2's 49716 Hz; that only shifts every pitch by the
 * same constant, and on the files where this form carries a real tune it lands
 * on sensible values (the two-tone ringers come out at 1975 and 2637 Hz, which
 * is what a phone ringer of that era sounds like).  Returns -1 if the result
 * is not a usable MIDI pitch. */
static int hp_reg_pitch(uint32_t fnum, uint32_t block) {
    /* Semitone frequencies for one octave, C4..B4, in milli-Hz.  Used instead
     * of a log() so this file keeps needing no libm - it is also built for the
     * DOS/OS-2/Watcom targets. */
    static const long semi[12] = {
        261626L, 277183L, 293665L, 311127L, 329628L, 349228L,
        369994L, 391995L, 415305L, 440000L, 466164L, 493883L
    };
    double hz;
    long mhz;
    int oct = 0, i, best = 0;
    long bestd = 0;

    if (fnum == 0) return -1;
    hz = (double) fnum * 49716.0 / (double)(1UL << (20 - block));
    if (hz < 8.0 || hz > 13000.0) return -1;
    mhz = (long)(hz * 1000.0 + 0.5);

    /* Fold into the C4..B4 octave, counting octaves as we go. */
    while (mhz >= semi[0] * 2) { mhz /= 2; oct++; }
    while (mhz <  semi[0])     { mhz *= 2; oct--; }

    for (i = 0; i < 12; i++) {                  /* nearest semitone */
        long d = mhz - semi[i];
        if (d < 0) d = -d;
        if (i == 0 || d < bestd) { bestd = d; best = i; }
    }
    /* The fold can leave us just under C, whose nearest entry is the B above. */
    if (semi[11] - mhz < mhz - semi[best] && semi[11] - mhz >= 0) best = 11;

    i = 60 + oct * 12 + best;
    if (i < 0 || i > 127) return -1;
    return i;
}

static int decode_handyphone(struct hp_events *e, const uint8_t *seq,
                             uint32_t seqlen, uint32_t ms_dur, uint32_t ms_gate,
                             uint8_t base_ch, uint8_t perc_mask) {
    uint32_t p = 0, cur_ms = 0;
    uint8_t oct_shift[4] = { 0, 0, 0, 0 };
    uint8_t program[4]   = { 0, 0, 0, 0 };
    /* State for the "43 03 90 <reg> <val>" direct register-write form (below):
     * the F-Number low byte per channel, and the pitch currently sounding so a
     * key-off knows which note to release. */
    uint8_t  reg_fnum[16];
    int      reg_playing[16];
    int      i;
    for (i = 0; i < 16; i++) { reg_fnum[i] = 0; reg_playing[i] = -1; }

    while (p < seqlen) {
        uint32_t dur;
        uint8_t e1;

        dur = hp_read_vlq(seq, &p, seqlen);
        cur_ms += dur * ms_dur;
        if (p >= seqlen) break;

        e1 = seq[p++];

        if (e1 == 0xff) {                       /* meta / exclusive / NOP */
            uint8_t e2;
            if (p >= seqlen) break;
            e2 = seq[p++];
            if (e2 == 0x00) {                   /* NOP */
                continue;
            } else if (e2 == 0x2f || e2 == 0x51 || e2 == 0x58) {
                uint8_t len;                    /* meta: length + payload */
                if (p >= seqlen) break;
                len = seq[p++];
                if ((uint32_t)p + len > seqlen) { p = seqlen; break; }
                p += len;                       /* dropped (GM tempo is fixed) */
            } else if (e2 == 0xf0) {            /* exclusive: length + payload */
                uint8_t len;
                if (p >= seqlen) break;
                len = seq[p++];
                if ((uint32_t)p + len > seqlen) { p = seqlen; break; }
                /* "43 03 90 <reg> <val>" is a direct write to an FM register.
                 * 21 files in the corpus (two-tone ringers and system alerts)
                 * carry their whole tune this way and emit no note events at
                 * all, so without this they render as pure silence.
                 *
                 * The registers are laid out as on OPL:
                 *   0xB0+n  F-Number, low 8 bits, channel n
                 *   0xC0+n  bit5 key-on, bits4-2 block, bits1-0 F-Num high
                 * so a key-on turns into a note-on at the pitch those two
                 * registers describe, and the matching key-off releases it.
                 *
                 * Treat this as approximate.  The layout is inferred from the
                 * corpus rather than from a Yamaha spec: it is convincing on
                 * the ringers (six files agree on the same two frequencies,
                 * landing within ~0.11 of a semitone), but the alert files
                 * write only two key-ons each, which is too little to confirm
                 * anything.  Playing them beats leaving them silent. */
                if (len >= 5 && seq[p] == 0x43 && seq[p + 1] == 0x03 &&
                    seq[p + 2] == 0x90) {
                    uint8_t reg = seq[p + 3], val = seq[p + 4];
                    if (reg >= 0xb0 && reg <= 0xbf) {
                        reg_fnum[reg - 0xb0] = val;
                    } else if (reg >= 0xc0 && reg <= 0xcf) {
                        int ch = reg - 0xc0;
                        uint8_t midi_ch = hp_melodic_channel(base_ch,
                                                             (uint8_t)(ch & 3));
                        if (reg_playing[ch] >= 0) {     /* release the old note */
                            if (hp_push(e, cur_ms, 0x80 | midi_ch,
                                        (uint8_t) reg_playing[ch], 0x40, 1) < 0)
                                return -1;
                            reg_playing[ch] = -1;
                        }
                        if (val & 0x20) {               /* key-on */
                            uint32_t fnum = ((uint32_t)(val & 3) << 8)
                                          | reg_fnum[ch];
                            int pitch = hp_reg_pitch(fnum, (val >> 2) & 7);
                            if (pitch >= 0) {
                                if (hp_push(e, cur_ms, 0x90 | midi_ch,
                                            (uint8_t) pitch,
                                            HP_NOTE_VELOCITY, 1) < 0)
                                    return -1;
                                reg_playing[ch] = pitch;
                            }
                        }
                    }
                }
                p += len;
            }
            /* other 0xff e2: no payload, ignore */
        } else if (e1 == 0x00) {                /* control event */
            uint8_t e2, ch, event, data;
            if (p >= seqlen) break;
            e2 = seq[p++];
            if (e2 == 0x00) {                   /* 00 00 xx */
                if (p >= seqlen) break;
                if (seq[p++] == 0x00)           /* 00 00 00 = end of sequence */
                    break;
                continue;                       /* 00 00 nonzero: undefined */
            }
            ch    = (e2 & 0xc0) >> 6;
            event = (e2 & 0x30) >> 4;
            data  =  e2 & 0x0f;
            if (event == 3) {                   /* long control: value byte */
                uint8_t val, midi_ch;
                if (p >= seqlen) break;
                val = seq[p++];
                midi_ch = (perc_mask & (1 << ch))
                        ? 9 : hp_melodic_channel(base_ch, ch);
                switch (data) {
                case 0x0:                       /* program change */
                    program[ch] = val & 0x7f;
                    /* On a percussion channel the program byte selects the
                     * drum-note pitch (used by the note handler), not a GM
                     * patch: suppress the program-change event itself, matching
                     * vavi-sound ProgramChangeMessage.getMidiEvents. */
                    if (!(perc_mask & (1 << ch)))
                        if (hp_push(e, cur_ms, 0xc0 | midi_ch, val & 0x7f, 0, 0) < 0)
                            return -1;
                    break;
                case 0x1:                       /* bank select */
                    /* Forward as MIDI CC 0x00 so the MAFM path can key
                     * voice-bank entries by it (see decode_mobile). */
                    if (hp_push(e, cur_ms, 0xb0 | midi_ch, 0x00, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                case 0x2:                       /* octave shift */
                    oct_shift[ch] = val;
                    break;
                case 0x3:                       /* modulation */
                    if (hp_push(e, cur_ms, 0xb0 | midi_ch, 0x01, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                case 0x4:                       /* pitch bend (7-bit -> 14) */
                    if (hp_push(e, cur_ms, 0xe0 | midi_ch, 0x00, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                case 0x7:                       /* volume */
                    if (hp_push(e, cur_ms, 0xb0 | midi_ch, 0x07, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                case 0xa:                       /* pan */
                    if (hp_push(e, cur_ms, 0xb0 | midi_ch, 0x0a, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                case 0xb:                       /* expression */
                    if (hp_push(e, cur_ms, 0xb0 | midi_ch, 0x0b, val & 0x7f, 1) < 0)
                        return -1;
                    break;
                default:
                    break;
                }
            }
            /* event 0/1/2 are short expression/pitch-bend/modulation; the
             * nibble is the value and there is no extra byte.  They are minor
             * expressive controls and are dropped for the GM conversion. */
        } else {                                /* note */
            uint8_t ch = (e1 & 0xc0) >> 6;
            uint8_t octave = (e1 & 0x30) >> 4;
            uint8_t note = e1 & 0x0f;
            uint32_t gate = hp_read_vlq(seq, &p, seqlen);
            uint8_t midi_ch;
            int pitch;
            if (gate == 0)
                continue;
            if (perc_mask & (1 << ch)) {        /* percussion: pitch = program */
                midi_ch = 9;
                pitch = program[ch] & 0x7f;
            } else {
                midi_ch = hp_melodic_channel(base_ch, ch);
                pitch = hp_pitch(octave, note, oct_shift[ch]);
            }
            if (hp_push(e, cur_ms, 0x90 | midi_ch,
                        (uint8_t)pitch, HP_NOTE_VELOCITY, 1) < 0)
                return -1;
            if (hp_push(e, cur_ms + gate * ms_gate, 0x80 | midi_ch,
                        (uint8_t)pitch, 0x40, 1) < 0)
                return -1;
        }
    }
    /* A register-write note is held until its key-off, so anything still
     * sounding when the stream ends has to be released here or it rings on
     * for the rest of the render. */
    for (i = 0; i < 16; i++) {
        if (reg_playing[i] >= 0) {
            uint8_t midi_ch = hp_melodic_channel(base_ch, (uint8_t)(i & 3));
            if (hp_push(e, cur_ms, 0x80 | midi_ch,
                        (uint8_t) reg_playing[i], 0x40, 1) < 0)
                return -1;
        }
    }
    return 0;
}

/* Emit the sorted HandyPhone event list into ctx as MIDI delta events. */
static void hp_emit(struct smaf_ctx *ctx, struct hp_events *e) {
    uint32_t i;
    qsort(e->ev, e->count, sizeof(struct hp_event), hp_cmp);
    for (i = 0; i < e->count; i++) {
        write_event(ctx, e->ev[i].at_ms, e->ev[i].status,
                    e->ev[i].d1, e->ev[i].d2, e->ev[i].have_d2);
    }
}

/* Walk every MTR* score track and, for each HandyPhone (format 0x00) track,
 * decode its Mtsq into the shared event list.  Returns the number of tracks
 * decoded, or -1 on allocation failure. */
static int decode_all_handyphone(struct hp_events *e, const uint8_t *in,
                                  uint32_t insize, uint32_t ms_dur,
                                  uint32_t ms_gate) {
    uint32_t pos = 8, end = insize;
    int track_no = 0;

    if (insize >= 8) {
        uint32_t declared = BE32(in + 4);
        if ((uint64_t)8 + declared <= insize)
            end = 8 + declared;
    }

    while (pos + 8 <= end && track_no < 4) {
        const uint8_t *c = in + pos;
        uint32_t sz = BE32(c + 4);
        uint32_t body = pos + 8;

        if ((uint64_t)body + sz > insize)
            sz = insize - body;

        if (memcmp(c, "MTR", 3) == 0 && sz >= 6 && c[8] == 0x00) {
            /* The 2-byte channel-status field packs a 4-bit value per channel
             * (2 bytes -> 4 channels), whose low 2 bits are the channel "type":
             * 0 NoCare, 1 Melody, 2 NoMelody, 3 Rhythm.  A Rhythm channel is
             * percussion: its notes are rerouted to MIDI channel 9 with the
             * program byte as the drum-note pitch (see decode_handyphone),
             * matching vavi-sound. */
            uint8_t perc_mask = 0;
            uint32_t p, tend;
            int i;

            for (i = 0; i < 4; i++) {
                uint8_t nib = (i < 2) ? ((c[12] >> (4 * (1 - i))) & 0x0f)
                                      : ((c[13] >> (4 * (3 - i))) & 0x0f);
                if ((nib & 0x03) == 3)  /* Rhythm */
                    perc_mask |= (uint8_t)(1 << i);
            }
            p = body + 6;               /* header = 4 + 2-byte channel status */
            tend = body + sz;
            while (p + 8 <= tend) {
                const uint8_t *s = in + p;
                uint32_t ssz = BE32(s + 4);
                uint32_t sbody = p + 8;
                if ((uint64_t)sbody + ssz > insize)
                    ssz = insize - sbody;
                if (memcmp(s, "Mtsq", 4) == 0) {
                    if (decode_handyphone(e, in + sbody, ssz, ms_dur, ms_gate,
                                          (uint8_t)(track_no * 4), perc_mask) < 0)
                        return -1;
                    track_no++;
                    break;
                }
                p = sbody + ssz;
                if (ssz == 0) p++;
            }
        }
        pos = body + sz;
        if (sz == 0) pos++;
    }
    return track_no;
}

/* ------------------------------------------------------------------------- */

int _WM_smaf2midi(const uint8_t *in, uint32_t insize,
                  uint8_t **out, uint32_t *outsize) {
    struct smaf_ctx ctx;
    const uint8_t *seq = NULL;
    uint32_t seqlen = 0;
    uint8_t tb_dur = 0x02, tb_gate = 0x02;
    uint32_t ms_dur, ms_gate;
    uint32_t track_size_pos, begin_track_pos, current_pos;
    uint8_t fmt = 0x02;
    uint8_t *huff_out = NULL;           /* allocated by huff_decompress on 0x01 */
    int ret = -1;

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
        /* No score track.  If the file at least has an ATR audio track
         * (Panasonic G60 system sounds, LG chocolate ringtone system sounds,
         * etc. - 83 such files in the libsmaf corpus), let it through with a
         * placeholder score.  MAFM's ATR wave-trigger path then plays the
         * audio and wildmidi_lib ends playback via its "no active voices"
         * check once the last wave finishes.  Files with neither MTR nor
         * ATR are still rejected. */
        if (!has_audio_track(in, insize)) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(no score track)", 0);
            return -1;
        }
        seq = NULL;
        seqlen = 0;
        tb_dur = 0x02;              /* placeholder; not used for audio-only */
        tb_gate = 0x02;
        fmt = 0xff;                 /* sentinel: audio-only */
    }

    /* Supported score-track formats: Mobile Standard (0x02), HandyPhone
     * Standard (0x00), MA-7 SEQU (0x03), the audio-only sentinel 0xff, and
     * now Mobile-Standard Huffman (0x01) - the body is [u32 BE uncompressed
     * length][Okumura-Huffman bit stream]; decompress it and treat the
     * result as a plain fmt=0x02 body. */
    if (fmt == 0x01) {
        uint32_t want;
        if (seqlen < 4) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(Huffman body truncated)", 0);
            return -1;
        }
        want = BE32(seq);
        if (want == 0 || want > 0x1000000u) {   /* 16 MiB sanity cap */
            _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(Huffman length out of range)", 0);
            return -1;
        }
        huff_out = huff_decompress(seq + 4, seqlen - 4, want);
        if (!huff_out) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_SMAF, "(Huffman decode failed)", 0);
            return -1;
        }
        seq = huff_out;
        seqlen = want;
        fmt = 0x02;                             /* now a plain Mobile stream */
    }
    if (fmt < 0xff && fmt > 0x03) {
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

    /* ---- decode the sequence according to its format ---- */
    if (fmt == 0xff) {
        /* Audio-only file: no score to decode.  The End Of Track marker below
         * receives a large delta so the wildmidi_lib clock has time for
         * MAFM's ATR-driven wave triggers to fire; wildmidi_lib's "no active
         * MAFM voices" check (with its built-in 10 s overrun window) ends
         * playback once the last wave has decayed.  All 83 audio-only files
         * in the libsmaf corpus are under 5 s of ATR content. */
        /* (nothing to emit here; the EOT-delta patch below handles it) */
    } else if (fmt == 0x00) {
        /* HandyPhone: merge every score track onto one timeline. */
        struct hp_events hev;
        int ntracks;
        memset(&hev, 0, sizeof(hev));
        ntracks = decode_all_handyphone(&hev, in, insize, ms_dur, ms_gate);
        if (ntracks < 0) {
            free(hev.ev);
            _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
            goto _end;
        }
        hp_emit(&ctx, &hev);
        free(hev.ev);
    } else {
        if (decode_mobile(&ctx, seq, seqlen, ms_dur, ms_gate,
                          fmt == 0x03) < 0) {
            _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
            goto _end;
        }
        /* flush any notes still held down */
        flush_all_offs(&ctx);
    }

    /* End Of Track.  Audio-only files carry a 3 s delta here (see the
     * fmt == 0xff branch above) so wildmidi_lib's clock runs long enough
     * for MAFM's ATR wave triggers to fire; every other case emits it
     * at delta 0 right after the last note-off. */
    if (fmt == 0xff)
        write_varlen(&ctx, (uint32_t) SMAF_DIVISION * 3);
    else
        write1(&ctx, 0x00);
    write1(&ctx, 0xff);
    write1(&ctx, 0x2f);
    write1(&ctx, 0x00);

    /* patch the MTrk length */
    current_pos = getdstpos(&ctx);
    seekdst(&ctx, track_size_pos);
    write4(&ctx, current_pos - begin_track_pos - 8);
    seekdst(&ctx, current_pos);

    /* An allocation failure anywhere in the write helpers leaves a truncated,
     * structurally invalid MIDI buffer; fail rather than hand it back. */
    if (ctx.oom) {
        _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
        goto _end;
    }

    *out = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    ctx.dst = NULL;                 /* ownership transferred */
    ret = 0;

_end:
    free(ctx.offs);
    free(huff_out);                     /* NULL-safe; only set on fmt=0x01 */
    if (ret < 0) {
        free(ctx.dst);
        *out = NULL;
        *outsize = 0;
    }
    return ret;
}
