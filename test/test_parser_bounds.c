/* assert-based test for parser over-reads on truncated input.
 *
 * Each input is copied into an exact-size heap buffer, so a read one byte
 * past the end is caught by AddressSanitizer (build with -fsanitize=address).
 * Without ASan the test still checks the parsers accept or reject cleanly.
 *
 * GHSA-wf5x-qm25-rf67: type-0 MIDI track of only VLQ continuation bytes.
 * GHSA-98xh-c4wm-qxq9, GHSA-7hpj-m84x-rmvx: MUS score with no end marker.
 * GHSA-7hpj-m84x-rmvx (HMP half): last HMP chunk ends mid-event or mid-delta,
 * or declares a length shorter than its own header. */
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
#include "f_hmp.h"

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

/* HMP chunks: number, length, track, delta (0x80 = 0), then events */
static const uint8_t hmp_delta_eof[] = {
    0,0,0,0, 17,0,0,0, 0,0,0,0, 0x80, 0x90,0x3c,0x40, 0x00
};
static const uint8_t hmp_event_eof[] = {
    0,0,0,0, 15,0,0,0, 0,0,0,0, 0x80, 0xb0,0x6e
};
static const uint8_t hmp_len_underflow[] = {
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0x80, 0x90
};
static const uint8_t hmp_ok[] = {
    0,0,0,0, 20,0,0,0, 0,0,0,0, 0x80, 0x90,0x3c,0x40, 0x8a, 0xff,0x2f,0x00
};

/* minimal 776-byte HMP header with one chunk appended */
static struct _mdi *parse_hmp(const uint8_t *chunk, uint32_t len) {
    uint32_t size = 776 + len;
    uint8_t *exact = calloc(1, size);
    struct _mdi *mdi;
    assert(exact != NULL);
    memcpy(exact, "HMIMIDIP", 8);
    exact[48] = 1;    /* chunks */
    exact[56] = 120;  /* bpm */
    memcpy(exact + 776, chunk, len);
    mdi = _WM_ParseNewHmp(exact, size);
    free(exact);
    return mdi;
}

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

    assert(parse_hmp(hmp_delta_eof, sizeof(hmp_delta_eof)) == NULL);
    assert(parse_hmp(hmp_event_eof, sizeof(hmp_event_eof)) == NULL);
    assert(parse_hmp(hmp_len_underflow, sizeof(hmp_len_underflow)) == NULL);
    mdi = parse_hmp(hmp_ok, sizeof(hmp_ok));
    assert(mdi != NULL);
    _WM_freeMDI(mdi);

    mdi = parse(_WM_ParseNewMus, mus_ok, sizeof(mus_ok));
    assert(mdi != NULL);
    _WM_freeMDI(mdi);
    return 0;
}
