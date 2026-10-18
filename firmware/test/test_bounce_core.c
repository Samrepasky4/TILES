/*
 * services/bounce_core.c -- Bouncing notes' balls: periods, the drop, the
 * landings and the polyrhythm between heights. Run with test/run.sh.
 */

#include <assert.h>
#include <stdio.h>

#include "bounce_core.h"

int main(void) {
    tiles_bounce_t b;
    tiles_bounce_clear(&b);

    /* 1. periods: every 1, 2, 4, 6 steps for heights 1-4 */
    assert(tiles_bounce_period(0) == 0 && tiles_bounce_period(1) == 1);
    assert(tiles_bounce_period(2) == 2 && tiles_bounce_period(3) == 4 && tiles_bounce_period(4) == 6);

    /* 2. off lanes never land */
    for (int i = 0; i < 20; i++) {
        assert(tiles_bounce_step(&b) == 0u);
    }

    /* 3. a ball dropped from 4 starts at the top, falls, lands, climbs back */
    tiles_bounce_set_height(&b, 0, 4);
    assert(tiles_bounce_ball_height(&b, 0) == 4);
    const uint8_t path[] = {3, 2, 1, 2, 3, 4, 3, 2, 1};
    for (unsigned i = 0; i < sizeof(path); i++) {
        uint16_t landed = tiles_bounce_step(&b);
        assert(tiles_bounce_ball_height(&b, 0) == path[i]);
        assert(((landed & 1u) != 0) == (path[i] == 1)); /* lands exactly on the floor */
    }

    /* 4. height 1 lands every step; off again: silent */
    tiles_bounce_set_height(&b, 0, 1);
    for (int i = 0; i < 5; i++) {
        assert(tiles_bounce_step(&b) & 1u);
        assert(tiles_bounce_ball_height(&b, 0) == 1);
    }
    tiles_bounce_set_height(&b, 0, 0);
    assert(tiles_bounce_step(&b) == 0u && tiles_bounce_ball_height(&b, 0) == 0);

    /* 5. heights 4 and 3 from the same moment: 3 against 2 over 12 steps */
    tiles_bounce_clear(&b);
    tiles_bounce_set_height(&b, 2, 4);
    tiles_bounce_set_height(&b, 11, 3);
    int hits4 = 0, hits3 = 0;
    for (int i = 0; i < 24; i++) {
        uint16_t landed = tiles_bounce_step(&b);
        hits4 += (landed >> 2) & 1u;
        hits3 += (landed >> 11) & 1u;
    }
    assert(hits4 == 4 && hits3 == 6);

    /* 6. drop_all puts every ball back at its top */
    tiles_bounce_step(&b);
    tiles_bounce_drop_all(&b);
    assert(tiles_bounce_ball_height(&b, 2) == 4 && tiles_bounce_ball_height(&b, 11) == 3);
    assert(tiles_bounce_ball_height(&b, 5) == 0);

    /* 7. out of range lanes and heights */
    tiles_bounce_set_height(&b, 12, 3);
    tiles_bounce_set_height(&b, 4, 9);
    assert(b.height[4] == 4);

    printf("bounce_core: all tests pass\n");
    return 0;
}
