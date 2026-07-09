# Emergency Soundbank — Live Synthesis Fallback

Refs: [issue #121](https://github.com/Mindwerks/wildmidi/issues/121).

Goal: when the user has no `.pat` set and no `.sf2`, WildMIDI still makes
sound. Not hi‑fi — audible, recognisable GM instruments generated in RAM at
startup.

## Feasibility: yes

WildMIDI already speaks one internal format: `struct _sample` (see
[include/sample.h](include/sample.h#L45)). Every loader — GUS, SF2 — ends up
handing a `_sample` chain to `sample_patch->first_sample` from inside
[`_WM_load_sample`](src/sample.c#L117). A synthesiser is just another loader
that never touches disk.

Integration point is one branch in `_WM_load_sample`:

```
if (guspat = _WM_load_gus_pat(...)) { /* existing */ }
else if (emergency_bank_enabled)   { guspat = _WM_synth_patch(patchid); }
```

Trigger: `--emergency` CLI flag, or automatic fallback when no cfg is found.
No API break; existing users notice nothing.

## Licensing

- `EmergencySoundbank.java` (Gervill / Sun) is **GPL‑2 only** → **cannot lift**. Technique is not copyrightable; a clean reimplementation is fine.
- `csharpsynthproject` is MIT → compatible, safe to port ideas or code.
- AKWF single‑cycle waveforms are **public domain (CC0)** → safe to embed as static tables.
- KissFFT is BSD‑3 → compatible if we want an iFFT approach.
- Sonivox EAS wavetable (`wt_44khz.c`) is Apache 2.0 → compatible with LGPLv3 (WildMIDI's library license), **not** with LGPLv2. WildMIDI is v3+, so it is usable — but only from library code, not from the GPLv3 player if we want to stay strict.

Any lifted code goes in its own file with its original header preserved and gets called out in `docs/license/`.

## Three approaches, laziest first

### A. Detuned sine + ADSR per program family  *(recommended first cut)*

One sine oscillator, per‑program pitch envelope, per‑program amplitude
envelope, per‑program brightness (a running average = one‑pole LPF). 128
programs collapse into ~8 families (piano, organ, guitar, bass, strings,
brass, reed, lead/pad, plus percussion tables) with a small parameter table.

- ~200 lines of C, no deps.
- One 1s sample per program → `128 × 44100 × 2 B ≈ 11 MB` worst case; do it lazy per program on first `NoteOn` and it stays under 1 MB in practice.
- Sound quality: chiptune‑ish. Fine as a "the config is missing" safety net.

### B. Additive (harmonic stack) + noise for percussion

Sum of 4–8 sines with a per‑program harmonic amplitude table, noise burst
for drums, same ADSR framework as A. Closer to a real timbre; brass and
strings become distinguishable. csharpsynth's generator is roughly this
shape and MIT‑licensed — that's the model to crib from.

- ~600 lines of C.
- Still no FFT dep.
- Percussion (channel 10) needs its own noise+resonator path — an extra ~100 lines.

### C. Gervill‑style frequency‑domain synthesis

Render weighted Gaussians in the frequency domain, phase‑randomise, iFFT,
window. This is what `EmergencySoundbank` does and why its output sounds
plausible. Requires an FFT — pull in KissFFT (BSD, ~1 kLOC, single header +
single .c).

- ~1200 lines of C + KissFFT.
- Genuinely usable for casual playback.
- Only worth doing if A/B prove insufficient.

## CPU cost

All figures are **one‑shot at startup**, or lazy on first use of each
program. Nothing runs per audio callback — output is a normal `_sample`
that flows through the existing mixer.

| Approach | Per‑patch synth | 128 patches (upper bound) | Runtime cost after warm‑up |
|----------|-----------------|---------------------------|----------------------------|
| A sine+ADSR | ~1 ms | <150 ms | zero |
| B additive  | ~5–10 ms | ~1 s | zero |
| C iFFT      | ~10–30 ms (44100‑pt) | ~2–4 s | zero |

Memory: 88 KB per 1s mono patch @ 44.1 kHz. Cap generated length per family
(pads = 2s, drums = 0.3s, plucks = 0.5s) and total stays around 4–6 MB even
if every program is touched.

## Difficulty

- **A**: ~half a day. New `src/emerg_synth.c` + `include/emerg_synth.h`, a
  128‑entry program table, one branch in `_WM_load_sample`, CLI flag in
  `src/player/wildmidi.c`, one env‑target/rate mapping using the existing
  `env_rate[]/env_target[]` machinery.
- **B**: 2–3 days. Same skeleton as A + a harmonic table per family + a
  noise+resonator path for drums + real tuning by ear.
- **C**: 1–2 weeks. Add KissFFT (or write a radix‑2 iFFT — ~120 lines),
  reproduce the Gaussian‑sum→phase‑rand→iFFT pipeline, tune per family,
  handle loop‑point selection on non‑decaying tones.

## Recommendation

Ship **A** as `--emergency` in a first pass. It answers issue #121: the
player never falls silent for lack of patches. If users want it to sound
good rather than just present, escalate to **B**; skip **C** unless someone
volunteers.

## Concrete task list for approach A

1. `include/emerg_synth.h`, `src/emerg_synth.c` — one public function
   `struct _sample *_WM_synth_patch(uint16_t patchid);` returning a
   `_sample` chain compatible with the mixer (16‑bit signed, `SAMPLE_LOOP`
   set on sustained families, envelope in `env_rate[]/env_target[]`).
2. Program → family table (~30 lines of static data).
3. Wire fallback into [`_WM_load_sample`](src/sample.c#L117) — one `else if`.
4. `--emergency` flag in `src/player/wildmidi.c` and matching library option.
5. Auto‑enable the fallback when no cfg is discoverable (opt‑in via option,
   not silent, so debugging stays honest).
6. `test/` — one MIDI file rendered with `-o` to WAV, byte length asserted
   non‑zero; that's the "did anything come out" smoke test.
