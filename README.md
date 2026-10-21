# TrackPatterns

Per-track patterns for the Elektron Octatrack (original OS 1.40C): each of the
16 tracks (T1–T8, M1–M8) can play its own pattern of the playing bank and
change it on its own, clip style, as on the Korg SQ-64. A change lands at the
end of that track's own loop, so it stays in time; the sound (the Part's
machines, samples, FX) stays as it is.

The design is the proposal [Per-track patterns on the Octatrack](https://claude.ai/artifact/LJ5dBZqcLULc8Y6kGFqzP8).
This repository implements its phase 1 (v1), first as a native octabam /
Modwerk module; in Modwerk it is meant to sit on a shared base event for the
step source (phase 0), beside Play Modes.

**Status: in progress, partly tested on one unit.** The engine, the glue,
the six sequencer detours and the key gesture are written; the host gate
passes. Under the ColdFire emulator, T1 and then M1 were switched by the
gesture from A01 to A03, each at its own loop start, while the global
pattern stayed A01, and a global change to A02 brought both back. On an
MKII (build `OCTABAM1`, 21 Oct 2026) the gesture, the way back to following
and a global change clearing the override passed for T1; MIDI tracks, the
stopped case and saving are not tested on a unit yet
([TESTING.md](trackpatterns/TESTING.md)). Build `OCTABAM2` adds the trig
LEDs for a track on its own pattern (emulator only so far). The track's
speed stays the playing pattern's, and nothing is saved
([INVESTIGATION.md](trackpatterns/INVESTIGATION.md)).

## v1 behaviour (from the proposal)

| Situation | v1 behaviour |
| --- | --- |
| Pick a track's pattern | Hold [TRACK n], hold [PATTERN], press [TRIG k]; MIDI mode addresses M1–M8 (in stock 1.40C this combination is just PATTERN + TRIG) |
| When it lands | At the end of that track's loop; while stopped, at once |
| Back to normal | The global pattern's trig, or any global pattern change (a bank change too), which clears every override |
| Sound design | Always the playing Part |
| Length and scale | The track loops at its source pattern's length and scale |
| Trig LEDs ([proposal](https://claude.ai/artifact/KsJYqnuHdtNacaXh98vJjQ)) | For the current track, when it plays its own pattern. Outside GRID REC (TRACKS trig mode), where stock shows only the running light: that pattern's trigs, blinking slowly. In GRID REC: stock's trigs (the global pattern the keys edit), blinking slowly as a warning. The running light stays in both. Hold TRACK n + PATTERN: the pattern T_n plays lights as the current one, a change it has queued as a queued one |
| Editing, recording | Still the global pattern (a known v1 limit) |
| Saving | Not saved |

## Layout

| file | what |
|---|---|
| `trackpatterns/trackpattern.h`, `trackpattern.c` | the engine: per-track sources, the queue, loop-start detection; pure C |
| `trackpatterns/adapter.c` | firmware glue: the sequencer's bytes, the entries the detours call, the popup text |
| `trackpatterns/hooks.s` | the eight detour stubs, register-saving entries and DRAM state |
| `trackpatterns/manifest.py` | the native declaration: the unit and its detours, each guarded by stock bytes |
| `trackpatterns/test-remix.py` | the test image: TRACK PATTERNS on the stock effects |
| `trackpatterns/generate.py` | compiles the three into the ColdFire unit `trackpatterns.s` |
| `trackpatterns/verify.py`, `test_*.c` | the host gate |
| `trackpatterns/INVESTIGATION.md` | what is known about the sequencer and what has to be read |
| `trackpatterns/TESTING.md` | what was run and what was not |

## Build and test

```sh
cd trackpatterns
python3 verify.py          # host suites; with m68k-elf-gcc also the ColdFire unit
python3 generate.py        # refresh trackpatterns.s after a change
```

No firmware is included here or may ever be committed; images, listings and
probes stay in git-ignored `out/`.

## Credits

Original code by pgstath, MIT ([LICENSE](LICENSE)). The sequencer addresses
come from devilfish707's [Play Modes investigation](https://github.com/devilfish707/Octaplay/blob/main/playmodes/INVESTIGATION.md),
which cites octabam (Sam Banks), Tim Hastie's DIRECT JUMP and SCALE QUANTIZER,
the KYOTI notes (Zac-Kyoti) and EUCLID; the trig-key, GRID REC and LIVE REC
addresses from pgstath's STEP REPEAT work on Play Modes.
