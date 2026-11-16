/* assert-based test for parser over-reads on truncated input.
 *
 * Each input is copied into an exact-size heap buffer, so a read one byte
 * past the end is caught by AddressSanitizer (build with -fsanitize=address).
 * Without ASan the test still checks the parsers accept or reject cleanly.
 *
 * GHSA-wf5x-qm25-rf67: type-0 MIDI track of only VLQ continuation bytes.
 * GHSA-98xh-c4wm-qxq9, GHSA-7hpj-m84x-rmvx: MUS score with no end marker. */
#undef NDEBUG /* the asserts are the test; keep them in a Release build */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "f_midi.h"
#include "f_mus.h"

static const uint8_t midi_type0_vlq[] = {
    'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,0x60,
    'M','T','r','k', 0,0,0,4, 0xff,0xff,0xff,0xff
};

/* song offset 0: the header itself decodes as no-delay note offs */
static const uint8_t mus_ofs0[] = {
    'M','U','S',0x1a, 0,0, 0,0, 0,0,
    0,0, 0,0, 0,0, 0,0,0,0
};

static const uint8_t mus_fastpath[] = {
    'M','U','S',0x1a, 8,0, 13,0, 1,0,
    0,0, 0,0, 0,0, 0x60, 0,0,0, 0,0,0,0
};

/* note off with a delay whose VLQ never terminates */
static const uint8_t mus_tick_vlq[] = {
    'M','U','S',0x1a, 4,0, 16,0, 1,0,
    0,0, 0,0, 0,0, 0x80,0x3c, 0xff,0xff
};

/* well-formed: one note on, a delay, end of song */
static const uint8_t mus_ok[] = {
    'M','U','S',0x1a, 6,0, 16,0, 1,0,
    0,0, 0,0, 0,0, 0x90,0x3c, 0x0a, 0x60, 0,0
};

static struct _mdi *parse(struct _mdi *(*fn)(const uint8_t *, uint32_t),
                          const uint8_t *data, uint32_t len) {
    uint8_t *exact = malloc(len);
    struct _mdi *mdi;
    assert(exact != NULL);
    memcpy(exact, data, len);
    mdi = fn(exact, len);
    free(exact);
    return mdi;
}

int main(void) {
    struct _mdi *mdi;

    _WM_SampleRate = 32072;

    assert(parse(_WM_ParseNewMidi, midi_type0_vlq, sizeof(midi_type0_vlq)) == NULL);

    /* truncated MUS ends the song where the data ends, like the loop does */
    mdi = parse(_WM_ParseNewMus, mus_ofs0, sizeof(mus_ofs0));
    if (mdi) _WM_freeMDI(mdi);
    mdi = parse(_WM_ParseNewMus, mus_fastpath, sizeof(mus_fastpath));
    if (mdi) _WM_freeMDI(mdi);
    mdi = parse(_WM_ParseNewMus, mus_tick_vlq, sizeof(mus_tick_vlq));
    if (mdi) _WM_freeMDI(mdi);

    mdi = parse(_WM_ParseNewMus, mus_ok, sizeof(mus_ok));
    assert(mdi != NULL);
    _WM_freeMDI(mdi);
    return 0;
}
