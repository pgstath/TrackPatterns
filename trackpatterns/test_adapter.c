/* Host tests for adapter.c, built with -DTP_HOST: the firmware addresses
 * the adapter reads are backed by a plain array. No firmware. */
#include <stdio.h>
#include <string.h>
#include "trackpattern.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* What hooks.s allocates on the unit. */
TpState tp_state;
char tp_toast[16];

unsigned tp_seq_pattern(unsigned track, unsigned raw, unsigned stock, unsigned stock_bank);
unsigned tp_peek_pattern(unsigned track, unsigned raw, unsigned len, unsigned stock);
unsigned tp_length_pattern(unsigned track);
unsigned tp_track_length(unsigned track, unsigned stock_len);
unsigned tp_key(unsigned track, unsigned pattern);
unsigned tp_gesture(unsigned pattern);
void tp_transport_start(void);

static uint8_t seq[0x100];             /* 0x80006500.. */
static uint8_t blob[4 * 0x9b340];      /* banks A..D at 0x400e21e0 */
static uint8_t keys[64 * 24];          /* key states at 0x46c7d8ee + 24 * code */
static uint8_t midi_mode[4];           /* 0x80000012 */

volatile uint8_t *tp_host_addr(uint32_t a) {
    if (a >= 0x80006500u && a < 0x80006500u + sizeof seq) return seq + (a - 0x80006500u);
    if (a >= 0x400e21e0u && a < 0x400e21e0u + sizeof blob) return blob + (a - 0x400e21e0u);
    if (a >= 0x46c7d8eeu && a < 0x46c7d8eeu + sizeof keys) return keys + (a - 0x46c7d8eeu);
    if (a == 0x80000012u) return midi_mode;
    printf("FAIL: adapter read an unexpected address 0x%08x\n", a);
    ++failures;
    static uint8_t junk[8];
    return junk;
}

static void set_transport(uint32_t v) { memcpy(seq + 0xb8, &v, 4); }
static void set_playing(unsigned bank, unsigned pattern) { seq[0xbd] = (uint8_t)bank; seq[0xbe] = (uint8_t)pattern; }

/* `steps` stock steps of one track looping at `len` from `from`. */
static void run(unsigned track, unsigned len, unsigned from, unsigned steps, unsigned *out) {
    for (unsigned i = 0; i < steps; ++i) out[i] = tp_seq_pattern(track, (from + i) % len, seq[0xbe], seq[0xbd]);
}

static uint8_t *pattern(unsigned bank, unsigned p) { return blob + bank * 0x9b340u + p * 0x8ed8u; }
static void key_state(unsigned code, uint32_t v) { memcpy(keys + 24 * code, &v, 4); }

int main(void) {
    unsigned out[64];
    set_transport(1);
    set_playing(0, 0);                                    /* A01 */

    /* Stock: every track reads the global pattern. */
    run(0, 16, 0, 20, out);
    for (unsigned i = 0; i < 20; ++i) CHECK(out[i] == 0, "T1 reads A01");
    CHECK(tp_seq_pattern(12, 0, 0, seq[0xbd]) == 0 && tp_length_pattern(5) == 0, "others too");

    /* Playing: T5 queues A03 mid-pass; it lands at T5's loop end. */
    run(4, 16, 0, 6, out);
    CHECK(tp_key(4, 2) == 1 && strcmp(tp_toast, "T5 > A03") == 0, "toast %s", tp_toast);
    CHECK(tp_length_pattern(4) == 0, "T5 still loops at A01's length");
    CHECK(tp_peek_pattern(4, 6, 16, 0) == 0 && tp_peek_pattern(4, 16, 16, 0) == 2,
          "look-ahead: A01 this pass, A03 from the loop start");
    run(4, 16, 6, 10 + 16, out);
    for (unsigned i = 0; i < 10; ++i) CHECK(out[i] == 0, "A01 until the loop end (%u)", i);
    for (unsigned i = 10; i < 26; ++i) CHECK(out[i] == 2, "A03 from step 1 (%u)", i);
    CHECK(tp_length_pattern(4) == 2, "now T5 loops at A03's length");
    CHECK(tp_seq_pattern(0, 4, 0, seq[0xbd]) == 0, "T1 stays on A01");

    /* MIDI tracks: M3 (track 10) on A16, a different loop length. */
    run(10, 12, 0, 5, out);
    CHECK(tp_key(10, 15) == 1 && strcmp(tp_toast, "M3 > A16") == 0, "toast %s", tp_toast);
    run(10, 12, 5, 7 + 12, out);
    CHECK(out[6] == 0 && out[7] == 15 && out[18] == 15, "M3 on A16 from its own loop start");

    /* The global pattern's key brings a track back. */
    run(4, 16, 0, 3, out);
    CHECK(tp_key(4, 0) == 1 && strcmp(tp_toast, "T5 FOLLOWS") == 0, "toast %s", tp_toast);
    run(4, 16, 3, 13 + 1, out);
    CHECK(out[12] == 2 && out[13] == 0, "T5 back on A01 at its loop end");

    /* A global pattern change (key, chain, arranger) clears every override. */
    CHECK(tp_seq_pattern(10, 0, 0, seq[0xbd]) == 15, "M3 still on A16");
    set_playing(0, 5);                                    /* A06 */
    CHECK(tp_seq_pattern(10, 1, 5, seq[0xbd]) == 5 && tp_seq_pattern(4, 1, 5, seq[0xbd]) == 5, "everyone on A06");
    CHECK(tp_length_pattern(10) == 5, "lengths from A06 too");

    /* Stopped: a key applies at once, and the rebuild prepares it. */
    set_transport(0);
    CHECK(tp_key(1, 7) == 1 && strcmp(tp_toast, "T2 > A08") == 0, "toast %s", tp_toast);
    CHECK(tp_peek_pattern(1, 0, 16, 5) == 7, "stopped: the rebuild prepares A08");
    set_transport(1);
    tp_transport_start();
    CHECK(tp_seq_pattern(1, 0, 5, seq[0xbd]) == 7, "PLAY plays A08 on T2 from step 1");

    /* Bank letter in the popup; out-of-range keys queue nothing. */
    set_playing(3, 5);                                    /* D06: a global change */
    CHECK(tp_key(0, 8) == 1 && strcmp(tp_toast, "T1 > D09") == 0, "toast %s", tp_toast);
    CHECK(tp_key(16, 0) == 0 && tp_key(0, 16) == 0, "out of range");
    CHECK(tp_seq_pattern(20, 3, 5, seq[0xbd]) == 5, "an unknown track reads the global pattern");


    /* Stock prepares a step of the next pattern just before a global change
     * lands (0x800065c1): every track follows stock for that step. */
    set_playing(0, 0);
    tp_seq_pattern(2, 0, 0, seq[0xbd]);
    tp_key(2, 4);                                         /* T3 queues A05 */
    for (unsigned r = 1; r < 16; ++r) tp_seq_pattern(2, r, 0, seq[0xbd]);
    CHECK(tp_seq_pattern(2, 0, 0, seq[0xbd]) == 4, "T3 on A05 from its loop start");
    CHECK(tp_seq_pattern(2, 1, 9, seq[0xbd]) == 9, "a step of the next global pattern (A10): stock's choice wins");
    CHECK(tp_peek_pattern(2, 2, 16, 9) == 9, "the rebuild too");
    set_playing(0, 9);
    CHECK(tp_seq_pattern(2, 2, 9, seq[0xbd]) == 9, "after the change: A10, override cleared");

    /* A bank change to the same pattern number (A10 -> B10) is a change too,
     * and the step stock prepares across it is B10's on every track. */
    set_transport(0);
    tp_key(2, 4);                                         /* stopped: T3 on A05 at once */
    set_transport(1);
    CHECK(tp_seq_pattern(2, 3, 9, 0) == 4, "T3 on A05");
    CHECK(tp_seq_pattern(2, 4, 9, 1) == 9, "the step prepared across A10 -> B10: stock's");
    seq[0xbd] = 1;                                        /* B10 plays */
    CHECK(tp_seq_pattern(2, 5, 9, 1) == 9 && tp_length_pattern(2) == 9, "B10: override cleared");

    /* Length: a track on another pattern loops at that pattern's length. */
    set_playing(0, 0);
    uint8_t *a01 = pattern(0, 0), *a03 = pattern(0, 2), *a16 = pattern(0, 15);
    a01[0x8e55] = 1;                                      /* A01 PER TRACK */
    a03[4 * 0x91a + 0x50] = 8;                            /* A03 T5: 8 steps */
    a16[0x48d0 + 2 * 0x8b0 + 0x28] = 12;                  /* A16 M3: 12 steps */
    a03[0x8e53] = 24;                                     /* A03's NORMAL length */
    tp_seq_pattern(4, 0, 0, seq[0xbd]);
    CHECK(tp_track_length(4, 16) == 16, "T5 following: stock's length");
    tp_key(4, 2);
    for (unsigned r = 1; r < 16; ++r) tp_seq_pattern(4, r, 0, seq[0xbd]);
    tp_seq_pattern(4, 0, 0, seq[0xbd]);                              /* T5 on A03 now */
    CHECK(tp_track_length(4, 16) == 8, "T5 on A03 (PER TRACK): its own 8 steps (%u)", tp_track_length(4, 16));
    a01[0x8e55] = 0;                                      /* A01 NORMAL */
    CHECK(tp_track_length(4, 16) == 24, "NORMAL: A03's pattern length (%u)", tp_track_length(4, 16));
    a01[0x8e55] = 1;
    tp_seq_pattern(10, 0, 0, seq[0xbd]); tp_key(10, 15);
    for (unsigned r = 1; r < 16; ++r) tp_seq_pattern(10, r, 0, seq[0xbd]);
    tp_seq_pattern(10, 0, 0, seq[0xbd]);
    CHECK(tp_track_length(10, 16) == 12, "M3 on A16: the MIDI record's length (%u)", tp_track_length(10, 16));
    a16[0x48d0 + 2 * 0x8b0 + 0x28] = 0;
    CHECK(tp_track_length(10, 16) == 16, "an empty length byte keeps stock's");
    CHECK(tp_track_length(0, 16) == 16, "T1 follows: stock's");

    /* The gesture: no TRACK key held leaves PATTERN + TRIG to stock. */
    set_playing(1, 0);                                    /* B01: a global change */
    key_state(0x2e, 1);                                   /* PATTERN held */
    key_state(0x18, 1);                                   /* a key past TRACK 8 */
    CHECK(tp_gesture(4) == 0 && tp_peek_pattern(1, 16, 16, 0) == 0, "no TRACK held: stock's");
    /* TRACK 2 held before PATTERN (2: a layer opened over it): T2 queues B05. */
    key_state(0x11, 2);
    CHECK(tp_gesture(4) == 2 && strcmp(tp_toast, "T2 > B05") == 0, "toast %s", tp_toast);
    CHECK(tp_peek_pattern(1, 16, 16, 0) == 4 && tp_peek_pattern(0, 16, 16, 0) == 0,
          "T2 queued, T1 not");
    /* The lowest held TRACK key wins; MIDI mode addresses M1..M8. */
    key_state(0x14, 1);
    CHECK(tp_gesture(9) == 2 && strcmp(tp_toast, "T2 > B10") == 0, "toast %s", tp_toast);
    key_state(0x11, 0);
    midi_mode[3] = 1;
    CHECK(tp_gesture(2) == 2 && strcmp(tp_toast, "M5 > B03") == 0, "toast %s", tp_toast);
    CHECK(tp_peek_pattern(12, 16, 16, 0) == 2 && tp_peek_pattern(4, 16, 16, 0) == 0, "M5, not T5");
    /* The global pattern's trig: M5 follows again. */
    CHECK(tp_gesture(0) == 2 && strcmp(tp_toast, "M5 FOLLOWS") == 0, "toast %s", tp_toast);
    midi_mode[3] = 0;
    key_state(0x14, 0);
    CHECK(tp_gesture(7) == 0, "TRACK released: stock's again");

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("TRACK PATTERNS adapter: all host tests passed\n");
    return 0;
}
