# SMAF: known gaps (MA-1 .. MA-6)

Things real `.mmf` files contain that WildMIDI currently ignores, drops or
refuses. Scope note: this list is about the MA-1..MA-6 generations; MA-7's
remaining gaps are tracked in `SmafFileFormat.txt` alongside its format notes.

All counts below come from auditing the ~1180 `.mmf` files in
[denjhang/libsmaf](https://github.com/denjhang/libsmaf), of which 1077 have a
score track.

---

## 1. PCM (sampled) voices - second pass DONE

MA-3/MA-5 `Mtsu` voice-exclusives come in two kinds selected by the voice-type
byte: FM and PCM (sampled). In this corpus the sampled ones are the majority:

| kind | count |
|---|---|
| FM voices | 2355 |
| PCM (sampled) voices | **2537** |

Historically playback threw them away - `src/mafm.c` returned early on
`is_pcm`. That guard is now replaced with a full sample-playback path:

- **Per-channel pan** tracked and applied at note-on (previously mono centre)
- **Pitch shift** for melodic voices: `drum_note != 0` plays at native rate,
  `drum_note == 0` treats note 60 as the root and resamples for other notes
- **ROM-wave fallback**: when a PCM voice references a wave the file did NOT
  ship (Yamaha MA chips carry a ROM sample set indexed by `wave_id` - the
  corpus audit shows 361 of 415 MA-3/5 files with PCM voices point exclusively
  at ROM waves 0..~25 that no file provides), fall back to the FM drum
  approximation. A synthetic kick/snare beats missing hits for percussion,
  and re-uses code the drum-channel fallback already needs.
- **MA-7 sampled voice acceptance**: forms 0x01, 0x03, 0x07 in the MA-7
  voice-exclusive header (previously rejected) are now decoded as PCM voices
  reading fs from body[0..1] BE and `wave_id` from body[15]. Even when
  the referenced wave is unavailable, DrumApprox fires so the sampled hit is
  audible instead of dropped.
- **ADSR envelope + loop region** for PCM voices with real env fields
  (MA-3/MA-5 sampled voices whose wave the file provides via Mtsp/Mwa).
  `pcm.env.AR/DR/SL/SR/RR` map to the same per-sample increments the FM
  operator EG uses; `pcm.env.TL` attenuates the base gain; `pcm.loop_pt` /
  `end_pt` / `loop` drive an in-tick wrap. Note-off transitions the slot
  to release phase; ATR one-shots and the streaming synthetic entry skip
  the envelope entirely (params with all-zero env fields are treated as
  no-envelope so RR=0's "slow ~20s glide" floor doesn't silence a valid
  long-duration wave).

Still open:

- **Bank matching is fine** - earlier note said "loose matching" was
  suspected; investigation with a printf-in-mafm_note_on trace shows the
  PCM path actually fires (17_Niko: 20 note-ons across 4 (ch, bank, pc)
  triples in the first 30 s, all correctly routed).  What made the batch
  numbers look unchanged is that peak/rms are a poor metric for sparse
  drum-transient additions on top of a continuous FM texture: 20 drum
  hits over 40 s barely move the aggregate rms.  Actual audio content
  IS increased; verify by listening.
- **No ROM sample bank** - the biggest remaining gap. Yamaha MA chips carry
  hundreds of ROM samples referenced by low `wave_id`; the corpus audit says
  361 of 415 MA-3/5 files with PCM voices reference exclusively ROM waves
  the file itself doesn't provide.  See the "ROM sample bank options" section
  below.  Decoding the Mtsu wave records (below) shrinks this a little - 23
  MA-5 files turn out to ship a wave their own sampled voices ask for - but
  the large majority still point at ROM.

## 2. `Mtsp` (embedded PCM in the score track) - DONE

Waves are now pulled from the score track's `Mtsp` chunk as well as the
`ATR`/`Awa` audio track. Each `Mtsp` contains `Mwa<n>` sub-chunks with a
3-byte header (`format`, u16 BE rate) followed by the same Yamaha ADPCM the
`Awa` path already knows how to decode. See `mafm_add_wave_mwa()` and
`mafm_build_mtsp_waves()`.

Streaming-audio special case: MA-7 files like `First Love` and Yamaha's
`example.mmf` ship an entire song as one big `Mwa` in `Mtsp` and trigger it
with a single note-on on the drum bank (0x7d), with no matching Mtsu voice
definition. `_WM_MAFM_NewSynth()` now synthesises a placeholder PCM bank
entry pointing at the first loaded wave, and `_WM_MAFM_HasCustomVoices()`
engages the MAFM engine on files that have Mtsp waves but no Mtsu voices,
so those files play their audio instead of GM silence.

## 3. Audio-only files (CNTI + ATR, no MTR) - DONE

83 files.  Panasonic G60 system sounds, LG chocolate-ringtone system sounds,
and similar - pure ADPCM audio with no score.  Previously
`_WM_smaf2midi()` returned "no score track" and the file was rejected.

The fix, in `src/smaf2mid.c`: when `find_sequence()` fails but the container
has any `ATR*` chunk, emit a minimal placeholder MIDI (tempo + a 3 s gap +
end-of-track) with the `fmt = 0xff` sentinel to skip score decoding.
`_WM_MAFM_HasCustomVoices()` gained a matching branch that engages MAFM on
ATR waves + Atsq triggers even without any Mtsu voice bank.  `mafm.c`'s
`_WM_MAFM_ActiveVoices()` now also counts a pending ATR trigger within 15 s
of the render cursor as active, so wildmidi_lib doesn't cut playback off at
the 3 s placeholder end before the wave has actually fired.

Verified: 83/83 audio-only files in the corpus render with audio (was 0/83).

## 4. Huffman-compressed Mobile Standard (`format_type 0x01`) - DONE

4 files.  All four carry a real Huffman payload: a 4-byte BE uncompressed
length followed by an MSB-first bitstream.

`_WM_smaf2midi` now decompresses it (Yamaha's Okumura-style prefix code, a
fresh C implementation in `smaf2mid.c`: bit 1 = internal node, recurse left
then right; bit 0 = leaf carrying an 8-bit literal), swaps in the
decompressed buffer and sets `fmt = 0x02` so the existing Mobile Standard
decoder handles the rest.  Before this the converter treated `0x01` like
`0x02` and produced 0.1 s of silence.

All four now render: `J-SH08/My Happiness.mmf` (55.7 s, peak 30002),
`J-SH010/21 IT'S A SMALL WORLD.mmf` (40.9 s, peak 30000),
`J-SH010/system/j-sh010_30.mmf` (6.1 s, peak 28446),
`J-SH010/system/j-sh010_46.mmf` (2.7 s, peak 5624).

## 5. Container metadata is not surfaced - tracked in docs/TEXT_TODO.md

**Impact: cosmetic.**  1178 files carry `CNTI`, 704 carry `OPDA`.

`CNTI` (contents class/type/copy status) and `OPDA` (title, artist, composer,
copyright - 2-uppercase-letter keys) are parsed by nothing.  This is really a
library-wide gap rather than a SMAF one - WildMIDI surfaces no text from any
format it reads, including SMF/KAR lyrics - so it now lives in
`docs/TEXT_TODO.md` along with the encoding question (SMAF predates universal
UTF-8 and the corpus contains Shift-JIS).

---

## Checked and NOT a problem

Worth recording so nobody re-investigates:

- **Multiple score tracks.** No file in the corpus has more than one
  Mobile/SEQU score track, so `find_sequence()` taking the first is not a live
  bug. The 150 four-track files are all HandyPhone, which
  `decode_all_handyphone()` already merges onto one timeline.
- **`ATR` alongside a score track** occurs *only* with `format_type 0x00`
  (52 files) - exactly the MA-1/MA-2 ADPCM drums that already play.
- **`MSTR`** (176 files) and **`GTR`** (29 files): master track and graphics
  track. Nothing audible.
- **`MMMG`-only files** (18): voice-bank containers, not songs. Correctly
  refused.
- **HV speech chunks** (`Mthv` / `Mhvs` / `Mhsc`): the middleware supports
  them, but no file in the corpus uses them.
- 2 files are corrupt (garbage chunk ids at the top level).

---

## What remains

- Gap 1 sub-item: ROM sample bank - see "ROM sample bank" below.
- Gap 1 sub-item: do setup waves and `Mtsp`/`Mwa` waves share one numbering
  space?  See "Wave delivery in Mtsu" below.
- Gap 5: expose `CNTI`/`OPDA` metadata - moved to `docs/TEXT_TODO.md`, which
  covers SMF/KAR lyrics and the encoding question too.

Gap 4 (Huffman-compressed Mobile Standard, format_type 0x01) is now DONE -
see the section further below.

---

## Wave delivery in Mtsu - DONE (MA-3, MA-5, MA-7)

Dense MA-3, MA-5 and MA-7 files ship wave data inline in `Mtsu` instead of
`Mtsp`/`Mwa`, as exclusive records of a few hundred to a few thousand bytes:

```
43 79 08 7F 23 [waveId] [00] [payload ...] F7      MA-7
43 79 07 7F 03 [waveId] [00] [payload ...] F7      MA-5
43 79 06 7F 03 [waveId] [00] [payload ...] F7      MA-3, payload 7-bit packed
```

The payload is the same 4-bit Yamaha ADPCM the `Awa`/`Mwa` path already
decodes (`_WM_MAFM_AdpcmDecodeAll`), packed **low nibble first**, starting at
byte 7.  The record carries no sample rate; the referencing voice record
supplies it, so these waves are stored with `fs = 0` and
`mafm_start_pcm_full()` takes the rate from the voice.  Byte 6 is 0 in every
record in the corpus and its meaning is not established.

**MA-3 packs its payload 7-bit clean** the way a SysEx body is supposed to be:
every group of 8 bytes carries 7 data bytes, the group's first byte holding
their high bits, most significant first (`_WM_MAFM_Unpack7()`).  MA-5 and MA-7
just run raw 8-bit ADPCM straight through the setup chunk.  The corpus is
unambiguous: not one of the 312 MA-3 records contains a byte above 0x7f, while
every one of the 599 MA-5/MA-7 records does.

Corpus evidence for the framing.  The corpus holds 911 such records over 1180
files; the sweep below scored a uniform sample of each family against every
offset 5..16 and both nibble orders.  A wrong decode shows up as a runaway
integrator, so the discriminators are the share of samples pegged at full
scale and the DC offset ("plausible" = <=5% clipped and |DC| <= 4000):

| family | records | sampled | best framing | plausible |
|--------|---------|---------|--------------|-----------|
| MA-7 `08` / `7F 23` |  10 |  10 | offset 7, low-nibble-first | 100%   |
| MA-5 `07` / `7F 03` | 589 | 148 | offset 7, low-nibble-first |  95.9% |
| MA-3 `06` / `7F 03` | 312 | 156 | 7-bit unpack, then as above |  96.8% |

Two wrong turns are worth recording so nobody repeats them.  **High-nibble-first**
pegs samples at full scale and drifts thousands off centre, which is why an
earlier pass recorded the Yamaha codec as "ruled out"; low-nibble-first is the
order the `Awa`/`Mwa` loaders already use.  And MA-3 was written off as "not
this codec" on the strength of a *false* observation - that two `SWEETRANCE1`
records of different lengths shared a long identical prefix.  They do not: the
common prefix inside a file is only the 5-byte record id, and the comparison
had accidentally been made across `SWEETRANCE.mmf` and `SWEETRANCE1.mmf`,
which are duplicates of the same song.  The real signal was in plain sight in
the hex - no byte above 0x7f.

**MA-3's sampled VOICE body is packed the same way**, and getting that wrong
is worse than not decoding the waves at all.  A sampled voice record is
uniformly 29 bytes, so the body is 19 bytes, and 19 packed bytes unpack to
exactly 16 - the MA-5 PCM body size.  Unpacked, the MA-5 field offsets apply
byte for byte.  Read raw, the fields land a byte early:

| field | read raw | after `_WM_MAFM_Unpack7()` |
|-------|----------|----------------------------|
| `Fs` inside 2000..48000        | 69%   | **100%** |
| `WaveID` hits a wave the file ships | 12.1% | **58.0%** |
| `TL` (gain `10^(-0.75*TL/20)`) | TL=28 on 86% of voices -> **0.089** | TL=0/4 dominate -> **~1.0** |

That 11x gain error made correctly-decoded samples nearly inaudible, and it
sounded *worse* than the DrumApprox fallback it replaced - caught by ear on
`JapaneseDrum4`, not by the corpus numbers, which happily reported "0 newly
silent" while the audio degraded.  Aggregate rms does not detect "right sample,
wrong gain".

This is the same bit-stealing scheme `apply_ma3_packed()` already undoes for
MA-3 FM voices: check its shifts and data byte *j* takes bit *(7-j)* of the
carrier, which is exactly MSB-first 7-bit unpacking.  MA-3 uses it in three
places - FM voice bodies, sampled voice bodies, and wave payloads.

The `WaveID` offset was pinned down by scoring every candidate against files
whose shipped wave ids are *sparse and high*: ids 0..4 are so common that any
small-valued field scores well by chance, and the naive sweep was misled by
exactly that.  Against sparse ids the winner is unambiguous (11.8x chance,
next best 3.6x).

**Reach.** Blossom's sampled voices reference 16 unique wave IDs but only 2
resolve to a wave it actually ships - the rest are ROM waves.  So for MA-7
this is a small win; the MA-5 side is where it pays off (23 files have a
sampled voice pointing at a wave the file itself delivers), and the
ROM-sample-bank question below still dominates.

**Verification.** Rendered 78 affected corpus files against the library built
from this commit's parent and from the commit itself (swap the built dylib,
not the `wildmidi` binary: the binary links `@rpath/libWildMidi.2.dylib`, so
copying the executable alone compares a build against itself and reports a
false "no change").  Result: 46 files changed, 32 identical, **0 newly silent
and 0 newly failing**.

The large rises are sampled voices that now play a real wave instead of the
FM DrumApprox fallback (`Sound_82` 69 -> 966 rms and renders 12k frames
longer as the wave tail now sounds; `JazzKit` 721 -> 3041; `Mystery`
1410 -> 4823).  The two notable drops are also correct: in
`HotRodCountry-Q-UTF8EN` the shipped wave 1 (clip 0.1%, DC -45) replaces a
DrumApprox voice that was simply louder than the genuine sample.  `Sound_108`
moves 26 -> 9 rms but is ~0.1% of full scale either way.

**Open: are setup waves and `Mtsp`/`Mwa` waves the same numbering space?**
55 corpus files carry both, and about half reuse the same numbers.  They are
*not* the same wave: decoding `Pop STAR`, `MySpanishGrace` and `EuroHouse`
both ways gives different lengths (e.g. wave 2: 9962 vs 5620 samples) and
correlation ~0.  So the two sources are probably separate banks that
`mafm.c` currently flattens into one `s->waves[]` array.

Until that is settled, `mafm_claim_wave()` lets an `Awa`/`Mwa` wave displace a
setup wave of the same number, which keeps those 55 files rendering exactly as
they did before setup records were read at all; setup waves only fill numbers
nothing else claims.  If the voice's `wave_id` turns out to address the Mtsu
bank instead, that priority should flip - which needs a reference decoder to
decide, not more corpus statistics.

---

## ROM sample bank

Yamaha MA chips carry an on-chip ROM sample bank that PCM voices reference
by low `wave_id` (0..~25).  361 of 415 MA-3/5 files with PCM voices in the
corpus reference exclusively ROM waves the file does not itself provide,
plus most MA-7 sampled voice records.  There is no free source:

  - **Yamaha MCP-MA7 for Windows** (see SmafFileFormat.txt for unshield
    instructions).  ROM likely sits inside `M7_EmuHw.dll` or
    `M7_EmuSmw7.dll`.  Extracting and redistributing would violate Yamaha's
    licence.
  - **SMAF-capable phone firmware** (Panasonic G60, LG Chocolate, Sharp GX,
    Samsung SGH-*).  Same licensing concern.
  - Yamaha's YMU chip datasheets are not public.

Every open-source SMAF project sidesteps this the same way:

  - **akustikrausch/yamaha-smaf-player** - "dependency-free... rebuilt with
    its own ROM-free FM engine".
  - **umjammer/vavi-sound** (Java) - plays the score but not sampled voices.
  - **but80/smaf825** - targets real Yamaha YMF825 hardware with its own
    on-chip ROM.

WildMIDI does the same: `_WM_MAFM_DrumApprox()` (ported from
akustikrausch) synthesises a three-way FM kick/snare/hat as the fallback
whenever a PCM voice's referenced wave is not in the file.  It is minimal
but self-contained and always works - MAFM does not depend on whichever
patch bank the caller has loaded.
