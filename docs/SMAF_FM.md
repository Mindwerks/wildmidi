# SMAF FM Synthesis Support (Design)

Status: **DESIGN / PROPOSAL** — no implementation yet.
Author: WildMIDI Developers, 2026.

## The problem

WildMIDI already reads Yamaha SMAF (`.mmf`) files, but only by converting the
score track to a Standard MIDI File (`smaf2mid.c`) and rendering it with the
General-MIDI wavetable synth.  For many real files this sounds wrong — "beeps,
flat, not musical" — because **the file's actual sound lives in data the GM
converter discards**:

* `Mtsu` (score-track setup) carries **custom FM voice definitions** as Yamaha
  voice-exclusives (`F0 43 ...`).  A program-change like "program 84" is an
  index into *these* voices, not a GM patch.  Sent to a GM synth it plays an
  unrelated instrument (e.g. "FX 6 goblins").
* `ATR` / `Awa` chunks carry **embedded ADPCM samples** (drums, phrases).

Faithful playback therefore needs an **FM synthesizer** (the MA-series chip)
plus an ADPCM decoder — not just a MIDI note stream.  The GM path stays as the
fallback for files that ship no custom voices (they reference the chip's ROM
voices, which are not public anyway).

Reference: `docs/formats/SmafFileFormat.txt` (container + score encodings).

## Why this cannot go through the SMF pipeline

The current flow is:

    .mmf --smaf2mid.c--> Standard MIDI File --f_midi--> _mdi events
        --> wavetable synth --> PCM

A Standard MIDI File has no way to express an FM operator patch.  The FM voice
data has nowhere to live in that pipeline.  An FM engine must render **PCM
directly** and have that PCM mixed into WildMIDI's output.

## The precedent: TinySoundFont (SF2)

WildMIDI already solved this exact shape of problem for SoundFonts.  It bundles
a self-contained third-party synth and renders its PCM into the mix buffer
instead of the wavetable output:

* `src/tsf/tsf.h`      — vendored single-header synth (upstream, unmodified).
* `src/sf2.c` / `include/sf2.h` — a thin WildMIDI wrapper: `_WM_SF2_*`.
* `src/wildmidi_lib.c` — when a soundfont is active the render loop calls
  `_WM_SF2_Event()` per event and `_WM_SF2_Render()` to fill the buffer
  (see the `mdi->sf2_synth` branches), bypassing the wavetable mixer.

The SF2 wrapper interface (the template to mirror):

    _WM_SF2_Magic / Load / Unload / Active     -- bank lifecycle
    _WM_SF2_NewSynth / FreeSynth / Reset       -- per-mdi instance
    _WM_SF2_Event(synth, mdi, event)           -- feed a MIDI event
    _WM_SF2_ActiveVoices(synth)                -- release-tail tracking
    _WM_SF2_Render(synth, out, frames)         -- stereo PCM into mix buffer

**An FM engine slots in the same way, behind a parallel `_WM_MAFM_*` wrapper.**

## Proposed approach: vendor the akustikrausch FM core

Rather than write an FM synth from scratch, vendor the one open,
permissively-licensed, ROM-free implementation that already works:

* Upstream: https://github.com/akustikrausch/yamaha-smaf-player
* License: **Apache-2.0** (clean per-file SPDX headers).  Apache-2.0 is FSF-
  approved compatible **into** (L)GPLv3 (one-way); WildMIDI is LGPLv3 (lib) /
  GPLv3 (player).  Incorporating it is allowed, provided we **preserve the
  Apache notices and add attribution** (a `NOTICE`/per-file header, and a line
  in the WildMIDI docs/credits).  Do NOT relicense the vendored files; keep
  their Apache headers and add WildMIDI's wrapper as separate LGPL files.

The upstream pieces and rough sizes:

| upstream file        | lines | role                                        |
|----------------------|-------|---------------------------------------------|
| `ma_fm_core.{h,cpp}` | ~560  | FM DSP: operators, EG, 2-op/4-op algorithms |
| `smaf_voice.{h,cpp}` | ~280  | VMA/VM35 voice-exclusive -> FM patch         |
| `yamaha_adpcm.h`     | ~70   | Yamaha 4-bit ADPCM codec                    |
| `ma_player.{h,cpp}`  | ~870  | event decoders + scheduler (we have our own) |
| `smaf_file.{h,cpp}`  | ~410  | container parser (we have our own)          |

We need the **synth** (fm core + voice decode + adpcm).  We do NOT need upstream's
container parser or event scheduler — `smaf2mid.c` / `f_smaf.c` already parse the
container and we drive events from our side.

### Vendoring choice: DECIDED — port the synth to C

The three synth modules (`ma_fm_core`, `smaf_voice`, `yamaha_adpcm`) are ported
to C89-ish C to match the rest of the library, so they build on every target in
the tree (mingw / os2 / djgpp / amiga) with no C++ toolchain required.  The port
preserves the upstream algorithm; each ported file keeps its Apache-2.0 SPDX
header and an attribution note (see NOTICE).  Divergence from upstream is the
accepted cost of a single-language, portable build.

We do NOT vendor upstream's container parser (`smaf_file`) or event scheduler
(`ma_player`) — `smaf2mid.c` / `f_smaf.c` already own that ground.

## Integration plan (mirrors SF2)

New files:

    src/mafm/ma_fm_core.*     vendored (Apache-2.0), FM DSP
    src/mafm/smaf_voice.*     vendored (Apache-2.0), voice-exclusive decode
    src/mafm/yamaha_adpcm.h   vendored (Apache-2.0), ADPCM
    src/mafm.c  include/mafm.h  WildMIDI wrapper (LGPL): the _WM_MAFM_* ABI

Wrapper interface (parallel to `sf2.h`):

    int   _WM_MAFM_HasCustomVoices(const uint8_t *smaf, uint32_t size);
          /* true if the file carries Mtsu/ATR voice or wave data worth
             synthesising; false -> use the existing GM conversion path. */
    void *_WM_MAFM_NewSynth(uint16_t rate);
    void  _WM_MAFM_FreeSynth(void *synth);
    void  _WM_MAFM_Reset(void *synth);
    int   _WM_MAFM_LoadVoices(void *synth, const uint8_t *smaf, uint32_t size);
          /* parse Mtsu voice-exclusives + ATR/Awa waves into the synth */
    void  _WM_MAFM_Event(void *synth, struct _mdi *mdi, struct _event *event);
    int   _WM_MAFM_ActiveVoices(void *synth);
    void  _WM_MAFM_Render(void *synth, int32_t *out, uint32_t frames);

Flow for a custom-voice SMAF file:

    .mmf --f_smaf.c--> (still) smaf2mid.c to get the note/timing event stream
        --> _mdi events, BUT tag the mdi so the render loop routes to _WM_MAFM_*
            instead of the wavetable synth
        --> _WM_MAFM_LoadVoices() binds Mtsu voices + Awa samples
        --> render loop: _WM_MAFM_Event per event, _WM_MAFM_Render to PCM

The mdi carries a new optional `void *mafm_synth` next to `sf2_synth`; the render
loop grows a third branch (wavetable | sf2 | mafm).  Program-change events map to
the bound FM voice rather than a GM patch; percussion / Awa notes trigger the
ADPCM one-shots.

Files that ship **no** custom voices keep the current behaviour unchanged
(`_WM_MAFM_HasCustomVoices` returns false -> GM conversion).

## Milestones

* **M0 — this design + licensing sign-off.**  Confirm Apache-2.0 vendoring is
  acceptable to the project and pick the vendoring option (C++ island vs C port).
* **M1 — ADPCM codec** (smallest, self-contained).  DONE: ported to
  `src/mafm/yamaha_adpcm.{c,h}`.  Verified decoding `Solid Soda`'s `Awa\x01`
  block (rate class from fmt2 nibble = 8000 Hz, LOW-nibble-first, ADPCM data
  begins 2 bytes into the wave body after [formatByte][fmt2]).  Output is a
  0.66 s one-shot with a clean attack-decay envelope (8191 -> 4324 -> 731
  mean-abs across the sample), i.e. a real percussion sample, not hash.
* **M2 — voice-exclusive decode.**  DONE: ported to `src/mafm/smaf_voice.{c,h}`
  (+ the patch structs and `_WM_MAFM_DefaultPatch` in `src/mafm/ma_fm_core.{c,h}`).
  Verified on `Solid Soda`'s `Mtsu`: all 6 custom voices decode (the MA-1/2 VMA
  `43 03` form), each `valid`, with bank/PC keys that match the score's program
  changes exactly — voice0 (bank2,pc84), voice1 (bank2,pc72), voice2 (bank2,pc71),
  voices3-5 (pc101 on banks 2/3/4).  FM fields are in range and distinct per
  voice, so the voice<->note binding will resolve.  Covers the MA-3 packed
  (`43 79 06`), MA-5 direct (`43 79 07` / `43 05 01`) and VMA (`43 03`) forms and
  PCM voices; only the VMA path is exercised so far (the samples are MA-1/2).
* **M3 — FM core, offline.**  DONE: ported to `src/mafm/ma_fm_core.{c,h}`
  (envelope, operator, voice; 8 connection algorithms; per-op feedback; voice
  LFO with vibrato/tremolo; KSR/KSL scaling; the built-in GM and drum
  approximation banks).  Verified by rendering `Solid Soda`'s decoded voices to
  WAV: pitch is correct (C5 note -> ~522 Hz measured vs 523 expected), envelopes
  have the right shape (voice 0 a fast pluck decaying by ~0.2 s, voice 3
  sustaining and ringing 0.17 s past key-off), voices retire cleanly (no stuck
  drones), and output peaks near full scale without clipping.  All three ported
  modules compile clean under `-std=c89 -pedantic -Wall -Wextra` (log2 replaced
  with a natural-log helper for C89), so they build on every target in the tree.
* **M4 — WildMIDI integration.**  DONE.  New `src/mafm.c` + `include/mafm.h`: the
  `_WM_MAFM_*` wrapper parses the file's Mtsu voice bank, keeps per-channel
  bank/program/volume/pitch state, allocates a 32-voice pool, and renders PCM.
  Wired in exactly like SF2: a `mafm_synth` field on `struct _mdi`;
  `f_smaf.c` creates it via `_WM_MAFM_NewSynth` when `_WM_MAFM_HasCustomVoices`
  is true (else the plain GM path); a `WM_GetOutput_MAFM` render loop (a copy of
  `WM_GetOutput_SF2` with the synth calls swapped) plus the dispatch, seek and
  free hooks in `wildmidi_lib.c` / `internal_midi.c`.  Build: `WANT_MAFM`
  option -> `WILDMIDI_MAFM` (CMake + build.zig), sources listed in both, `src`
  added to the include path.  Verified: `Solid Soda` renders 20.2 s of audio
  through the FM path (confirmed by the mono L==R render signature vs the GM
  wavetable's stereo), no clipping; a plain MIDI still renders stereo GM (no
  regression); tokenize test passes; both CMake and the Zig build (which has SF2
  OFF, MAFM ON -- proving the guards are independent) compile clean.
* **M5 — fidelity + coverage.**  IN PROGRESS.  DONE: the ATR sampled-drum path.
  `mafm.c` now decodes the `Awa` ADPCM wave bank and the `Atsq` trigger sequence
  at synth-creation time and plays the samples on a 16-slot PCM pool against the
  synth's own sample clock (a `mafm_pcm_tick` advanced once per output frame,
  summed with the FM voices).  Verified on `Solid Soda`: 2 waves, 9 triggers,
  drums land at the right times (transients at ~2.2/6.3/10.9/15/18.5 s match the
  decoded schedule); `Mars Mine` also renders.  Notes learned: the `Atsq` shares
  the score tick base (4 ms/tick here) -- don't misread the ATR header's own
  timebase field, a wrong value halves/doubles every hit; the `Awa` body is
  `[formatByte][fmt2][adpcm]` (data +2), LOW-nibble-first; ATR sub-chunks are
  found by scanning for known ids since the ATR header width varies.  Pending
  triggers are NOT counted as active voices (that could spin the end-of-song
  drain forever).  STILL OPEN: PCM voice-exclusives (`43 79` type!=0, distinct
  from ATR triggers), the MA-3/5 (`43 79 06/07`) FM voice forms (only VMA `43 03`
  exercised so far), smarter voice-stealing (currently slot 0), and per-voice
  pan.  NOTE: FM render is CPU-heavy (per-sample double math x 32 voices), ~tens
  of seconds to render a 20-30 s song offline.

## Non-goals (first pass)

* Bit-exact hardware emulation.  "Recognised, and it plays" (upstream's stance).
* The MA-7 SEQU score format (still declined in `smaf2mid.c`).
* MFi (`.mld`) — a separate format; see `docs/formats/MFIFileFormat.txt`.  Its FM
  playback would reuse the same engine later.

## Resolved during implementation

1. Build language: RESOLVED - the synth was PORTED TO C (not vendored as a C++
   TU), so it builds on every target with no C++ toolchain.  All three synth
   modules compile under -std=c89 -pedantic -Wall -Wextra.
2. Build gating: RESOLVED - gated behind the WANT_MAFM CMake option ->
   WILDMIDI_MAFM (default ON), mirrored in build.zig.  Size-constrained builds
   that leave it off get the stub (mafm.c compiles empty, like sf2.c does).
3. Voice binding: RESOLVED - the wrapper (mafm.c) keeps per-channel bank/program
   state (chan_bank / chan_program) from the ev_control_bank_select / ev_patch
   events and resolves the custom voice itself; smaf2mid.c stays generic.

## Still open (polish, non-blocking)

* PCM voice-exclusives (43 79 type!=0): sampled *instruments* (as opposed to the
  ATR drum triggers, which are done).  Parsed by smaf_voice.c but not played.
* MA-3 / MA-5 FM voice forms (43 79 06/07): decoded but UNTESTED - need MA-3/5
  sample files (the corpus on hand is MA-1/2 only).
* Voice-stealing is naive (always slot 0) for both the FM and PCM pools; dense
  material can glitch.  Replace with oldest/quietest selection.
* Per-voice pan is ignored (the render sums mono into both channels).
* Loop (WM_MO_LOOP) and mid-song seek through the FM path are wired (Reset hooks
  in wildmidi_lib.c) but NOT yet verified; the PCM trigger cursor (s->cursor in
  mafm.c) in particular may not survive a reset/seek and needs testing.
* CPU cost: the FM render is heavy (per-sample double math x 32 voices), ~tens of
  seconds to render a 20-30 s song offline.  No optimization pass yet.

## Build coverage

CMake and the Zig build enable the FM path (WANT_MAFM / WILDMIDI_MAFM).  The
Android NDK build (android/jni/Android.mk) also enables it.  The Watcom builds:
os2/makefile.wat enables it; win32wat/makefile links the stub only (that target
historically ships without SMAF at all, so it just needs the SMAF objects to
resolve the references, not the FM feature).  All the _WM_MAFM_* call sites in
wildmidi_lib.c / internal_midi.c / f_smaf.c are guarded by #ifdef WILDMIDI_MAFM,
so a target that does not define it builds and links cleanly without the mafm
sources.
