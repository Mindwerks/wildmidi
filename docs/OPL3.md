# OPL3 FM Synthesis in WildMIDI

WildMIDI can synthesise General MIDI playback through an emulated Yamaha
YMF262 (OPL3) FM chip, so it produces sound even when no GUS patch set and
no SoundFont are available.

```bash
wildmidi --opl3 song.mid            # built-in FM instrument table
wildmidi -c GENMIDI.op2 song.mid    # authentic DMX GENMIDI FM bank
```

Programmatically, pass `"@opl3"` (see `WM_OPL3_CONFIG` in `include/synth.h`)
or a path to a `.op2` bank as the config argument of `WildMidi_Init()`. The
`.op2` file is recognised by its `#OPL_II#` magic, the same way an `.sf2`
soundfont is recognised in place of a config file.

## Using Nuked-OPL3-fast

The FM chip emulator is **Nuked-OPL3-fast** by Tony Gies, a bit-exact
performance fork of Nuked OPL3 by Alexey Khokholov (Nuke.YKT):

- upstream: <https://github.com/tgies/Nuked-OPL3-fast> (version 1.8-fast.3,
  tracking Nuked OPL3 1.8, commit cfedb09)

Four files were imported verbatim, original copyright headers preserved:

| File | Role |
|---|---|
| `src/opl3.c` | the chip emulator |
| `include/opl3.h` | its public API (`OPL3_Reset`, `OPL3_WriteReg`, `OPL3_GenerateResampled`, …) |
| `include/wf_rom.h` | pre-generated logsin waveform lookup table |
| `include/gen_logsin.py` | generator for `wf_rom.h` (regenerate: `python3 gen_logsin.py > wf_rom.h`) |


## License compatibility

- Nuked-OPL3-fast is **LGPL 2.1 or later**.
- The WildMIDI library is **LGPL 3 or later**; the player is GPL 3 or later.

LGPL 2.1's "or any later version" clause allows the emulator to be used
under LGPL 3, so the combined library is distributed under LGPL 3 as usual.
Credit and license text stay in the imported files' headers.

The GENMIDI instrument data story, for the record:

- The original DMX `GENMIDI` lump shipped with Doom is proprietary — we do
  not ship or embed it.
- [DMXOPL](https://github.com/sneakernets/DMXOPL) is an MIT-licensed
  replacement GM bank in the same format. Its instrument data is **embedded**
  in the library (`include/synth_bank.h`, with its MIT license text), so
  `--opl3` works with no data files and sounds identical to
  `wildmidi -c GENMIDI.op2` with the DMXOPL bank. Passing any other `.op2`
  file replaces the embedded bank for that session.
- Gervill's `EmergencySoundbank.java` (GPL-2-only) was deliberately never
  consulted.

## How it is wired up

WildMIDI's mixer is a wavetable engine: every loader hands it chains of
`struct _sample` (PCM + loop points + envelope parameters). Rather than
bolt on a second real-time synthesis path, the OPL3 integration renders
each GM program **once, at patch-load time**, into an ordinary `_sample`:

1. `WildMidi_Init` (src/wildmidi_lib.c) recognises `"@opl3"` or `#OPL_II#`
   magic and calls `_WM_opl3_init_patches()`, which fills `_WM_patch[]`
   with entries whose `filename == NULL`.
2. `_WM_load_sample` (src/sample.c) routes `filename == NULL` patches to
   `_WM_synth_patch()` instead of the GUS loader — one `else if`.
3. `_WM_synth_patch` (src/synth.c) boots a Nuked OPL3 instance per patch,
   programs a 2-op FM voice from either the loaded `.op2` bank or the
   built-in table, key-ons, and captures the chip output:
   - **Sustaining voices** (carrier EGT bit set) render ~300 ms into the
     sustain plateau and loop an integer number of fundamental periods,
     using the mixer's 10-bit fractional loop points for a click-free seam.
     `SAMPLE_SUSTAIN` keeps the mixer envelope parked until note-off.
   - **Decaying voices** (EGT clear: pianos, plucks, all percussion) render
     as one-shots so their natural FM decay is heard; the mixer envelope
     only gates note-off.
   - Each tonal program is rendered at **three roots two octaves apart**
     (OPL blocks 2/4/6), chained with `freq_low`/`freq_high` boundaries, so
     the mixer never pitch-shifts a sample more than one octave and the FM
     timbre stays put across the keyboard.
   - GENMIDI note offsets are honoured by shifting the pitch reported to
     the mixer (`semitone_ratio`, no libm); percussion records use their
     fixed-note field.
4. Velocity, channel volume, expression, pan and note-off release are all
   handled by the existing mixer — the samples carry no per-note state.

Build wiring: `opl3.c` is listed in `src/CMakeLists.txt` and in each of the
stand-alone build files (`os2/`, `djgpp/`, `amiga/`, `macosx/`, `mingw/`,
`msvc/`, `android/`), same as every other library source.

## Cost

Rendering is one-shot per patch at first use; playback afterwards runs the
ordinary mixer with zero added per-sample cost. Measured on canyon.mid
(2 min 1 s) rendered offline to WAV, Apple Silicon:

| Mode | Wall time | Peak RSS |
|---|---|---|
| SF2 (florestan-subset via TSF) | 0.88 s | 3.75 MiB |
| OPL3 built-in table | ~1.0 s | ~3.8 MiB |

Memory for the rendered bank stays in the low single-digit MiB even if a
song touches every program (three ~300 ms samples per tonal patch, one
one-shot per drum key).

## Known limitations

- `.op2` double-voice instruments layer two detuned voices; the second
  voice's fine-tune detune uses a linear approximation of the exponential
  pitch ratio (accurate for the small detunes banks actually use).
- Three sample roots per program instead of true per-note synthesis; FM
  timbre is exact at the roots and pitch-shifted up to ±1 octave between
  them.
- OPL vibrato/tremolo (AM/VIB operator bits) affect the rendered sample but
  are frozen into the loop; long-period LFO movement does not continue
  through a held note the way it would on real hardware.
