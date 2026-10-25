/*
 * services/physics_core.c -- the physics modes: Ripples' rings land on
 * sixteenths and reach as far as the strike was hard; Sand pours faster
 * pressed harder, piles, slides and runs out; Orbits keep whole-beat
 * periods, stop and carry on on the grid, and never fly off under Pull;
 * the cradle knocks on the beat (long-short with Shuffle) and comes to
 * rest. Run with test/run.sh.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "physics_core.h"

#define DT (1.0 / 256.0) /* a step, in beats */

static bool on_grid(double t, double per_beat) {
    double x = t * per_beat;
    return fabs(x - floor(x + 0.5)) < 1e-6;
}

static int notes_on(const tiles_phys_out_t *out, uint8_t cell) {
    int n = 0;
    for (uint8_t i = 0; i < out->note_count; i++) {
        n += out->note_cell[i] == cell ? 1 : 0;
    }
    return n;
}

static int velocity_on(const tiles_phys_out_t *out, uint8_t cell) {
    for (uint8_t i = 0; i < out->note_count; i++) {
        if (out->note_cell[i] == cell) {
            return out->note_velocity[i];
        }
    }
    return 0;
}

static tiles_ripples_t r;
static tiles_sand_t s;
static tiles_orbits_t o;
static tiles_cradle_t c;

static void test_ripples(void) {
    tiles_phys_out_t out;
    double t = 10.1;
    /* 1. A light strike: its cell at once, the neighbours on later
     * sixteenths, nothing far away. */
    tiles_ripples_clear(&r, 7u, t);
    tiles_phys_out_clear(&out);
    tiles_ripples_strike(&r, 14u, 0.3f, t, &out); /* row 2, col 2 */
    assert(out.note_count == 1u && out.note_cell[0] == 14u);
    tiles_ripples_release(&r, 14u);
    int near = 0, far = 0;
    for (int i = 0; i < 1024; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_ripples_step(&r, t, &out);
        for (uint8_t k = 0; k < out.note_count; k++) {
            assert(on_grid(t - fmod(t, 0.25), 4.0)); /* sanity of the helper */
            uint8_t cell = out.note_cell[k];
            float dx = (float)(cell % 6) - 2.0f, dy = (float)(cell / 6) - 2.0f;
            if (sqrtf(dx * dx + dy * dy) <= 1.5f) {
                near++;
            } else {
                far++;
            }
        }
    }
    assert(near >= 4 && far <= 4);
    assert(!tiles_ripples_active(&r));

    /* 2. A hard strike reaches across the grid, every hit on a sixteenth,
     * softer further out. */
    tiles_ripples_clear(&r, 7u, t);
    tiles_phys_out_clear(&out);
    tiles_ripples_strike(&r, 0u, 1.0f, t, &out);
    tiles_ripples_release(&r, 0u);
    int v_near = 0, v_far = 0, hits = 0;
    double last_hit = -1.0;
    for (int i = 0; i < 2048; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_ripples_step(&r, t, &out);
        if (out.note_count > 0u) {
            hits += out.note_count;
            /* within a step of a sixteenth */
            double frac = t * 4.0 - floor(t * 4.0);
            assert(frac < DT * 4.0 + 1e-9);
            assert(t > last_hit);
            last_hit = t;
        }
        if (notes_on(&out, 1u)) {
            v_near = velocity_on(&out, 1u);
        }
        if (notes_on(&out, 23u)) {
            v_far = velocity_on(&out, 23u);
        }
    }
    assert(hits >= 15);
    assert(v_near > 0 && v_near > v_far);

    /* 3. Holding: a ring from the held cell on every beat. */
    tiles_ripples_clear(&r, 7u, 20.5);
    t = 20.5;
    tiles_phys_out_clear(&out);
    tiles_ripples_strike(&r, 9u, 0.6f, t, &out);
    tiles_ripples_set_pressure(&r, 9u, 1.0f);
    int beats = 0;
    for (int i = 0; i < 4 * 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_ripples_step(&r, t, &out);
        if (notes_on(&out, 9u)) {
            assert(on_grid(floor(t + 1e-9), 1.0) && t - floor(t) < 2.0 * DT);
            beats++;
        }
    }
    assert(beats == 4); /* 21 is under 0.3 beats away: 22, 23, 24, 25 */
    /* 4. Circle + tap: its rings go. */
    assert((tiles_ripples_erasable(&r) >> 9) & 1u);
    assert(tiles_ripples_erase(&r, 9u));
    assert(!tiles_ripples_erase(&r, 9u));
    tiles_ripples_release(&r, 9u);

    /* 5. Splash: drops land on eighths, a pad or two away. */
    tiles_ripples_clear(&r, 99u, 30.0);
    r.splash = true;
    t = 30.03;
    tiles_phys_out_clear(&out);
    tiles_ripples_strike(&r, 8u, 1.0f, t, &out);
    tiles_ripples_release(&r, 8u);
    int drops = 0;
    for (uint8_t i = 0; i < TILES_RIPPLES_MAX_DROPS; i++) {
        if (r.drop[i].live) {
            drops++;
            assert(on_grid(r.drop[i].at, 2.0) && r.drop[i].at > t);
            float dx = (float)(r.drop[i].to % 6) - 2.0f, dy = (float)(r.drop[i].to / 6) - 1.0f;
            float d = sqrtf(dx * dx + dy * dy);
            assert(d >= 1.4f && d <= 2.9f);
        }
    }
    assert(drops == 2);
}

static void test_sand(void) {
    tiles_phys_out_t out;
    const float spb = 0.5f; /* 120 BPM */
    assert(tiles_sand_division(0.0f) == 1u && tiles_sand_division(0.3f) == 2u);
    assert(tiles_sand_division(0.6f) == 4u && tiles_sand_division(1.0f) == 8u);

    /* 1. A tap drops one grain: it lands on the bottom row and plays. */
    double t = 4.0;
    tiles_sand_clear(&s, t);
    tiles_sand_strike(&s, 2u, t);
    tiles_sand_release(&s, 2u);
    int landed = 0;
    while (t < 5.9) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
        landed += notes_on(&out, 20u); /* row 3, col 2 */
    }
    assert(landed == 1 && s.h[2] == 1u);
    /* The hourglass: beat 6 takes it, with a soft note on the floor. */
    int leaked = 0;
    while (t < 6.1) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
        leaked += notes_on(&out, 20u);
    }
    assert(leaked == 1 && s.h[2] == 0u);

    /* 2. Pouring hard: eighths of a beat... a full press pours 8 a beat. */
    tiles_sand_clear(&s, 0.0);
    t = 0.0;
    tiles_sand_strike(&s, 3u, t);
    tiles_sand_set_pressure(&s, 3u, 1.0f);
    for (int i = 0; i < 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
    }
    tiles_sand_release(&s, 3u);
    for (int i = 0; i < 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
    }
    unsigned total = 0;
    for (int col = 0; col < 6; col++) {
        total += s.h[col];
    }
    /* 1 + 8 poured, minus the leak at beat 2 (one per pile), and the
     * pile spread by avalanches over the neighbours. */
    assert(total >= 5u && total <= 9u);
    assert(s.h[2] > 0u || s.h[4] > 0u);
    assert(s.h[3] >= s.h[2] && s.h[3] >= s.h[4]);

    /* 3. Circle + tap empties the column, its grains as notes. */
    uint8_t before = s.h[3];
    assert(before > 0u);
    assert(tiles_sand_erase(&s, 3u, t, spb) && s.h[3] == 0u);
    int drained = 0;
    for (int i = 0; i < 128; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
        drained += out.note_count;
    }
    assert(drained >= before);

    /* 4. Flip: the piles fall to the top. */
    tiles_sand_clear(&s, t);
    s.h[0] = 6u;
    tiles_sand_flip(&s, true);
    assert(s.h[0] == 0u && s.gdir == -1);
    int top_notes = 0;
    for (int i = 0; i < 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_sand_step(&s, t, spb, &out);
        top_notes += notes_on(&out, 0u) + notes_on(&out, 6u);
    }
    assert(s.h[0] >= 4u && top_notes >= 4);
}

static void test_orbits(void) {
    tiles_phys_out_t out;
    assert(tiles_orbits_period(1.0f) == 1.0f);
    assert(tiles_orbits_period(sqrtf(2.0f)) == 1.5f);
    assert(tiles_orbits_period(2.0f) == 3.0f);
    assert(tiles_orbits_period(sqrtf(5.0f)) == 3.0f);
    assert(tiles_orbits_period(3.0f) == 6.0f);
    assert(tiles_orbits_period(4.0f) == 8.0f);

    memset(&o, 0, sizeof(o));
    o.running = true;
    double t = 3.37;
    tiles_orbits_clear(&o, t);
    /* 1. The first strike is the sun; a tap after it, a planet. */
    assert(tiles_orbits_strike_places_sun(&o, 14u));
    assert(!tiles_orbits_strike_places_sun(&o, 15u));
    tiles_orbits_launch(&o, 15u, 100u, t);  /* a pad away: every beat */
    tiles_orbits_launch(&o, 2u, 90u, t);    /* two rows up: every 3 */
    assert(o.planet[0].period == 1.0f && o.planet[1].period == 3.0f);
    double prev15 = -1.0, prev2 = -1.0;
    int n15 = 0, n2 = 0;
    for (int i = 0; i < 12 * 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_orbits_step(&o, t, &out);
        if (notes_on(&out, 15u)) {
            if (prev15 >= 0.0) {
                assert(fabs(t - prev15 - 1.0) < 2.0 * DT);
            }
            assert(t * 4.0 - floor(t * 4.0) < 2.0 * DT * 4.0); /* on a sixteenth */
            prev15 = t;
            n15++;
        }
        if (notes_on(&out, 2u)) {
            if (prev2 >= 0.0) {
                assert(fabs(t - prev2 - 3.0) < 2.0 * DT);
            }
            prev2 = t;
            n2++;
        }
        float x, y;
        tiles_orbits_planet_xy(&o, 0u, t, &x, &y);
        assert(fabsf(hypotf(x - 2.5f, y - 2.5f) - 1.0f) < 1e-3f);
    }
    assert(n15 == 12 && n2 == 4);

    /* 2. Stop: frozen and silent; play: on from a whole beat later. */
    tiles_orbits_stop(&o, t);
    float x0, y0, x1, y1;
    tiles_orbits_planet_xy(&o, 0u, t, &x0, &y0);
    for (int i = 0; i < 300; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_orbits_step(&o, t, &out);
        assert(out.note_count == 0u);
    }
    tiles_orbits_planet_xy(&o, 0u, t, &x1, &y1);
    assert(x0 == x1 && y0 == y1);
    tiles_orbits_play(&o, t);
    assert(fmod(o.offset, 1.0) < 1e-9 || fmod(o.offset, 1.0) > 1.0 - 1e-9);
    prev15 = -1.0;
    for (int i = 0; i < 6 * 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_orbits_step(&o, t, &out);
        if (notes_on(&out, 15u)) {
            assert(t * 4.0 - floor(t * 4.0) < 2.0 * DT * 4.0);
            if (prev15 >= 0.0) {
                assert(fabs(t - prev15 - 1.0) < 2.0 * DT);
            }
            prev15 = t;
        }
    }
    assert(prev15 > 0.0);

    /* 3. Pull: a crowd of planets stays bound for 64 beats; off again,
     * every one plays on the sixteenths at its own period. */
    for (uint8_t cell = 0; cell < 24u; cell += 3u) {
        tiles_orbits_launch(&o, cell, 100u, t);
    }
    tiles_orbits_set_pull(&o, true, t);
    for (int i = 0; i < 64 * 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_orbits_step(&o, t, &out);
        for (uint8_t k = 0; k < TILES_ORBITS_MAX_PLANETS; k++) {
            if (o.planet[k].live) {
                assert(o.planet[k].r >= 0.5f - 1e-4f && o.planet[k].r <= o.planet[k].r0 + 1.5f + 1e-4f);
            }
        }
    }
    tiles_orbits_set_pull(&o, false, t);
    double last[TILES_ORBITS_MAX_PLANETS];
    for (int k = 0; k < (int)TILES_ORBITS_MAX_PLANETS; k++) {
        last[k] = -1.0;
    }
    for (int i = 0; i < 16 * 256; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_orbits_step(&o, t, &out);
        for (uint8_t k = 0; k < TILES_ORBITS_MAX_PLANETS; k++) {
            if (o.planet[k].live && notes_on(&out, o.planet[k].home)) {
                assert(t * 4.0 - floor(t * 4.0) < 2.0 * DT * 4.0);
                if (last[k] >= 0.0) {
                    assert(fabs(t - last[k] - (double)o.planet[k].period) < 2.0 * DT);
                }
                last[k] = t;
            }
        }
    }

    /* 4. Circle + tap: a sun goes with its planets. */
    assert((tiles_orbits_erasable(&o) >> 14) & 1u);
    assert(tiles_orbits_erase(&o, 14u));
    assert(!tiles_orbits_active(&o));
    assert(tiles_orbits_sun_count(&o) == 0u);
}

static void test_cradle(void) {
    tiles_phys_out_t out;
    tiles_cradle_clear(&c);
    double t = 7.3;
    /* 1. Two balls lifted two rows from the left end (row 1, col 1). */
    tiles_phys_out_clear(&out);
    tiles_cradle_strike(&c, 7u, 100u, t, &out);
    assert(out.note_count == 0u && tiles_cradle_active(&c));
    t += 0.5;
    tiles_cradle_release(&c, 7u, t); /* let go at 7.8: first knock at 8.0? no, 8.5 (0.2 on) */
    double knocks[40];
    int n = 0;
    for (int i = 0; i < 40 * 256 && n < 40; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_cradle_step(&c, t, &out);
        if (out.note_count > 0u) {
            /* the balls on the other end fly out: columns 6 and 5 */
            assert(out.note_count == 2u);
            bool right = notes_on(&out, 23u) && notes_on(&out, 22u);
            bool left = notes_on(&out, 18u) && notes_on(&out, 19u);
            assert(right != left);
            assert((n % 2 == 0) == right);
            knocks[n++] = t;
        }
    }
    assert(fabs(knocks[0] - 8.0) < 2.0 * DT || fabs(knocks[0] - 8.5) < 2.0 * DT);
    for (int i = 1; i < n; i++) {
        assert(fabs(knocks[i] - knocks[i - 1] - 1.0) < 2.0 * DT);
    }
    assert(n >= 15 && n < 40 && !tiles_cradle_active(&c)); /* it comes to rest */

    /* 2. Shuffle: long-short. */
    tiles_cradle_clear(&c);
    c.shuffle = true;
    t = 100.0;
    tiles_phys_out_clear(&out);
    tiles_cradle_strike(&c, 0u, 100u, t, &out); /* one ball, top row: high */
    tiles_cradle_release(&c, 0u, t);
    n = 0;
    for (int i = 0; i < 8 * 256 && n < 6; i++) {
        t += DT;
        tiles_phys_out_clear(&out);
        tiles_cradle_step(&c, t, &out);
        if (out.note_count > 0u) {
            knocks[n++] = t;
        }
    }
    assert(n == 6);
    for (int i = 1; i < n; i++) {
        double gap = knocks[i] - knocks[i - 1];
        double want = i % 2 == 1 ? 2.0 / 3.0 : 4.0 / 3.0; /* out right (short), back left (long) */
        assert(fabs(gap - want) < 2.0 * DT);
    }

    /* 3. A tap on the bottom row plays that ball; circle + tap catches. */
    tiles_phys_out_clear(&out);
    tiles_cradle_strike(&c, 21u, 90u, t, &out);
    assert(out.note_count == 1u && out.note_cell[0] == 21u && out.note_velocity[0] == 90u);
    assert(tiles_cradle_erasable(&c) & (1u << 18));
    assert(tiles_cradle_erase(&c, 18u) && !tiles_cradle_active(&c));
}

int main(void) {
    test_ripples();
    test_sand();
    test_orbits();
    test_cradle();
    printf("physics_core: all tests pass\n");
    return 0;
}
