# TinySoundFont2

Vendored copy of [TinySoundFont2](https://github.com/anzz1/TinySoundFont2)
v2.03 at commit `7570b49704219430e467c98676d01f9c4b32f37b` (2026-11-07), used
by `src/sf2.c` when built with `-DWANT_SF2=ON`. See its `TECHNICAL_DETAILS.md`
for how the SoundFont 2.01 and General MIDI specifications are implemented.

MIT licensed — see `LICENSE`.

To refresh, replace `tsf.h` and `LICENSE` with a newer upstream revision. Only
`tsf.h` is compiled; the rest of the upstream repo (examples, tools) is not
needed.
