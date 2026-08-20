/* assert-based test for zero-length XMI notes.
 *
 * Regression guard for issue #295: an XMI note on carries its duration in
 * ticks, and _WM_ParseNewXmi() counts that duration down to decide when to
 * emit the note off.  A duration of 0 is indistinguishable from "this key is
 * not sounding" in that countdown, so the note off used to be dropped and the
 * note hung until the next note on the same key released it - the descending
 * triplet at 38s in TES: Arena's SUNNYDAY.XMI rings for 1.6s that way.
 *
 * The file below is the smallest XMI that carries one: a single note on with
 * a zero duration, a delta, and an end of track. */
#undef NDEBUG /* the asserts are the test; keep them in a Release build */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "f_xmidi.h"

#define NOTE 60
#define VEL  100

static const uint8_t xmi[] = {
    /* XDIR form: 14 bytes after the length field */
    'F','O','R','M',  0,0,0,14,
    'X','D','I','R',
    'I','N','F','O',  0,0,0,2,
    1,0,                                /* one XMID form follows */

    'C','A','T',' ',  0,0,0,28,
    'X','M','I','D',

    'F','O','R','M',  0,0,0,20,
    'X','M','I','D',

    'E','V','N','T',  0,0,0,8,
    0x90, NOTE, VEL, 0x00,              /* note on, duration 0 ticks */
    0x0a,                               /* ten ticks pass */
    0xff, 0x2f, 0x00                    /* end of track */
};

int main(void) {
    struct _mdi *mdi;
    struct _event *ev;
    uint32_t on_at = 0, off_at = 0;
    int seen_on = 0, seen_off = 0;
    uint32_t samples = 0;

    _WM_SampleRate = 32072;

    mdi = _WM_ParseNewXmi(xmi, (uint32_t)sizeof(xmi));
    assert(mdi != NULL);

    for (ev = mdi->events; ev->do_event != NULL; ev++) {
        if (ev->evtype == ev_note_on
            && ev->event_data.data.value == ((NOTE << 8) | VEL)) {
            assert(!seen_on); /* one note on, so any second is a parser bug */
            seen_on = 1;
            on_at = samples;
        } else if (ev->evtype == ev_note_off
                   && ((ev->event_data.data.value >> 8) & 0x7f) == NOTE) {
            assert(seen_on);
            seen_off = 1;
            off_at = samples;
        }
        samples += ev->samples_to_next;
    }

    /* the note off has to exist at all... */
    assert(seen_off);
    /* ...and land on the same sample as the note on, since that is the
       duration the file asked for */
    assert(off_at == on_at);

    _WM_freeMDI(mdi);
    return 0;
}
