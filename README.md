# TrackPatterns

Per-track patterns for the Elektron Octatrack (original OS 1.40C): each of the
16 tracks (T1–T8, M1–M8) can play its own pattern of the playing bank and
change it on its own, resembling a clip style workflow. A change lands at the
end of that track's own loop, so it stays in time; the sound (the Part's
machines, samples, FX) stays as it is.

The design is the proposal [Per-track patterns on the Octatrack](https://claude.ai/artifact/LJ5dBZqcLULc8Y6kGFqzP8).
This repository implements its phase 1 (v1), first as a native octabam /
Modwerk module; in Modwerk it is meant to sit on a shared base event for the
step source (phase 0), beside Play Modes.

**Status: 0.1.0, experimental, tested on one MKII.** The same code is
submitted to octabam as [sambanks/octabam#846](https://github.com/sambanks/octabam/pull/846).
Everything below is recorded in [TESTING.md](trackpatterns/TESTING.md).

- **Host gate:** passes.
- **Under the ColdFire emulator:** T1  (Track 1) and then M1 (MIDI Track 1) were switched by the
  gesture from A01 to A03, each at its own loop start, while the global
  pattern stayed A01; a global change to A02 brought both back. MKI and
  MKII behaved the same.
- **On the contributor's MKII** (21–22 Oct 2026), these were reported as
  passed:
  - `OCTABAM1`, `OCTABAM2` and `OCTABAM3`: the gesture on T1 and on M1,
    following again, a global change, the stopped case, chains and GRID
    REC, save and power cycle, and the trig LED checks.
  - `OCTABAM8`: the same gesture, global-change and save tests beside KITS
    and DIRECT JUMP (Kyoti).
  - `OCTABAM9`: a 32-step override and the other trig modes, CHROMATIC
    included.
  - CF METER, with and without the module (see Cost).
- **Not measured:** a long run over 30mins, and chip cycles per detour.
- **Open:** a silent hang, not yet explained. On `OCTABAM2` and `OCTABAM3`
  the unit hung after 5–20 minutes on the main playback screen with an
  override set.
- **By design:** the track's speed stays the playing pattern's, and nothing
  is saved ([INVESTIGATION.md](trackpatterns/INVESTIGATION.md)).

## v1 behaviour (from the proposal)

| Situation | v1 behaviour |
| --- | --- |
| Pick a track's pattern | Hold [TRACK n], hold [PATTERN], press [TRIG k]; MIDI mode addresses M1–M8 (in stock 1.40C this combination is just PATTERN + TRIG) |
| When it lands | At the end of that track's loop; while stopped, at once |
| Back to normal | The global pattern's trig, or any global pattern change (a bank change too), which clears every override |
| Sound design | Always the playing Part |
| Length and scale | Under SCALE MODE PER TRACK the track loops at its own length in its source pattern, inside the global pattern's MASTER LENGTH, which still restarts every track. Under NORMAL every track wraps at the global pattern's length. Speed (scale) stays the playing pattern's |
| Trig LEDs | For the current track, when it plays its own pattern as in stock. When the current track does not play the current globally selected pattern, in GRID_REC mode  the patterns trigs, blinking slowly, from the 16 steps its playhead is in. In other trig modes (CHROMATIC included) the module draws nothing. The design reason behind it is to show the performer that the current sequence shown and blinking is not the one played. |
| Editing and recording | Still done in the global pattern (a known v1 limit). Treat this version as performance-focused and not editing/composition focused |
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

`trackpatterns/manifest.py` is the Modwerk form: each detour is guarded by
the address, length and SHA-256 of the stock bytes it replaces. The octabam
submission (#846) carries the same code with octabam's usual literal-byte
guards.

## Cost

**Under the emulator** (`ot_emu --mkii`), instructions per call, median /
max, counting the stock instructions each detour replays:

| detour | median / max |
|---|---|
| audio tick | 86 / 95 |
| MIDI tick | 89 / 98 |
| audio advance | 60 / 186 |
| MIDI advance | 60 / 84 |
| the LED pass's own part | 52 / 658 |

A 16-track step is about 2,360 instructions, 3,700 at most.

**Memory:** 3,457 B of code and data in the DRAM reserve; no `.bss`, heap,
battery RAM or DSP words. The stack is about 72 B.

**On an MKII with CF METER** (300 BPM, a trig on every step of 15 tracks):
- With the module loaded and every track following, idle time and the frame
  interrupt match the build without it, within the noise.
- With eight tracks on their own patterns, the LED pass and gestures, idle is
  1.5 points lower (about 5.4 µs of each 362.8 µs frame). The interrupt's
  mean is unchanged, and no frame ran late.

## Compatibility

- **On the unit:** beside KITS and DIRECT JUMP (Kyoti), in octabam-core
  (`OCTABAM8`).
- **Emulator only:** beside Play Modes (0.1.1 and 0.2.0), Euclid, Tape Echo,
  Mini Verb, E-Verb and USB audio, in one Modwerk image.
  - The detour ledger reports no shared byte; Play Modes patches the
    neighbouring instructions in the same sequencer functions.
  - The gesture run and the trig LEDs match the module alone.

## Releases

See [CHANGELOG.md](CHANGELOG.md).

## Credits

Original code by pgstath, MIT ([LICENSE](LICENSE)). The sequencer addresses
come from devilfish707's [Play Modes investigation](https://github.com/devilfish707/Octaplay/blob/main/playmodes/INVESTIGATION.md),
which cites octabam (Sam Banks), Tim Hastie's DIRECT JUMP and SCALE QUANTIZER,
the KYOTI notes (Zac-Kyoti) and EUCLID; the trig-key, GRID REC and LIVE REC
addresses from pgstath's STEP REPEAT work on Play Modes.
