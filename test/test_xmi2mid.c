/* assert-based test for _WM_xmi2midi(), the XMI to MIDI converter.
 *
 * Regression guard for issue #301:
 * 1. Every XMI sequence gets its own division from its tempo, but the
 *    merged type-2 MIDI wrote track 0's for all of them, so the others
 *    played at the wrong speed.  Below, sequence A has no tempo (division
 *    60) and B has 500000 us (division 180): the file must carry 180 and
 *    A's ticks must be scaled by 3.
 * 2. A bare FORM XMID (no XDIR/CAT around it) was rejected as "Not an xmi
 *    file".  It must convert, byte-identical to the same sequence wrapped. */
#undef NDEBUG /* the asserts are the test; keep them in a Release build */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xmi2mid.h"

/* note on, duration 1 tick (note off at 3), ten ticks, end of track */
#define SEQ_A 0x90, 60, 100, 0x01, 0x0a, 0xff, 0x2f, 0x00
/* the same after a 500000 us tempo */
#define SEQ_B 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20, SEQ_A

static const uint8_t two_seqs[] = {
    'F','O','R','M',  0,0,0,14,
    'X','D','I','R',
    'I','N','F','O',  0,0,0,2,
    2,0,
    'C','A','T',' ',  0,0,0,66,
    'X','M','I','D',
    'F','O','R','M',  0,0,0,20,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,8,  SEQ_A,
    'F','O','R','M',  0,0,0,26,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,14, SEQ_B
};

static const uint8_t wrapped[] = {
    'F','O','R','M',  0,0,0,14,
    'X','D','I','R',
    'I','N','F','O',  0,0,0,2,
    1,0,
    'C','A','T',' ',  0,0,0,32,
    'X','M','I','D',
    'F','O','R','M',  0,0,0,20,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,8,  SEQ_A
};

static const uint8_t bare[] = {
    'F','O','R','M',  0,0,0,20,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,8,  SEQ_A
};

/* Extreme rescales: sequence A at division 1 (tempo 2778 us) next to B
 * at division 6039 (tempo 0xffffff us) multiplies A's ticks by 6039.  A
 * waits each of waits[] (in XMI delay units, x3 in ticks), with a
 * controller event between them.  Builds the XMI into buf and returns its size. */
static const uint8_t seq_b_evnt[] = {
    'F','O','R','M',  0,0,0,26,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,14,
    0xff, 0x51, 0x03, 0xff, 0xff, 0xff, SEQ_A
};

static uint8_t *put4(uint8_t *p, const char *id, uint32_t len) {
    memcpy(p, id, 4);
    p[4] = len >> 24; p[5] = len >> 16; p[6] = len >> 8; p[7] = len;
    return p + 8;
}

static uint32_t build_rescale(uint8_t *buf, const uint32_t *waits, int n) {
    uint8_t evnt[2048], *e = evnt, *p = buf;
    uint32_t evlen, w;
    int i;

    e += 6;
    memcpy(evnt, "\xff\x51\x03\x00\x0a\xda", 6);
    for (i = 0; i < n; i++) {
        for (w = waits[i]; w; w -= *e++) /* delays are summed bytes < 0x80 */
            *e = (w > 127) ? 127 : w;
        if (i < n - 1) { /* the last wait ends at the end of track */
            *e++ = 0xb0; *e++ = 7; *e++ = 100;
        }
    }
    *e++ = 0xff; *e++ = 0x2f; *e++ = 0x00;
    if ((e - evnt) & 1)
        *e++ = 0; /* pad to even after the end of track */
    evlen = (uint32_t)(e - evnt);
    assert(evlen <= sizeof(evnt));

    p = put4(p, "FORM", 14);
    memcpy(p, "XDIR", 4); p += 4;
    p = put4(p, "INFO", 2);
    *p++ = 2; *p++ = 0;
    p = put4(p, "CAT ", 4 + 8 + 12 + evlen + sizeof(seq_b_evnt));
    memcpy(p, "XMID", 4); p += 4;
    p = put4(p, "FORM", 12 + evlen);
    memcpy(p, "XMID", 4); p += 4;
    p = put4(p, "EVNT", evlen);
    memcpy(p, evnt, evlen); p += evlen;
    memcpy(p, seq_b_evnt, sizeof(seq_b_evnt)); p += sizeof(seq_b_evnt);
    return (uint32_t)(p - buf);
}

static int convert_rescale(const uint32_t *waits, int n) {
    uint8_t xmi[2200], *mid;
    uint32_t size;
    int ret = _WM_xmi2midi(xmi, build_rescale(xmi, waits, n), &mid, &size,
                           XMIDI_CONVERT_NOCONVERSION);
    if (ret == 0)
        free(mid);
    else
        assert(mid == NULL && size == 0);
    return ret;
}

/* delta of the end of track event closing the MTrk at p; returns the next */
static const uint8_t *eot_delta(const uint8_t *p, uint8_t *delta) {
    uint32_t len = ((uint32_t)p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    const uint8_t *end = p + 8 + len;
    assert(!memcmp(p, "MTrk", 4));
    assert(!memcmp(end - 3, "\xff\x2f\x00", 3));
    *delta = end[-4]; /* single-byte VLQ in these files */
    return end;
}

int main(void) {
    uint8_t *mid, *mid2;
    uint32_t size, size2;
    const uint8_t *p;
    uint8_t delta;

    assert(_WM_xmi2midi(two_seqs, sizeof(two_seqs), &mid, &size,
                        XMIDI_CONVERT_NOCONVERSION) == 0);
    assert(mid[9] == 2 && mid[11] == 2);       /* format 2, two tracks */
    assert(mid[12] == 0 && mid[13] == 180);    /* the finer division */
    p = eot_delta(mid + 14, &delta);
    assert(delta == 81);                       /* A: 3 -> 30 ticks, x3 */
    p = eot_delta(p, &delta);
    assert(delta == 27);                       /* B: untouched */
    assert(p == mid + size);
    free(mid);

    assert(_WM_xmi2midi(wrapped, sizeof(wrapped), &mid, &size,
                        XMIDI_CONVERT_NOCONVERSION) == 0);
    assert(_WM_xmi2midi(bare, sizeof(bare), &mid2, &size2,
                        XMIDI_CONVERT_NOCONVERSION) == 0);
    assert(size == size2 && !memcmp(mid, mid2, size));
    free(mid);
    free(mid2);

    {
        /* 44577 ticks -> 269200503: one delta past the SMF limit */
        static const uint32_t one_long[] = { 14859 };
        /* two 22500 tick waits -> 135877500 each: valid deltas although
           the absolute time ends past the limit */
        static const uint32_t two_valid[] = { 7500, 7500 };
        /* nine 40005 tick waits -> 241590195 each: valid deltas adding up
           past INT32_MAX */
        static const uint32_t past_int32[] = {
            13335, 13335, 13335, 13335, 13335, 13335, 13335, 13335, 13335
        };
        assert(convert_rescale(one_long, 1) == -1);
        assert(convert_rescale(two_valid, 2) == 0);
        assert(convert_rescale(past_int32, 9) == -1);
    }
    return 0;
}
