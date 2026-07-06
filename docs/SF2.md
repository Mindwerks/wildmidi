# SoundFont2 (SF2) Support

Implementation plan and design notes for [issue #8](https://github.com/Mindwerks/wildmidi/issues/8).

## Approach

WildMIDI gains SF2 rendering via [TinySoundFont](https://github.com/schellingb/TinySoundFont)
(`tsf.h`, MIT licensed, single-header, pure C), fetched at configure time with CMake
`FetchContent` and pinned to a known-good commit.

The key insight: WildMIDI's format parsers (MID, XMI, MUS, HMP, HMI) already convert
everything into a single time-stamped event list (`struct _event`), and the renderer is
just the stage that consumes that list. SF2 support swaps the *rendering* stage while
keeping every parser, the timing engine, and the public API untouched. No other MIDI
library renders XMI/MUS/HMP/HMI through SoundFonts — this is WildMIDI's niche.

TinySoundFont's common criticism — "you must do your own MIDI parsing" — is exactly
what makes it the right fit here: its `tsf_channel_*` API maps 1:1 onto WildMIDI's
already-decoded event types.

## How it works

1. **Loading** — `WildMidi_Init()` / `WildMidi_InitVIO()` sniff the config file for the
   RIFF/`sfbk` magic (not the file extension). An `.sf2` file given directly as the
   "config file" is loaded through the existing VIO buffer callbacks into one global
   `tsf*` instance. Additionally, the timidity.cfg parser understands a
   `soundfont <file>` directive (relative paths resolved against the config dir, same
   as `source`), so `/etc/timidity/fluidr3_gm.cfg` works as-is.
2. **Per-song synth** — each opened midi (`struct _mdi`) gets its own `tsf_copy()` of
   the global instance (sample data shared, voices private), created in `_WM_initMDI()`
   and freed in `_WM_freeMDI()`. Concurrent handles stay safe.
3. **Rendering** — `WM_GetOutput_SF2()` mirrors the scheduling loop of
   `WM_GetOutput_Linear()`. Every event still runs its normal `do_event` (tempo, lyrics,
   channel state; GUS note-ons no-op safely since no GUS patches are loaded) and is
   additionally translated to the corresponding `tsf_channel_*` call. Audio is rendered
   by `tsf_render_short()` into the 32-bit mix buffer, so `WM_MO_REVERB` and the rest of
   the output pipeline behave exactly like the GUS path.
4. **Release tails** — when the event list ends, rendering continues while
   `tsf_active_voice_count() > 0`, so SF2 release envelopes aren't clipped at
   end-of-song.

## Event translation

| WildMIDI event | TSF call |
|---|---|
| note on/off | `tsf_channel_note_on` / `_off` |
| patch change | `tsf_channel_set_presetnumber(..., isdrum)` |
| pitch wheel | `tsf_channel_set_pitchwheel` |
| all controllers (bank, volume, pan, expression, hold, RPN/NRPN, sound/notes off, …) | `tsf_channel_midi_control` (TSF implements the full CC map) |
| Roland drum-track sysex | `tsf_channel_set_bank_preset(ch, 128, 0)` |

Channel 9 defaults to bank 128 (drums) at synth creation, per GM convention.

## Build

* `WANT_SF2` CMake option, default `ON`; requires CMake >= 3.14 (auto-disables with a
  warning on older CMake so legacy platforms keep building).
* Offline/distro builds: point `FETCHCONTENT_SOURCE_DIR_TINYSOUNDFONT` at a local
  checkout — standard CMake, no extra plumbing.
* License: `tsf.h` is MIT, compatible with linking into the LGPLv3 library.

## Non-goals (first pass)

* Mixing GUS patches and SF2 in one config — if a soundfont is loaded it takes over
  rendering entirely.
* SF3 (ogg-compressed) soundfonts — one `#define TSF_SF3` + stb_vorbis away if wanted.
* timidity per-soundfont options (`amp=`, `order=`, `remove`) — parsed token is the
  filename only.
* `WildMidi_MasterVolume()` does not affect the SF2 path yet (TSF has its own gain).
