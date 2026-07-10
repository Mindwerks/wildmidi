/*
 * HMI2MIDI: HMI Sound Operating System HMI to MIDI Library
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

/* HMI Converter
 *
 * Format info comes from f_hmi.c: an HMI file holds a set of tracks that
 * play simultaneously, so we emit a type-1 MIDI file.  Tracks are standard
 * MIDI event streams with conventional delta times and running status, plus
 * two HMI extensions: 0xfe prefixed HMI-only events (skipped), and note on
 * events that carry an extra variable length note duration instead of
 * having matching note off events.  Like xmi2mid, we convert each track
 * into a time sorted event list so the note off events generated from
 * those durations can be merged in, then write the list out as an MTrk.
 * The file tempo comes from a BPM byte in the header; divisions are fixed
 * at 60.
 */

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hmi2mid.h"
#include "wm_error.h"

#define HMI_DIVISIONS   60
#define TRK_CHUNKSIZE   8

typedef struct _midi_event {
    int32_t time;
    uint8_t status;
    uint8_t data[2];
    uint32_t len;
    uint8_t *buffer;
    struct _midi_event *next;
} midi_event;

struct hmi_ctx {
    uint8_t *dst, *dst_ptr;
    uint32_t dstsize, dstrem;
    int oom;
    midi_event *list;
    midi_event *current;
};

#define DST_CHUNK 8192
static void resize_dst(struct hmi_ctx *ctx) {
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

static void write1(struct hmi_ctx *ctx, uint32_t val)
{
    if (ctx->dstrem < 1) {
        resize_dst(ctx);
        if (ctx->dstrem < 1)
            return;
    }
    *ctx->dst_ptr++ = val & 0xff;
    ctx->dstrem--;
}

static void write2(struct hmi_ctx *ctx, uint32_t val)
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

static void write4(struct hmi_ctx *ctx, uint32_t val)
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

static void seekdst(struct hmi_ctx *ctx, uint32_t pos) {
    while (ctx->dstsize < pos) {
        resize_dst(ctx);
        if (ctx->oom)
            return;
    }
    ctx->dst_ptr = ctx->dst + pos;
    ctx->dstrem = ctx->dstsize - pos;
}

static void skipdst(struct hmi_ctx *ctx, int32_t pos) {
    size_t newpos = (ctx->dst_ptr - ctx->dst) + pos;
    while (ctx->dstsize < newpos) {
        resize_dst(ctx);
        if (ctx->oom)
            return;
    }
    ctx->dst_ptr = ctx->dst + newpos;
    ctx->dstrem = ctx->dstsize - newpos;
}

static uint32_t getdstpos(struct hmi_ctx *ctx) {
    return (ctx->dst_ptr - ctx->dst);
}

static void DeleteEventList(midi_event *mlist) {
    midi_event *event;
    midi_event *next;

    next = mlist;

    while ((event = next) != NULL) {
        next = event->next;
        free(event->buffer);
        free(event);
    }
}

/* Sets current to the new event and updates list.
 * Returns the new event, or NULL when out of memory. */
static midi_event *CreateNewEvent(struct hmi_ctx *ctx, int32_t time) {
    midi_event *event;

    if (!ctx->list) {
        event = (midi_event *) calloc(1, sizeof(midi_event));
        if (event == NULL)
            return (NULL);
        ctx->list = ctx->current = event;
        ctx->current->time = (time < 0)? 0 : time;
        return (ctx->current);
    }

    if (time < 0) {
        event = (midi_event *) calloc(1, sizeof(midi_event));
        if (event == NULL)
            return (NULL);
        event->next = ctx->list;
        ctx->list = ctx->current = event;
        return (ctx->current);
    }

    if (ctx->current->time > time)
        ctx->current = ctx->list;

    while (ctx->current->next) {
        if (ctx->current->next->time > time) {
            event = (midi_event *) calloc(1, sizeof(midi_event));
            if (event == NULL)
                return (NULL);
            event->next = ctx->current->next;
            ctx->current->next = event;
            ctx->current = event;
            ctx->current->time = time;
            return (ctx->current);
        }

        ctx->current = ctx->current->next;
    }

    event = (midi_event *) calloc(1, sizeof(midi_event));
    if (event == NULL)
        return (NULL);
    ctx->current->next = event;
    ctx->current = event;
    ctx->current->time = time;
    return (ctx->current);
}

static int PutVLQ(struct hmi_ctx *ctx, uint32_t value) {
    int32_t buffer;
    int i = 1, j;
    buffer = value & 0x7F;
    while (value >>= 7) {
        buffer <<= 8;
        buffer |= ((value & 0x7F) | 0x80);
        i++;
    }
    for (j = 0; j < i; j++) {
        write1(ctx, buffer & 0xFF);
        buffer >>= 8;
    }

    return (i);
}

/* Conventional MIDI variable length quantity */
static int GetVLQ(const uint8_t **pp, uint32_t *remaining, uint32_t *quant) {
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

/* Converts an HMI track into a time sorted event list.
 * Returns 0 on success, -1 on corrupt data, -2 when out of memory. */
static int ConvertTracktoList(struct hmi_ctx *ctx, const uint8_t *data,
                              uint32_t remaining) {
    int32_t time = 0;
    uint32_t delta = 0;
    uint8_t running_event = 0;
    uint8_t status;
    int end = 0;

    if (GetVLQ(&data, &remaining, &delta) < 0)
        return (-1);
    time += delta;

    while (!end) {
        if (!remaining)
            return (-1);

        if (data[0] == 0xfe) {
            /* HMI only event of some sort. */
            uint32_t skip;

            if (remaining < 2)
                return (-1);
            if (data[1] == 0x10) {
                if (remaining < 5)
                    return (-1);
                skip = data[4] + 5 + 4;
            } else if (data[1] == 0x15) {
                skip = 8;
            } else {
                skip = 4;
            }
            if (skip > remaining)
                return (-1);
            data += skip;
            remaining -= skip;
        } else {
            if (data[0] >= 0x80) {
                status = *data++;
                remaining--;
                /* Sysex resets running event data,
                 * 0xff does not alter running event */
                if ((status == 0xF0) || (status == 0xF7)) {
                    running_event = 0;
                } else if (status < 0xF0) {
                    running_event = status;
                }
            } else {
                if (!running_event)
                    return (-1);
                status = running_event;
            }

            switch (status >> 4) {
            case 0x9: {
                /* Note on has extra data stating note length */
                uint32_t length = 0;
                midi_event *prev;

                if (remaining < 2)
                    return (-1);
                if (CreateNewEvent(ctx, time) == NULL)
                    return (-2);
                ctx->current->status = status;
                ctx->current->data[0] = *data++;
                ctx->current->data[1] = *data++;
                remaining -= 2;

                if (GetVLQ(&data, &remaining, &length) < 0)
                    return (-1);

                prev = ctx->current;
                if (CreateNewEvent(ctx, time + length) == NULL)
                    return (-2);
                ctx->current->status = status;
                ctx->current->data[0] = prev->data[0];
                ctx->current->data[1] = 0;
                ctx->current = prev;
                break;
            }

            /* 2 byte data */
            case 0x8:
            case 0xa:
            case 0xb:
            case 0xe:
                if (remaining < 2)
                    return (-1);
                if (CreateNewEvent(ctx, time) == NULL)
                    return (-2);
                ctx->current->status = status;
                ctx->current->data[0] = *data++;
                ctx->current->data[1] = *data++;
                remaining -= 2;
                break;

            /* 1 byte data */
            case 0xc:
            case 0xd:
                if (remaining < 1)
                    return (-1);
                if (CreateNewEvent(ctx, time) == NULL)
                    return (-2);
                ctx->current->status = status;
                ctx->current->data[0] = *data++;
                remaining--;
                break;

            case 0xf:
                if (CreateNewEvent(ctx, time) == NULL)
                    return (-2);
                ctx->current->status = status;
                if (status == 0xff) {
                    if (remaining < 1)
                        return (-1);
                    ctx->current->data[0] = *data++;
                    remaining--;
                    if (ctx->current->data[0] == 0x2f)
                        end = 1;
                }
                if (GetVLQ(&data, &remaining, &ctx->current->len) < 0)
                    return (-1);
                if (ctx->current->len > remaining)
                    return (-1);
                if (ctx->current->len) {
                    ctx->current->buffer = (uint8_t *) malloc(ctx->current->len);
                    if (ctx->current->buffer == NULL) {
                        ctx->current->len = 0;
                        return (-2);
                    }
                    memcpy(ctx->current->buffer, data, ctx->current->len);
                    data += ctx->current->len;
                    remaining -= ctx->current->len;
                }
                break;

            default: /* Never occur */
                return (-1);
            }
        }

        if (end)
            break;

        if (GetVLQ(&data, &remaining, &delta) < 0)
            return (-1);
        time += delta;
    }

    /* Move the end of track marker after the note off events generated
     * from the note durations, so none of them get cut short. */
    if (ctx->current->next) {
        midi_event *event = ctx->list;
        midi_event *prev = NULL;
        midi_event *tail;

        while (event->next) {
            if ((event->status == 0xff) && (event->data[0] == 0x2f)) {
                if (prev)
                    prev->next = event->next;
                else
                    ctx->list = event->next;
                tail = event->next;
                while (tail->next)
                    tail = tail->next;
                event->time = tail->time;
                event->next = NULL;
                tail->next = event;
                break;
            }
            prev = event;
            event = event->next;
        }
    }

    return (0);
}

/* Converts an event list to a MTrk
 * Returns bytes of the array */
static uint32_t ConvertListToMTrk(struct hmi_ctx *ctx, midi_event *mlist) {
    int32_t time = 0;
    midi_event *event;
    uint32_t delta;
    uint8_t last_status = 0;
    uint32_t i = 8;
    uint32_t j;
    uint32_t size_pos, cur_pos;
    int end = 0;

    write1(ctx, 'M');
    write1(ctx, 'T');
    write1(ctx, 'r');
    write1(ctx, 'k');

    size_pos = getdstpos(ctx);
    skipdst(ctx, 4);

    for (event = mlist; event && !end; event = event->next) {
        delta = (event->time - time);
        time = event->time;

        i += PutVLQ(ctx, delta);

        if ((event->status != last_status) || (event->status >= 0xF0)) {
            write1(ctx, event->status);
            i++;
        }

        last_status = event->status;

        switch (event->status >> 4) {
        /* 2 bytes data
         * Note off, Note on, Aftertouch, Controller and Pitch Wheel */
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xE:
            write1(ctx, event->data[0]);
            write1(ctx, event->data[1]);
            i += 2;
            break;

        /* 1 bytes data
         * Program Change and Channel Pressure */
        case 0xC:
        case 0xD:
            write1(ctx, event->data[0]);
            i++;
            break;

        /* Variable length
         * SysEx */
        case 0xF:
            if (event->status == 0xFF) {
                if (event->data[0] == 0x2f)
                    end = 1;
                write1(ctx, event->data[0]);
                i++;
            }
            i += PutVLQ(ctx, event->len);
            if (event->len) {
                for (j = 0; j < event->len; j++) {
                    write1(ctx, event->buffer[j]);
                    i++;
                }
            }
            break;

        /* Never occur */
        default:
            _WM_DEBUG_MSG("%s: unrecognized event", _WM_FUNCTION);
            break;
        }
    }

    cur_pos = getdstpos(ctx);
    seekdst(ctx, size_pos);
    write4(ctx, i - 8);
    seekdst(ctx, cur_pos);

    return (i);
}

#define READ_INT32LE(b) ((uint32_t)((b)[0] | ((b)[1] << 8) | ((b)[2] << 16) | ((b)[3] << 24)))

int _WM_hmi2midi(const uint8_t *in, uint32_t insize,
                 uint8_t **out, uint32_t *outsize) {
    struct hmi_ctx ctx;
    uint16_t hmi_bpm;
    uint32_t hmi_track_cnt;
    uint32_t tempo;
    uint32_t i;
    int ret = -1;

    memset(&ctx, 0, sizeof(struct hmi_ctx));

    if (insize <= 370) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
        return (-1);
    }

    if (memcmp(in, "HMI-MIDISONG061595", 18)) {
        _WM_GLOBAL_ERROR(WM_ERR_NOT_HMI, NULL, 0);
        return (-1);
    }

    hmi_bpm = in[212];
    hmi_track_cnt = in[228];

    if (!hmi_track_cnt) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(no tracks)", 0);
        return (-1);
    }
    if (!hmi_bpm) {
        _WM_GLOBAL_ERROR(WM_ERR_INVALID, "(bad bpm)", 0);
        return (-1);
    }
    tempo = 60000000 / hmi_bpm;

    if (insize < (370 + (hmi_track_cnt * 4))) {
        _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
        return (-1);
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
    write2(&ctx, hmi_track_cnt + 1);
    write2(&ctx, HMI_DIVISIONS);

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

    for (i = 0; i < hmi_track_cnt; i++) {
        uint32_t track_offset = READ_INT32LE(&in[370 + (i * 4)]);
        uint32_t track_header_length;
        int cvt_ret;

        /* written to avoid an integer overflow: insize > 370 here */
        if (track_offset > insize - (0x5a + 4)) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_HMI, "(too short)", 0);
            goto _end;
        }

        if (memcmp(&in[track_offset], "HMI-MIDITRACK", 13)) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_HMI, NULL, 0);
            goto _end;
        }

        track_header_length = READ_INT32LE(&in[track_offset + 0x57]);
        if (track_header_length >= insize - track_offset) {
            _WM_GLOBAL_ERROR(WM_ERR_NOT_HMI, "(too short)", 0);
            goto _end;
        }
        track_offset += track_header_length;

        ctx.list = NULL;
        if ((cvt_ret = ConvertTracktoList(&ctx, &in[track_offset], insize - track_offset)) < 0) {
            if (cvt_ret == -2) {
                _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
            } else {
                _WM_GLOBAL_ERROR(WM_ERR_CORUPT, "(too short)", 0);
            }
            DeleteEventList(ctx.list);
            goto _end;
        }
        ConvertListToMTrk(&ctx, ctx.list);
        DeleteEventList(ctx.list);
        ctx.list = NULL;

        if (ctx.oom) {
            _WM_GLOBAL_ERROR(WM_ERR_MEM, NULL, 0);
            goto _end;
        }
    }

    *out = ctx.dst;
    *outsize = ctx.dstsize - ctx.dstrem;
    return (0);

_end:
    free(ctx.dst);
    *out = NULL;
    *outsize = 0;
    return (ret);
}
