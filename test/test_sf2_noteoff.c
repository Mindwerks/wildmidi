/* assert-based test for the SF2 path's note-off deferral.
 *
 * Regression guard for issue #295: tsf releases a voice from wherever its
 * amplitude envelope has reached, so a note switched off while still in its
 * attack is silent.  XMI scores carry zero-length notes (the descending
 * triplet at 38s in SUNNYDAY.XMI is three of them), which the GUS mixer keeps
 * audible by holding the release back until the first envelope stage has run.
 *
 * The soundfont below is built here rather than shipped: one preset, one
 * instrument, one looping square-wave sample, and a one second attack.  With
 * an attack that long a note released on the same sample can only be heard if
 * the release was deferred, so silence is an unambiguous failure. */
#undef NDEBUG /* the asserts are the test; keep them in a Release build */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "wildmidi_lib.h"
#include "internal_midi.h"
#include "sf2.h"

#define SAMPLE_FRAMES 128
#define RENDER_RATE   32072

/* SF2 generator operators, from the spec's list */
#define GEN_ATTACKVOLENV   34
#define GEN_RELEASEVOLENV  38
#define GEN_INSTRUMENT     41
#define GEN_SAMPLEID       53
#define GEN_SAMPLEMODES    54

static uint8_t *sf2;
static uint32_t sf2_len;

static void put(const void *src, uint32_t len) {
    memcpy(sf2 + sf2_len, src, len);
    sf2_len += len;
}

static void put16(uint16_t v) {
    uint8_t b[2];
    b[0] = (uint8_t)(v & 0xff); b[1] = (uint8_t)(v >> 8);
    put(b, 2);
}

static void put32(uint32_t v) {
    uint8_t b[4];
    b[0] = (uint8_t)(v & 0xff);   b[1] = (uint8_t)((v >> 8) & 0xff);
    b[2] = (uint8_t)((v >> 16) & 0xff); b[3] = (uint8_t)(v >> 24);
    put(b, 4);
}

/* a 20 byte NUL padded name field */
static void put_name(const char *s) {
    char name[20];
    memset(name, 0, sizeof(name));
    strncpy(name, s, sizeof(name) - 1);
    put(name, sizeof(name));
}

/* chunk headers are back-patched once the body length is known */
static uint32_t begin_chunk(const char *id) {
    put(id, 4);
    put32(0);
    return sf2_len; /* body start */
}

static void end_chunk(uint32_t body_start) {
    uint32_t len = sf2_len - body_start;
    uint32_t at = body_start - 4;
    sf2[at]     = (uint8_t)(len & 0xff);
    sf2[at + 1] = (uint8_t)((len >> 8) & 0xff);
    sf2[at + 2] = (uint8_t)((len >> 16) & 0xff);
    sf2[at + 3] = (uint8_t)(len >> 24);
}

static uint32_t begin_list(const char *type) {
    uint32_t body = begin_chunk("LIST");
    put(type, 4);
    return body;
}

/* attack_tc/release_tc are timecents: seconds == 2^(tc/1200) */
static void build_sf2(int16_t attack_tc, int16_t release_tc) {
    uint32_t riff, list, chunk;
    int i;

    sf2 = (uint8_t *) malloc(4096 + SAMPLE_FRAMES * 2);
    assert(sf2 != NULL);
    sf2_len = 0;

    riff = begin_chunk("RIFF");
    put("sfbk", 4);

    list = begin_list("INFO");
    chunk = begin_chunk("ifil"); put16(2); put16(1); end_chunk(chunk);
    chunk = begin_chunk("isng"); put("EMU8000\0", 8); end_chunk(chunk);
    chunk = begin_chunk("INAM"); put("wmtest\0\0", 8); end_chunk(chunk);
    end_chunk(list);

    list = begin_list("sdta");
    chunk = begin_chunk("smpl");
    for (i = 0; i < SAMPLE_FRAMES; i++) {
        put16((uint16_t)(int16_t)(i < SAMPLE_FRAMES / 2 ? 16000 : -16000));
    }
    for (i = 0; i < 46; i++) put16(0); /* spec's trailing zero padding */
    end_chunk(chunk);
    end_chunk(list);

    list = begin_list("pdta");

    chunk = begin_chunk("phdr");
    put_name("wmtest"); put16(0); put16(0); put16(0); put32(0); put32(0); put32(0);
    put_name("EOP");    put16(0); put16(0); put16(1); put32(0); put32(0); put32(0);
    end_chunk(chunk);

    chunk = begin_chunk("pbag");
    put16(0); put16(0);
    put16(1); put16(0); /* terminal */
    end_chunk(chunk);

    chunk = begin_chunk("pmod");
    put16(0); put16(0); put16(0); put16(0); put16(0); /* terminal */
    end_chunk(chunk);

    chunk = begin_chunk("pgen");
    put16(GEN_INSTRUMENT); put16(0);
    put16(0); put16(0); /* terminal */
    end_chunk(chunk);

    chunk = begin_chunk("inst");
    put_name("wmtest"); put16(0);
    put_name("EOI");    put16(1);
    end_chunk(chunk);

    chunk = begin_chunk("ibag");
    put16(0); put16(0);
    put16(4); put16(0); /* terminal, after the four generators below */
    end_chunk(chunk);

    chunk = begin_chunk("imod");
    put16(0); put16(0); put16(0); put16(0); put16(0); /* terminal */
    end_chunk(chunk);

    chunk = begin_chunk("igen");
    put16(GEN_ATTACKVOLENV);  put16((uint16_t)attack_tc);
    put16(GEN_RELEASEVOLENV); put16((uint16_t)release_tc);
    put16(GEN_SAMPLEMODES);   put16(1); /* loop continuously */
    put16(GEN_SAMPLEID);      put16(0); /* must come last in the zone */
    put16(0); put16(0);                 /* terminal */
    end_chunk(chunk);

    chunk = begin_chunk("shdr");
    put_name("wmtest");
    put32(0); put32(SAMPLE_FRAMES); put32(0); put32(SAMPLE_FRAMES);
    put32(44100);
    put16(60); /* originalPitch 60, pitchCorrection 0 */
    put16(0); put16(1); /* sampleLink, sampleType == monoSample */
    put_name("EOS");
    put32(0); put32(0); put32(0); put32(0); put32(0); put16(0); put16(0); put16(0);
    end_chunk(chunk);

    end_chunk(list);
    end_chunk(riff);
}

static struct _mdi mdi;

static void send(void *synth, uint16_t evtype, uint8_t ch, uint32_t value) {
    struct _event event;
    memset(&event, 0, sizeof(event));
    event.evtype = evtype;
    event.event_data.channel = ch;
    event.event_data.data.value = value;
    _WM_SF2_Event(synth, &mdi, &event);
}

/* peak of frames rendered after a note on/off pair separated by `gap` frames */
static int32_t render_note(uint32_t gap, uint32_t frames) {
    uint32_t held = (gap > frames) ? gap : frames;
    int32_t *buf = (int32_t *) calloc(held * 2, sizeof(int32_t));
    void *synth = _WM_SF2_NewSynth(RENDER_RATE);
    int32_t peak = 0;
    uint32_t i;

    assert(buf != NULL);
    assert(synth != NULL);

    send(synth, ev_note_on, 0, (60 << 8) | 100);
    if (gap) _WM_SF2_Render(synth, buf, gap);
    send(synth, ev_note_off, 0, (60 << 8));
    memset(buf, 0, held * 2 * sizeof(int32_t)); /* peak of the tail only */
    _WM_SF2_Render(synth, buf, frames);

    for (i = 0; i < frames * 2; i++) {
        int32_t v = buf[i] < 0 ? -buf[i] : buf[i];
        if (v > peak) peak = v;
    }
    _WM_SF2_FreeSynth(synth);
    free(buf);
    return peak;
}

int main(void) {
    int32_t deferred, sustained;

    memset(&mdi, 0, sizeof(mdi));
    _WM_MasterVolume = 948; /* WildMidi_Init()'s default */

    /* one second attack (2^0 s), half second release (2^(-1200/1200) s) */
    build_sf2(0, -1200);
    assert(_WM_SF2_Load(sf2, sf2_len) == 0);
    assert(_WM_SF2_Active());

    /* Note off on the very sample the note started: only audible because the
     * release waits for the attack.  Half a second in, the envelope is still
     * climbing, so this is well clear of any release tail. */
    deferred = render_note(0, RENDER_RATE / 2);
    assert(deferred > 0);

    /* and it is the same note, not some artefact: holding the key for the
     * same span gives the same envelope, so the two peaks must agree */
    sustained = render_note(RENDER_RATE / 2, 1);
    assert(deferred >= sustained - (sustained / 8));
    assert(deferred <= sustained + (sustained / 8));

    _WM_SF2_Unload();
    assert(!_WM_SF2_Active());
    free(sf2);
    return 0;
}
