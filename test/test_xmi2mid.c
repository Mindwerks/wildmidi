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

/* sequence A at division 1 (tempo 2778 us) waits 44577 ticks; B is at
 * division 6039 (tempo 0xffffff us).  Rescaled, A's end lands past the
 * SMF delta limit 0x0fffffff, so the conversion must fail cleanly. */
#define LONG_WAIT 117 /* bytes of 0x7f delay, 127 * 117 * 3 = 44577 ticks */
static const uint8_t too_long_head[] = {
    'F','O','R','M',  0,0,0,14,
    'X','D','I','R',
    'I','N','F','O',  0,0,0,2,
    2,0,
    'C','A','T',' ',  0,0,0,184,
    'X','M','I','D',
    'F','O','R','M',  0,0,0,138,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,126,
    0xff, 0x51, 0x03, 0x00, 0x0a, 0xda
};
static const uint8_t too_long_tail[] = {
    0xff, 0x2f, 0x00,
    'F','O','R','M',  0,0,0,26,
    'X','M','I','D',
    'E','V','N','T',  0,0,0,14,
    0xff, 0x51, 0x03, 0xff, 0xff, 0xff,
    0x90, 60, 100, 0x01, 0x01, 0xff, 0x2f, 0x00
};

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
        uint8_t too_long[sizeof(too_long_head) + LONG_WAIT + sizeof(too_long_tail)];
        memcpy(too_long, too_long_head, sizeof(too_long_head));
        memset(too_long + sizeof(too_long_head), 0x7f, LONG_WAIT);
        memcpy(too_long + sizeof(too_long_head) + LONG_WAIT,
               too_long_tail, sizeof(too_long_tail));
        assert(_WM_xmi2midi(too_long, sizeof(too_long), &mid, &size,
                            XMIDI_CONVERT_NOCONVERSION) == -1);
        assert(mid == NULL && size == 0);
    }
    return 0;
}
