/*
 * services/midi_clock.c (pico time stubbed): an external Start begins on
 * the next Clock, the downbeat, with the counter on a beat boundary, so
 * steps land on the DAW's grid. Run with test/run.sh.
 */

#include <assert.h>
#include <stdio.h>

#include "midi_clock.h"
#include "midi_in.h"

uint32_t g_now_ms;

static tiles_midi_in_realtime_callback_t s_rt;
bool tiles_midi_in_register_realtime_callback(tiles_midi_in_realtime_callback_t callback) {
    s_rt = callback;
    return true;
}

static void byte(uint8_t b) {
    s_rt(b, g_now_ms);
}

/* One main-loop pass: the clock scan, then op_mode's single read. */
static tiles_midi_clock_state_t pass(void) {
    tiles_midi_clock_scan();
    return tiles_midi_clock_get_state();
}

int main(void) {
    g_now_ms = 1000u;
    tiles_midi_clock_init();
    assert(s_rt != NULL);

    /* 1. clock already running (no Start): it's picked up as a start */
    for (int i = 0; i < 5; i++) {
        byte(0xF8);
    }
    tiles_midi_clock_state_t st = pass();
    assert(st.running && st.start_edge && st.pulse_count == 5u);

    /* 2. Stop, then a Start alone in a pass: nothing starts yet */
    g_now_ms += 10u;
    byte(0xFC);
    st = pass();
    assert(!st.running && !st.start_edge);
    byte(0xFA);
    st = pass();
    assert(!st.running && !st.start_edge);

    /* 3. the next Clock is the downbeat: start there, on a beat boundary */
    g_now_ms += 10u;
    byte(0xF8);
    st = pass();
    assert(st.running && st.start_edge && st.pulse_count % 24u == 0u);
    uint32_t downbeat = st.pulse_count;
    /* six more Clocks: exactly one sixteenth after the downbeat */
    for (int i = 0; i < 6; i++) {
        byte(0xF8);
    }
    st = pass();
    assert(st.running && !st.start_edge && st.pulse_count == downbeat + 6u);

    /* 4. Start and its Clock in the same pass (the usual USB case): same result */
    for (int i = 0; i < 7; i++) {
        byte(0xF8); /* leave the counter off a beat */
    }
    byte(0xFC);
    pass();
    byte(0xFA);
    byte(0xF8);
    st = pass();
    assert(st.running && st.start_edge && st.pulse_count % 24u == 0u);

    /* 5. a Start already on a beat boundary's eve stays put (no extra beat) */
    byte(0xFC);
    pass();
    for (int i = 0; i < 23; i++) {
        byte(0xF8);
    }
    pass();
    uint32_t before = tiles_midi_clock_get_state().pulse_count;
    assert(before % 24u == 23u);
    byte(0xFA);
    byte(0xF8);
    st = pass();
    assert(st.start_edge && st.pulse_count == before + 1u);

    /* 6. Continue resumes without a start edge; Stop cancels a waiting Start */
    byte(0xFC);
    pass();
    byte(0xFB);
    st = pass();
    assert(st.running && !st.start_edge);
    byte(0xFA);
    byte(0xFC);
    byte(0xF8);
    st = pass();
    assert(!st.running && !st.start_edge);

    /* 7. a Start whose Clock never comes is forgotten once the clock is gone */
    byte(0xFA);
    pass();
    g_now_ms += 600u; /* past the external-clock timeout */
    st = pass();
    assert(!st.running && !st.start_edge);

    printf("midi_clock: all tests pass\n");
    return 0;
}
