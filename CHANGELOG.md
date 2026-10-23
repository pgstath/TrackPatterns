# Changelog

## 0.1.0 (22 Oct 2026), experimental

The first release. This is the code submitted to octabam as
[sambanks/octabam#846](https://github.com/sambanks/octabam/pull/846).

- **A pattern per track:** hold [TRACK n], hold [PATTERN], press [TRIG k].
  - Track n plays pattern k of the playing bank from the end of its own
    loop, so it stays in time.
  - While the sequencer is stopped, the change happens at once.
  - A popup names the change when it is done displaying (`T1 ▸ A03`, or `T1 FOLLOWS` when the pattern is back to the original).
- **Back to the global pattern:** press the global pattern's trig for one
  track. Any global pattern change, a bank change included, brings every
  track back.
- **Trig LEDs** for the current track when it plays its own pattern and not the global one: 
  - Outside GRID REC, that pattern's trigs blink from the 16 steps its
    playhead is in.
  - In GRID REC, the stock trigs blink as a warning.
  - TRACK n + PATTERN lights the pattern T_n plays.
- **Size and patches:** one DRAM unit (3,457 B) and eight detours, each
  guarded by the stock bytes it replaces.

Tested on one MKII (OS 1.40C, `OCTABAM1`–`OCTABAM3`, `OCTABAM8`, `OCTABAM9`)
and under the ColdFire emulator. [trackpatterns/TESTING.md](trackpatterns/TESTING.md)
has every run.

**Known limits:**
- Editing and recording the global pattern.
- Track Pattern overrides are not saved.
- A track's speed stays the playing pattern's.
- Under NORMAL every track wraps at the global pattern's length.

**Open:**
- A silent hang, not yet explained. On `OCTABAM2` and `OCTABAM3` the unit
  hung after 5–20 minutes on the main playback screen with an override set.
  It is under investigation; `OCTABAM6`, without the LED pass, is the
  bisect image.
