/* assert-based test for _WM_get_patch_data()'s bank resolution order.
 *
 * Regression guard for issue #295: a non-zero bank in a timidity.cfg is a
 * sparse overlay on bank 0 (eawpats' "bank 8" defines one program, "drumset 8"
 * one note), so a program the overlay does not define has to come from bank 0
 * and not from the nearest-patch search inside the overlay. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "patches.h"

struct _mdi;
extern struct _patch *_WM_get_patch_data(struct _mdi *mdi, uint16_t patchid);

static struct _patch b0_p0, b0_p24, b0_d38, b8_p80, b8_d54;

static void add(struct _patch *p, uint16_t patchid) {
    memset(p, 0, sizeof(*p));
    p->patchid = patchid;
    p->next = _WM_patch[patchid & 0x7F];
    _WM_patch[patchid & 0x7F] = p;
}

int main(void) {
    add(&b0_p0,  0x0000);       /* bank 0, program 0 */
    add(&b0_p24, 0x0018);       /* bank 0, program 24 */
    add(&b0_d38, 0x00a6);       /* drumset 0, note 38 (0x26 | 0x80) */
    add(&b8_p80, 0x0850);       /* bank 8, program 80 - the whole overlay */
    add(&b8_d54, 0x08b6);       /* drumset 8, note 54 - the whole overlay */

    /* exact hits win */
    assert(_WM_get_patch_data(NULL, 0x0018) == &b0_p24);
    assert(_WM_get_patch_data(NULL, 0x0850) == &b8_p80);
    assert(_WM_get_patch_data(NULL, 0x08b6) == &b8_d54);

    /* a program the overlay lacks comes from bank 0, not from the overlay */
    assert(_WM_get_patch_data(NULL, 0x0818) == &b0_p24);
    assert(_WM_get_patch_data(NULL, 0x08a6) == &b0_d38);

    /* a bank nothing defines still falls back to bank 0 (SMAF selects
       Yamaha's own 0x7c banks, which no GUS patch set has) */
    assert(_WM_get_patch_data(NULL, 0x7c00) == &b0_p0);

    /* only when bank 0 has no such program either does the nearest one
       stand in, rather than playing silence */
    assert(_WM_get_patch_data(NULL, 0x0017) == &b0_p24);
    assert(_WM_get_patch_data(NULL, 0x0819) == &b0_p24);

    return 0;
}
