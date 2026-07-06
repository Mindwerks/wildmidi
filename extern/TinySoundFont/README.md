# TinySoundFont

Vendored copy of [TinySoundFont](https://github.com/schellingb/TinySoundFont)
at commit `fbc913531b85f5707f49115110bb86b1cd583885` (2025-07-19), used by
`src/sf2.c` when built with `-DWANT_SF2=ON`.

MIT licensed — see `LICENSE`.

To refresh, replace `tsf.h` and `LICENSE` with a newer upstream revision. Only
`tsf.h` is compiled; the rest of the upstream repo (examples, tools) is not
needed.
