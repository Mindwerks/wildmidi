/* Assert-based smoke test for the Mtsp/Mwa wave loader in mafm.c.
 *
 * Two things get exercised:
 *  - a file with NO Mtsu voice records but an Mtsp Mwa should engage the
 *    engine, so streaming-audio MA-7 files (First Love, Yamaha example.mmf)
 *    have somewhere for their single-note trigger to go instead of falling
 *    back to a GM patch that plays silence;
 *  - a file with neither must NOT engage it, so the "engage on Mtsp waves"
 *    clause stays gated on wave_count rather than on the file parsing.
 *
 * See docs/formats/SmafFileFormat.txt. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mafm.h"

/* Yamaha ADPCM "silence" byte: nibble 0x8 = -delta_min, the pattern real
 * Mwa waves open with.  Any byte works; using this one makes the wave
 * decode to something less than white noise if someone plays it. */
#define ADPCM_QUIET 0x80

/* Write a big-endian uint32 to p and return p + 4. */
static uint8_t *put_u32be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t) v;
    return p + 4;
}

/* Build a minimal MA-7 (format_type 0x03) container with a Mobile-standard
 * score track that carries an Mtsp holding one Mwa wave. */
static uint8_t *build_streaming_mmf(uint32_t *size_out) {
    /* Mwa1 body: 3-byte header (fmt, rate BE) + a chunk of ADPCM.  Any size
     * works; give it 32 bytes so mafm_add_wave_mwa can decode a full window. */
    static const uint8_t mwa_body[3 + 32] = {
        0x20, 0x1f, 0x40,           /* fmt=0x20, rate = 0x1F40 = 8000 Hz     */
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET,
        ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET, ADPCM_QUIET
    };
    /* Empty Mtsq: just an end-of-sequence meta. */
    static const uint8_t mtsq_body[] = { 0x00, 0xff, 0x2f, 0x00 };

    uint32_t mwa   = 8 + (uint32_t) sizeof(mwa_body);
    uint32_t mtsp  = 8 + mwa;
    uint32_t mtsq  = 8 + (uint32_t) sizeof(mtsq_body);
    uint32_t mtr   = 36 + mtsq + mtsp;         /* fmt-3 header is 36 bytes  */
    uint32_t total = 8 + 8 + mtr;
    uint8_t *b = (uint8_t *) calloc(total, 1);
    uint8_t *p = b;
    assert(b != NULL);

    memcpy(p, "MMMD", 4); p += 4;
    p = put_u32be(p, total - 8);

    memcpy(p, "MTR", 3); p += 3; *p++ = 0x07;   /* MA-7 track id byte       */
    p = put_u32be(p, mtr);

    *p++ = 0x03;                                /* format_type: SEQU        */
    *p++ = 0x00; *p++ = 0x02; *p++ = 0x02;      /* seqtype, tb_dur, tb_gate */
    p += 32;                                    /* 32-byte channel status   */

    memcpy(p, "Mtsq", 4); p += 4;
    p = put_u32be(p, (uint32_t) sizeof(mtsq_body));
    memcpy(p, mtsq_body, sizeof(mtsq_body));
    p += sizeof(mtsq_body);

    memcpy(p, "Mtsp", 4); p += 4;
    p = put_u32be(p, mwa);
    memcpy(p, "Mwa", 3); p += 3; *p++ = 0x01;   /* Mwa\x01 = wave number 1  */
    p = put_u32be(p, (uint32_t) sizeof(mwa_body));
    memcpy(p, mwa_body, sizeof(mwa_body));
    p += sizeof(mwa_body);

    assert(p == b + total);
    *size_out = total;
    return b;
}

/* Same shape, but no Mtsp - just a Mobile-standard score track.  Used as the
 * "engine must NOT engage" control: without a voice bank and without any
 * Mwa waves, MAFM has nothing to do and HasCustomVoices should say so. */
static uint8_t *build_empty_mmf(uint32_t *size_out) {
    static const uint8_t mtsq_body[] = { 0x00, 0xff, 0x2f, 0x00 };
    uint32_t mtsq  = 8 + (uint32_t) sizeof(mtsq_body);
    uint32_t mtr   = 36 + mtsq;
    uint32_t total = 8 + 8 + mtr;
    uint8_t *b = (uint8_t *) calloc(total, 1);
    uint8_t *p = b;
    assert(b != NULL);
    memcpy(p, "MMMD", 4); p += 4;
    p = put_u32be(p, total - 8);
    memcpy(p, "MTR", 3); p += 3; *p++ = 0x07;
    p = put_u32be(p, mtr);
    *p++ = 0x03; *p++ = 0; *p++ = 2; *p++ = 2; p += 32;
    memcpy(p, "Mtsq", 4); p += 4;
    p = put_u32be(p, (uint32_t) sizeof(mtsq_body));
    memcpy(p, mtsq_body, sizeof(mtsq_body));
    p += sizeof(mtsq_body);
    assert(p == b + total);
    *size_out = total;
    return b;
}

int main(void) {
    uint8_t *mmf; uint32_t sz;

    /* Streaming case: Mtsp with a Mwa but no Mtsu voice records.  MAFM must
     * still engage - that is the fix.  Before, HasCustomVoices returned 0
     * for this shape and the file rendered as GM silence. */
    mmf = build_streaming_mmf(&sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 1);
    free(mmf);

    /* Control: no voices AND no waves.  MAFM must NOT engage.  Belt-and-
     * braces check that the new "engage if Mtsp waves exist" clause is
     * gated on wave_count > 0, not on file existence. */
    mmf = build_empty_mmf(&sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 0);
    free(mmf);

    printf("smaf_mtsp ok\n");
    return 0;
}
