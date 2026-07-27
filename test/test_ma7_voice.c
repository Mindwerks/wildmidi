/* assert-based smoke test for the MA-7 Mtsu voice-exclusive decode
 * (_WM_MAFM_ParseVoiceExclusive, src/mafm/smaf_voice.c).
 *
 * MA-7 stores its operators in the chip's native 10-byte layout rather than
 * the 7-byte packed VM35 one the older chips use; the decoder undoes the
 * shuffle Yamaha's middleware applies.  See docs/formats/SmafFileFormat.txt.
 * The bytes below are the pc=0x0a voice out of AB00221GM7.MMF. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "mafm/smaf_voice.h"

/* the exclusive payload, i.e. what sits between the F0 length and the F7 */
static const uint8_t ma7_voice[] = {
    0x43, 0x79, 0x08, 0x7f, 0x21,   /* Yamaha, MA-7, voice-set sub-id       */
    0x7c, 0x02, 0x0a, 0x00,         /* bank MSB/LSB, program, drum note     */
    0x00,                           /* voice type: 0 = FM                   */
    0x00,                           /* reserved                             */
    0x00, 0x79, 0x40,               /* 3 global bytes; alg = 0x40 & 7 = 0    */
    /* operator 0, native layout: bytes 5,7,8 are the reserved zeroes */
    0x33, 0x3b, 0x94, 0x70, 0x44, 0x00, 0x41, 0x00, 0x00, 0x90,
    /* operator 1 */
    0x22, 0x44, 0xdf, 0x02, 0x41, 0x00, 0x00, 0x00, 0x00, 0x10
};

/* form 0x02: 4 operators (alg = 0x4d & 7 = 5) plus a 16-byte trailer */
static const uint8_t ma7_form02[] = {
    0x43, 0x79, 0x08, 0x7f, 0x21,
    0x7c, 0x01, 0x19, 0x00, 0x00,
    0x02,                           /* form 0x02 */
    0x00, 0x79, 0x4d,
    0x42, 0x65, 0xf6, 0x60, 0x03, 0x08, 0x20, 0x1a, 0x27, 0xa4,
    0x52, 0x6d, 0xf2, 0x1e, 0x03, 0x08, 0x00, 0x1e, 0x36, 0x14,
    0x13, 0x44, 0xf8, 0x5a, 0x03, 0x0a, 0x05, 0x0e, 0xca, 0x54,
    0x13, 0x51, 0xe6, 0x18, 0x03, 0x02, 0x20, 0x1e, 0x36, 0x14,
    0x06, 0x08, 0x1f, 0xf8, 0x1f, 0xf8, 0x1d, 0x3c,
    0x1b, 0x45, 0x1c, 0xfd, 0x1f, 0x95, 0x10, 0x13
};

/* form 0x03: a sampled voice - body starts with the PCM rate 0x3e94 */
static const uint8_t ma7_form03[] = {
    0x43, 0x79, 0x08, 0x7f, 0x21,
    0x7c, 0x01, 0x00, 0x80, 0x34,
    0x03,                           /* form 0x03 */
    0x3e, 0x94, 0x78, 0x8b, 0x13, 0x53, 0xf2, 0x2c, 0x01, 0x0b,
    0x00, 0x00, 0x08, 0x2a, 0x0d, 0x6e, 0x0c, 0x08, 0x1e, 0x78,
    0x1e, 0x78, 0x1e, 0x78, 0x1d, 0x4c, 0x1d, 0x4c, 0x1f, 0x9d,
    0x1f, 0x00, 0x96
};

int main(void) {
    struct mafm_parsed_voice v;
    const struct mafm_op_patch *m, *c;

    _WM_MAFM_ParseVoiceExclusive(ma7_voice, (uint32_t)sizeof(ma7_voice), &v);

    assert(v.valid);
    assert(!v.is_pcm);
    assert(v.key.bank_msb == 0x7c);
    assert(v.key.bank_lsb == 0x02);
    assert(v.key.pc == 0x0a);
    assert(v.key.drum_note == 0x00);

    m = &v.patch.ops[0];            /* modulator */
    c = &v.patch.ops[1];            /* carrier   */

    /* The fields that live at the SAME offset in both layouts. */
    assert(m->ar == 9 && m->sl == 4);
    assert(m->rr == 3 && m->dr == 11);
    assert(m->tl == 28 && m->ksl == 0);
    assert(c->ar == 13 && c->sl == 15);
    assert(c->tl == 0);

    /* The two that MA-7 moves, and the whole point of this test: MULTI/DT come
     * from native byte 9 and WS/FB from native byte 6.  Reading them at the
     * VM35 offsets instead yields MULTI 0 on both operators - which is what
     * every wrong guess at this layout produced, and no real patch has. */
    assert(m->multi == 9 && m->dt == 0);
    assert(c->multi == 1 && c->dt == 0);
    assert(m->wave == 8 && m->fb == 1);
    assert(c->wave == 0 && c->fb == 0);

    /* A carrier at full level against an attenuated modulator: the sanity
     * check that the operator order came out right. */
    assert(c->tl < m->tl);

    /* Form 0x02: the same 10-byte operators, four of them, with a 16-byte
     * trailer the engine ignores.  From AccidentCafe7-Q-UTF8EN.mmf. */
    _WM_MAFM_ParseVoiceExclusive(ma7_form02, (uint32_t)sizeof(ma7_form02), &v);
    assert(v.valid);
    assert(v.patch.ops[0].multi == 10 && v.patch.ops[0].tl == 24);
    assert(v.patch.ops[1].multi == 1  && v.patch.ops[1].tl == 7);
    assert(v.patch.ops[2].multi == 5  && v.patch.ops[2].tl == 22);
    assert(v.patch.ops[3].multi == 1  && v.patch.ops[3].tl == 6);

    /* Forms 0x01/0x03/0x07 are sampled voices, not FM: decline them rather
     * than decode the bytes as operators. */
    _WM_MAFM_ParseVoiceExclusive(ma7_form03, (uint32_t)sizeof(ma7_form03), &v);
    assert(!v.valid);

    printf("ma7_voice ok\n");
    return 0;
}
