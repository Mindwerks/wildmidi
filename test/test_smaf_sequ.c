/* assert-based smoke test for the MA-7 "SEQU" (score format_type 0x03) decode
 * in smaf2mid.c.  SEQU is Mobile Standard with the status byte repacked to
 * address 32 channels: bit 7 is the channel bank, bits 6-4 are the event type
 * (MIDI status nibble 0x8+n), bits 3-0 are the low channel nibble.
 * See docs/formats/SmafFileFormat.txt. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern int _WM_smaf2midi(const uint8_t *in, uint32_t insize,
                         uint8_t **out, uint32_t *outsize);

/* The Mtsq event stream under test.  Each record is [duration VLQ][event]. */
static const uint8_t seq[] = {
    0x00, 0x30, 0x07, 0x64,       /* ch0  CC volume 100          -> b0 07 64 */
    0x00, 0xb1, 0x07, 0x50,       /* ch17 CC volume  80  (bit 7) -> b1 07 50 */
    0x00, 0x40, 0x05,             /* ch0  program 5              -> c0 05    */
    0x00, 0x60, 0x00, 0x50,       /* ch0  pitch bend 0,0x50      -> e0 00 50 */
    0x00, 0x11, 0x3c, 0x64, 0x0a, /* ch1  note 60 vel 100 gate 10           */
    0x14, 0x01, 0x3e, 0x0a,       /* ch1  note 62, running velocity 100     */
    0x00, 0xff, 0x2f, 0x00        /* end of sequence                        */
};

/* Build the smallest MMMD container that carries one MA-7 score track. */
static uint8_t *build_mmf(uint32_t *size_out) {
    uint32_t mtsq = 8 + (uint32_t)sizeof(seq);   /* "Mtsq" + size + body      */
    uint32_t mtr  = 36 + mtsq;                   /* 4 fixed + 32 chstat + sub */
    uint32_t total = 8 + 8 + mtr;                /* MMMD hdr + MTR hdr + body */
    uint8_t *b = (uint8_t *) calloc(total, 1);
    uint32_t p = 0;
    assert(b != NULL);

    memcpy(b + p, "MMMD", 4); p += 4;
    b[p++] = 0; b[p++] = 0;
    b[p++] = (uint8_t)((total - 8) >> 8); b[p++] = (uint8_t)(total - 8);

    memcpy(b + p, "MTR", 3); p += 3;
    b[p++] = 0x07;                              /* MA-7 score track id byte  */
    b[p++] = 0; b[p++] = 0;
    b[p++] = (uint8_t)(mtr >> 8); b[p++] = (uint8_t)mtr;

    b[p++] = 0x03;                              /* format_type: SEQU         */
    b[p++] = 0x00;                              /* sequence_type             */
    b[p++] = 0x02;                              /* timebase_dur:  4 ms/tick  */
    b[p++] = 0x02;                              /* timebase_gate: 4 ms/tick  */
    p += 32;                                    /* channel_status, all zero  */

    memcpy(b + p, "Mtsq", 4); p += 4;
    b[p++] = 0; b[p++] = 0;
    b[p++] = (uint8_t)(sizeof(seq) >> 8); b[p++] = (uint8_t)sizeof(seq);
    memcpy(b + p, seq, sizeof(seq)); p += (uint32_t)sizeof(seq);

    assert(p == total);
    *size_out = total;
    return b;
}

/* Does the emitted MIDI contain this exact byte run anywhere? */
static int contains(const uint8_t *hay, uint32_t n,
                    const uint8_t *needle, uint32_t m) {
    uint32_t i;
    if (m > n) return 0;
    for (i = 0; i + m <= n; i++)
        if (memcmp(hay + i, needle, m) == 0) return 1;
    return 0;
}

#define HAS(...) do { \
    static const uint8_t want[] = { __VA_ARGS__ }; \
    assert(contains(mid, midsize, want, (uint32_t)sizeof(want))); \
} while (0)

int main(void) {
    uint8_t *mmf, *mid = NULL;
    uint32_t mmfsize = 0, midsize = 0;

    mmf = build_mmf(&mmfsize);
    assert(_WM_smaf2midi(mmf, mmfsize, &mid, &midsize) == 0);
    assert(mid != NULL && midsize > 22);
    assert(memcmp(mid, "MThd", 4) == 0);

    /* 0x3n -> control change, and 0xb1 proves bit 7 is the channel bank and
     * not a MIDI status flag: channel 17 folds onto MIDI channel 1. */
    HAS(0xb0, 0x07, 0x64);
    HAS(0xb1, 0x07, 0x50);

    HAS(0xc0, 0x05);                    /* 0x4n -> program change */
    HAS(0xe0, 0x00, 0x50);              /* 0x6n -> pitch bend     */

    /* 0x1n carries an explicit velocity; 0x0n reuses the channel's running
     * velocity, so note 62 must come out at 100 as well. */
    HAS(0x91, 0x3c, 0x64);
    HAS(0x91, 0x3e, 0x64);

    /* Notes carry their own gate time, so each one must get a note-off. */
    HAS(0x81, 0x3c, 0x40);
    HAS(0x81, 0x3e, 0x40);

    free(mid);
    free(mmf);
    return 0;
}
