/* Assert-based test for the MA-7 wave-delivery record loader in mafm.c.
 *
 * A "43 79 08 7F 23" record in Mtsu carries a sampled wave:
 *
 *     43 79 08 7F 23 [waveId] [00] [4-bit Yamaha ADPCM, low nibble first]
 *
 * Two things are exercised:
 *  - a file whose only Mtsu content is such a record must engage the MAFM
 *    engine, i.e. the wave really lands in the bank;
 *  - the same file with the sub-id changed to an unknown one must NOT engage,
 *    so engagement is attributable to the 7F 23 loader rather than to the mere
 *    presence of a setup record.
 *
 * The record is deliberately longer than 127 bytes, which is what a real wave
 * looks like.  That length only fits Mtsu's variable-length quantity, so this
 * doubles as the regression test for reading <len> as a VLQ instead of a
 * single byte -- with a one-byte read the length is misparsed, the wave never
 * loads, and the first assert fails.
 *
 * See docs/formats/SmafFileFormat.txt. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mafm.h"

/* An ADPCM byte that is not 0xF0, so a mis-parsing walk cannot mistake the
 * payload for the start of another exclusive record. */
#define ADPCM_FILL 0x88
#define ADPCM_LEN  200          /* > 127, so the length needs a 2-byte VLQ */

/* Write a big-endian uint32 to p and return p + 4. */
static uint8_t *put_u32be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t) v;
    return p + 4;
}

/* Build a minimal MA-7 (format_type 0x03) container whose score track carries
 * an Mtsu holding one wave-delivery record with the given family/sub-id.
 * family 0x08 + sub 0x23 is MA-7, 0x07 + 0x03 MA-5, 0x06 + 0x03 MA-3. */
static uint8_t *build_wave_mmf_family(uint8_t family, uint8_t sub_id,
                                      uint32_t *size_out);

static uint8_t *build_wave_mmf(uint8_t sub_id, uint32_t *size_out) {
    return build_wave_mmf_family(0x08, sub_id, size_out);
}

static uint8_t *build_wave_mmf_family(uint8_t family, uint8_t sub_id,
                                      uint32_t *size_out) {
    static const uint8_t mtsq_body[] = { 0x00, 0xff, 0x2f, 0x00 };

    /* 43 79 08 7F <sub> <waveId> <00> + ADPCM + F7 */
    uint32_t payload = 7 + ADPCM_LEN + 1;
    /* f0 + 2-byte VLQ + payload */
    uint32_t mtsu_body = 1 + 2 + payload;
    uint32_t mtsu  = 8 + mtsu_body;
    uint32_t mtsq  = 8 + (uint32_t) sizeof(mtsq_body);
    uint32_t mtr   = 36 + mtsu + mtsq;          /* fmt-3 header is 36 bytes */
    uint32_t total = 8 + 8 + mtr;
    uint8_t *b = (uint8_t *) calloc(total, 1);
    uint8_t *p = b;
    uint32_t i;
    assert(b != NULL);
    assert(payload >= 128 && payload <= 16383);  /* must need a 2-byte VLQ */

    memcpy(p, "MMMD", 4); p += 4;
    p = put_u32be(p, total - 8);

    memcpy(p, "MTR", 3); p += 3; *p++ = 0x07;   /* MA-7 track id byte       */
    p = put_u32be(p, mtr);

    *p++ = 0x03;                                /* format_type: SEQU        */
    *p++ = 0x00; *p++ = 0x02; *p++ = 0x02;      /* seqtype, tb_dur, tb_gate */
    p += 32;                                    /* 32-byte channel status   */

    memcpy(p, "Mtsu", 4); p += 4;
    p = put_u32be(p, mtsu_body);
    *p++ = 0xf0;
    *p++ = (uint8_t)(0x80 | (payload >> 7));    /* VLQ length, 2 bytes      */
    *p++ = (uint8_t)(payload & 0x7f);
    *p++ = 0x43; *p++ = 0x79; *p++ = family;    /* Yamaha, MA generation    */
    *p++ = 0x7f; *p++ = sub_id;
    *p++ = 0x05;                                /* waveId 5                 */
    *p++ = 0x00;                                /* reserved / format byte   */
    /* MA-3 payloads are 7-bit packed, so keep the fill inside 0..0x7f for
     * that family; MA-5/MA-7 store raw bytes and may exceed 0x7f. */
    for (i = 0; i < ADPCM_LEN; i++)
        *p++ = (family == 0x06) ? (ADPCM_FILL & 0x7f) : ADPCM_FILL;
    *p++ = 0xf7;

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

    /* A 7F 23 wave record is the file's only content.  MAFM must engage,
     * which it can only do if the VLQ length was read correctly and the
     * record decoded into the wave bank. */
    mmf = build_wave_mmf(0x23, &sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 1);
    free(mmf);

    /* Control: identical file, unknown sub-id.  Nothing should load, so the
     * engagement above is down to the 7F 23 loader and not to the walk simply
     * finding a record. */
    mmf = build_wave_mmf(0x2f, &sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 0);
    free(mmf);

    /* MA-5 spells the same record "43 79 07 7F 03". */
    mmf = build_wave_mmf_family(0x07, 0x03, &sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 1);
    free(mmf);

    /* MA-3 spells it "43 79 06 7F 03" and 7-bit packs the payload, so it goes
     * through _WM_MAFM_Unpack7() before the ADPCM decoder. */
    mmf = build_wave_mmf_family(0x06, 0x03, &sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 1);
    free(mmf);

    /* MA-3's sub-id under the MA-7 family id is not a wave record, so the
     * family check has to be part of the match rather than the sub-id alone. */
    mmf = build_wave_mmf_family(0x08, 0x03, &sz);
    assert(_WM_MAFM_HasCustomVoices(mmf, sz) == 0);
    free(mmf);

    printf("smaf_7f23 ok\n");
    return 0;
}
