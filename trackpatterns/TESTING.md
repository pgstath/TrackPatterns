# TRACK PATTERNS testing

## Host gate (21 Oct 2026)

`python3 verify.py` on macOS (Apple clang, Homebrew python 3.12,
m68k-elf-gcc 16.2.0): both host suites pass, and the ColdFire unit compiles
and assembles for the 5475 with no calls outside it.

What the suites cover:

- A zeroed state follows the global pattern on every track.
- A queued change lands exactly at the track's own loop start, for lengths
  1–64, with the rest of the pass still on the old pattern.
- The global pattern's trig queues the way back to following; asking for
  what the track already plays withdraws a queued change.
- A global pattern change clears every override and every queued change;
  the same global pattern seen again changes nothing.
- While stopped, a change applies at once and PLAY plays it from step 1.
- The look-ahead names what then plays, across the loop end and after a
  queue (as the key stub has stock rebuild its steps).
- PLAY restarts land a queued change on step 1; the repeated step of a
  MASTER LENGTH restart is a loop start too.
- MIDI tracks (8–15), the popup text (`T5 > A03`, `M2 FOLLOWS`, the bank
  letter), out-of-range tracks and patterns.
- The adapter on fake sequencer memory: transport, playing bank and
  pattern, length source following the track.
- A bank change to the same pattern number (A10 → B10) clears the
  overrides, and the step stock prepares across it is B10's on every track;
  P16 is a pattern like any other.
- The gesture on fake key memory: no TRACK key held leaves the press to
  stock; TRACK 2 held (state 2, as once the PATTERN layer opened over it)
  queues for T2 with the toast `T2 > B05`; the lowest held TRACK key wins;
  MIDI mode addresses M5, not T5; the global pattern's trig makes the track
  follow again; a key past TRACK 8 is not a TRACK key.

Planted defects fail them: a loop-start test that ignores the repeated step
(5 engine failures), a global change that no longer clears (2 adapter, 9
engine failures).

The hand-written entries in `hooks.s` were disassembled after assembly and
read as written.

## ColdFire port (21 Oct 2026)

Image: native remix `trackpatterns-test` (TRACK PATTERNS + the 14 stock
effects) built in a Modwerk worktree at `c0f513b9` with `make bus`; the six
sites carry their jumps (the rebuild spans padded with `nop`). Emulator
`ot_emu` from the same tree. Project: a copy of the contributor's own,
clock and transport receive off, bank A with `ot_spec.py`: A01 T1 trigs on
steps 1, 5, 9, 13; A03 T1 on 3, 7, 11, 15; both 16 steps at 1X. Script:
NO (the date popup), PLAY, at 4 s a poke of T1's queue (`next[0] = 3`, A03;
no key gesture yet), STOP at 9 s. Watched: T1's counter `0x800064d0`, the
step handler's per-track trig entry `0x46c7a810` (a plain trig writes
`0x1d` at `0x4009d42a`) and `tp_state`.

| | result |
|---|---|
| T1's plain trigs before the change | steps 5, 9, 13, 1, 5, 9, 13 (A01) |
| the queue lands | at 15.97 s, the step T1's counter wraps to step 1: `src[T1] = A03`, `next` cleared |
| T1's plain trigs after it | steps 3, 7, 11, 15, round after round (A03), the global pattern still A01 |

Port evidence only, one track, queued by a poke.

## Gesture, MIDI track, global change (21 Oct 2026)

**Stock first.** On the image above (no gesture site yet, so the keys are
stock): hold TRACK 1, hold PATTERN, press TRIG 3 while playing. Stock
queued A03 for every track (`0x800065c0 <- 2` at `0x400a06dc`) and played
it at the pattern's end. TRACK 1's key state (`0x46c7d8ee + 24 * 0x10`)
went 1 on the press (pc `0x40031936`), then 2 when the PATTERN layer opened
over it (`0x400312e8`), and 0 on the release (`0x4003180c`). So the
combination has no meaning of its own in stock, and the trig still reaches
the PATTERN layer's handler with the TRACK key still marked as held.

**Image:** the same remix rebuilt with the seventh detour (`0x40056b50 ->
tp_pattern_key`) and the bank-aware tick. `tp_state` at `0x40a95f2c`.

**Project** `STEPTPM`: a copy of `STEPTP` plus A02 T1 trigs on 2 and 10
(`ot_spec.py`), and M1's trig mask written into the bank file's MTRA
records (tag + 9, as the TRAC records): A01 M1 on 1 and 9, A03 M1 on 5 and
13. `.work` and `.strd` checksummed and read back.

**Script:** NO, PLAY; at 3 s hold TRACK 1, hold PATTERN, TRIG 3; at 5 s the
MIDI key (MIDI mode), then the same gesture; at 6.8 s the MIDI key again; at
10 s PATTERN + TRIG 2 with no TRACK key held; STOP at 14 s. Watched: the
playing and queued pattern, `0x80000012`, `src`/`next` of T1 and M1, the
engine's global id, the per-track trig entry `0x46c7a810[0]` (a plain trig
writes `0x1d`: at `0x4009d42a` for T1, at `0x4009d0fc` in the MIDI step
handler for M1), the counters `0x800064d0` and `0x800064d8`.

| | result |
|---|---|
| before any change | T1 on steps 1, 5, 9, 13 and M1 on 1, 9 (A01) |
| TRACK 1 + PATTERN + TRIG 3 | `next[T1] = A03` (15.31 s); no global queue written; the toast reads `T1 ▸ A03` over the A01 screen (the display draws `>` as ▸) |
| T1's loop start | `src[T1] = A03` at 15.97 s; then T1 on 3, 7, 11, 15 round after round; M1 still on 1, 9 |
| MIDI mode, the same gesture | `0x80000012 = 1`; `next[M1] = A03` (17.81 s), T1 untouched |
| M1's loop start | `src[M1] = A03` at 19.13 s; then M1 on 5, 13; T1 still on A03 |
| PATTERN + TRIG 2, no TRACK held | stock's global queue (`0x800065c0 = 1`, 22.11 s); A02 plays at 22.27 s; at the next step the engine clears `src` and `next` of T1 and M1 and records A02 |
| after the global change | T1 on 2, 10 (A02's); M1 silent (A02 has no M1 trigs); no stray A03 step at the change |

Not run yet: MIDI out notes (M1 has no channel in the project copy, so
UART0 carried MIDI clock only) and note-offs across a switch; a bank change
under the port (host-tested only); the latched PATTERN (a tap, then TRACK +
TRIG); TRACK pressed after PATTERN; the rebuild path (an edit while a track
is on another pattern); a different length per track; Play Modes built
alongside; cycle counts; any unit.

## Not yet

The track's speed from its own pattern, persistence, trig LEDs per track,
cycle counts, any unit.
