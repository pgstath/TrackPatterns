"""TRACK PATTERNS -- each track plays its own pattern of the playing bank,
clip style: a change is queued per track and lands at the end of that
track's own loop. Everything else (tempo, the Part's machines, samples and
FX, scenes, MASTER LENGTH, chains) stays stock.

How: where the sequencer hands a track's step to the step handler
(trackpattern.c, adapter.c), the pattern argument is replaced by the
track's own; where the per-track advance compares the track's counter with
its length, the length comes from that pattern. A track with no change set
gets exactly what stock passes.

The gesture: hold [TRACK n], hold [PATTERN], press [TRIG k] queues pattern
k for track n (M1..M8 in MIDI mode); without a TRACK key held, PATTERN +
TRIG is stock's global change, which clears every override.

STATUS (21 Oct 2026): in progress. Engine and glue host-tested; the sites
below were read from the contributor's original 1.40C (INVESTIGATION.md
section 4) and are guarded by their stock bytes. Run under the ColdFire
emulator only (TESTING.md); not run on a unit.

  0x400a2d76  audio tick: the pattern argument of 0x4009d1e8
  0x400a39c0  MIDI tick: the pattern argument of 0x4009cf4c
  0x4009dc8c  audio rebuild after an edit: the pattern argument (8, padded)
  0x4009e3e2  MIDI rebuild: the same (8, padded)
  0x400a3d88  audio per-track advance: the length the counter wraps at
  0x400a3ea8  MIDI per-track advance: the same
  0x40056b50  PATTERN layer's trig press: the gesture, with a TRACK key held

Play Modes' step-load and rebuild detours (0x400a2d6a, 0x400a39b6,
0x4009dc86, 0x4009e3dc) end where these begin; the two share no byte.
"""
from remix.schema import Category, Proof, Detour, Kind, Linked, Module, Gate
from remix.stock_guard import stock_guard

MODULE = Module(
    name="trackpatterns",
    key="TRACK PATTERNS",
    kind=Kind.CF_PATCH,
    category=Category.MACHINES,
    author="pgstath", author_url="https://github.com/pgstath",
    proof=Proof.UNTESTED,
    proof_note="host-tested; run under the ColdFire emulator; not on a unit",
    doc="Each track plays its own pattern of the playing bank; changes land at the track's loop end.",
    linked=(Linked("trackpatterns", "modules/trackpatterns/trackpatterns.s", cpu="5475", dram=True),),
    detours=(
        Detour(0x400a2d76, stock_guard(0x400a2d76, 6, "6572f739d291e58d90068dcc596390f28504a68302bb9fd6af24db122f45f2cf"),
               "trackpatterns", "tp_audio_tick", "audio tick: the track's own pattern", kind="jmp"),
        Detour(0x400a39c0, stock_guard(0x400a39c0, 6, "96b24804daf26f7a219dbf5241600694b5f7ba31d25710d7cfc0f1554765bac5"),
               "trackpatterns", "tp_midi_tick", "MIDI tick: the track's own pattern", kind="jmp"),
        Detour(0x4009dc8c, stock_guard(0x4009dc8c, 8, "8cdf350b8a4791708d81db95615cef0b8be141469a8af636fed563b79dfafa96"),
               "trackpatterns", "tp_audio_rebuild", "audio rebuild: the track's own pattern", kind="jmp", pad_to=8),
        Detour(0x4009e3e2, stock_guard(0x4009e3e2, 8, "2aece201acf0729de6a6d5aa9e53caca65230484ce964bea9ec392f49ab03844"),
               "trackpatterns", "tp_midi_rebuild", "MIDI rebuild: the track's own pattern", kind="jmp", pad_to=8),
        Detour(0x400a3d88, stock_guard(0x400a3d88, 6, "5fba35b710b7a957fc2c3365e60cb7580ef1b87f7e3269297dc4c6c148cd1ccb"),
               "trackpatterns", "tp_audio_length", "audio advance: the length of the track's own pattern", kind="jmp"),
        Detour(0x400a3ea8, stock_guard(0x400a3ea8, 6, "5fba35b710b7a957fc2c3365e60cb7580ef1b87f7e3269297dc4c6c148cd1ccb"),
               "trackpatterns", "tp_midi_length", "MIDI advance: the length of the track's own pattern", kind="jmp"),
        Detour(0x40056b50, stock_guard(0x40056b50, 6, "5c03ce1dea644685012b6b9fec8445e2efec2d2e96818c3c842fc4311dbbc55a"),
               "trackpatterns", "tp_pattern_key", "PATTERN + TRIG with a TRACK key held: that track's pattern", kind="jmp"),
    ),
    gates=(Gate("modules/trackpatterns/verify.py", remix_arg=False),),
)
