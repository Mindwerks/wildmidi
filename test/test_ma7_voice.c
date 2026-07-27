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

    printf("ma7_voice ok\n");
    return 0;
}
