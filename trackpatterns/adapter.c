/* TRACK PATTERNS -- the firmware side: reads the sequencer's own state,
 * keeps the engine's state in loader-owned DRAM, and is what the detours
 * call. Every address is cited here or in INVESTIGATION.md.
 *
 * Compiled with -DTP_HOST, the firmware addresses become an array so the
 * glue can be tested on the host (test_adapter.c). */
#include "trackpattern.h"

#ifdef TP_HOST
volatile uint8_t *tp_host_addr(uint32_t address);
#define ADDR(a) tp_host_addr((uint32_t)(a))
#else
#define ADDR(a) ((volatile uint8_t *)(uintptr_t)(a))
#endif
#define U8(a)  (*(volatile uint8_t *)ADDR(a))
#define U32(a) (*(volatile uint32_t *)ADDR(a))

/* --- the sequencer's own state (INVESTIGATION.md "Anchors") --- */
enum {
    SEQ_TRANSPORT  = 0x800065b8u, /* 0 stopped, 1 playing (EUCLID, emu_frames.py) */
    SEQ_BANK       = 0x800065bdu, /* playing bank (DIRECT JUMP)                   */
    SEQ_PATTERN    = 0x800065beu, /* playing pattern                              */
    BANK_BLOB      = 0x400e21e0u, /* bank 0 in RAM (STEP_LOCKS.md section 1)      */
    BANK_STRIDE    = 0x9b340u,
    PATTERN_STRIDE = 0x8ed8u,
    AUDIO_TRACK    = 0x91au,      /* audio track record stride                    */
    MIDI_TRACKS    = 0x48d0u,     /* MIDI track records: pattern + 0x48d0 + t*0x8b0 */
    MIDI_TRACK     = 0x8b0u,
    TRK_LENGTH     = 0x50u,       /* audio track length (the advance's a4@(80))   */
    MIDI_LENGTH    = 0x28u,       /* MIDI track length (the advance's a4@(18680)) */
    PAT_LENGTH     = 0x8e53u,     /* pattern length (SCALE MODE NORMAL)           */
    PAT_SCALE_MODE = 0x8e55u,     /* 0 NORMAL, else PER TRACK                     */
};

/* --- the keys (INVESTIGATION.md "Anchors") --- */
enum {
    KEY_STATE   = 0x46c7d8eeu, /* + 24 * code: nonzero while the key is held (1; 2
                                  once a layer opened over it); output-matrix reads it too */
    KEY_STRIDE  = 24u,
    KEY_TRACK_1 = 0x10u,       /* TRACK 1..8 are key codes 0x10..0x17              */
    MIDI_MODE   = 0x80000012u, /* nonzero: the TRACK keys address M1..M8 (Play Modes) */
};

/* Loader-owned DRAM, zeroed at boot (hooks.s): zero = every track follows. */
extern TpState tp_state;
extern char tp_toast[16];

static unsigned tp_playing(void) { return U32(SEQ_TRANSPORT) == 1; }

static unsigned tp_global_pattern(void) {
    unsigned pattern = U8(SEQ_PATTERN);
    return pattern < TP_PATTERNS ? pattern : 0;
}

static unsigned tp_global_bank(void) {
    unsigned bank = U8(SEQ_BANK);
    return bank < 16 ? bank : 0;
}

/* Every entry first follows the global pattern: a change of it (key,
 * chain, arranger, a bank change too) clears the overrides, as v1
 * specifies. */
static unsigned tp_sync(void) {
    unsigned global = tp_global_pattern();
    tp_global(&tp_state, tp_global_bank() * TP_PATTERNS + global);
    return global;
}

/* The tick, once per track step, before the step handler: the pattern
 * argument the handler gets for this track. Tracks 8..15 are M1..M8.
 * `stock` and `stock_bank` are what stock chose: the playing pattern, or
 * (just before a global pattern change lands) the next one, which the tick
 * reads from 0x800065c1 / 0x800065c2 for the step it prepares across the
 * change. Then every track follows stock: the change is about to clear the
 * overrides anyway. */
unsigned tp_seq_pattern(unsigned track, unsigned raw, unsigned stock, unsigned stock_bank) {
    unsigned global = tp_sync();
    unsigned pattern = tp_step(&tp_state, track, raw, global);
    return stock == global && stock_bank == tp_global_bank() ? pattern : stock;
}

/* The rebuild after an edit, and any look-ahead: the pattern the track
 * will read on `raw`. While stopped the next run starts every track over,
 * so a queued change has landed by then. Changes nothing. */
unsigned tp_peek_pattern(unsigned track, unsigned raw, unsigned len, unsigned stock) {
    unsigned global = tp_sync();
    if (stock != global) return stock;
    if (!tp_playing()) return tp_pattern(&tp_state, track, global);
    return tp_peek(&tp_state, track, raw, len, global);
}

/* The pattern whose length a track loops at. */
unsigned tp_length_pattern(unsigned track) {
    unsigned global = tp_sync();
    return tp_pattern(&tp_state, track, global);
}

/* The per-track advance, as it compares the track's counter with its
 * length: the length to use. A track that follows the global pattern
 * keeps `stock_len` (what stock read); a track on another pattern reads the
 * same field from that pattern: its own length under SCALE MODE PER TRACK,
 * the pattern length under NORMAL (the playing pattern's mode decides, as
 * in stock). */
unsigned tp_track_length(unsigned track, unsigned stock_len) {
    unsigned global = tp_sync();
    unsigned pattern = tp_pattern(&tp_state, track, global);
    if (pattern == global || track >= TP_TRACKS) return stock_len;
    unsigned bank = U8(SEQ_BANK);
    if (bank > 15) return stock_len;
    uint32_t playing = BANK_BLOB + bank * BANK_STRIDE + global * PATTERN_STRIDE;
    uint32_t source = BANK_BLOB + bank * BANK_STRIDE + pattern * PATTERN_STRIDE;
    uint32_t at;
    if (!U8(playing + PAT_SCALE_MODE))
        at = source + PAT_LENGTH;
    else if (track < 8)
        at = source + track * AUDIO_TRACK + TRK_LENGTH;
    else
        at = source + MIDI_TRACKS + (track - 8) * MIDI_TRACK + MIDI_LENGTH;
    unsigned len = U8(at);
    return len ? len : stock_len;
}

/* PLAY (or an external start): every track starts its first pass again. */
void tp_transport_start(void) {
    tp_restart(&tp_state);
}

/* The key gesture, from the key handler: queue `pattern` (0..15) for
 * `track` and write the popup text into tp_toast (hooks.s opens the stock
 * toast; key-handler context only). Returns 0 when nothing was queued. */
unsigned tp_key(unsigned track, unsigned pattern) {
    unsigned global = tp_sync();
    unsigned code = tp_queue(&tp_state, track, pattern, global, tp_playing());
    if (!code) return 0;
    tp_label(&tp_state, track, U8(SEQ_BANK), tp_toast);
    return 1;
}

/* PATTERN held and a trig pressed (hooks.s, inside the PATTERN layer's trig
 * handler): with a TRACK key held as well, the trig queues its pattern for
 * that track instead of changing the global pattern. The lowest held TRACK
 * key wins. Returns 0 to leave the press to stock, 1 when it took the press,
 * 2 when it took it and queued a change (tp_toast filled). */
unsigned tp_gesture(unsigned pattern) {
    for (unsigned n = 0; n < 8; ++n)
        if (U32(KEY_STATE + KEY_STRIDE * (KEY_TRACK_1 + n)))
            return 1u + tp_key(U32(MIDI_MODE) ? n + 8u : n, pattern);
    return 0;
}
