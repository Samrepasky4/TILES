#include "bounce_core.h"

#include <string.h>

void tiles_bounce_clear(tiles_bounce_t *b) {
    memset(b, 0, sizeof(*b));
}

uint8_t tiles_bounce_period(uint8_t height) {
    if (height == 0u) {
        return 0u;
    }
    return height == 1u ? 1u : (uint8_t)(2u * (height - 1u));
}

/* Phase 0 is on the floor; the ball climbs to its height at phase h-1 and
 * falls back, landing at phase 2(h-1) = 0. Starting at phase h-1 is the
 * top, about to fall. */
void tiles_bounce_set_height(tiles_bounce_t *b, uint8_t lane, uint8_t height) {
    if (lane >= TILES_BOUNCE_LANES) {
        return;
    }
    if (height > TILES_BOUNCE_MAX_HEIGHT) {
        height = TILES_BOUNCE_MAX_HEIGHT;
    }
    b->height[lane] = height;
    b->phase[lane] = height > 1u ? (uint8_t)(height - 1u) : 0u;
}

void tiles_bounce_drop_all(tiles_bounce_t *b) {
    for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
        tiles_bounce_set_height(b, lane, b->height[lane]);
    }
}

uint8_t tiles_bounce_ball_height(const tiles_bounce_t *b, uint8_t lane) {
    if (lane >= TILES_BOUNCE_LANES || b->height[lane] == 0u) {
        return 0u;
    }
    uint8_t h = b->height[lane];
    uint8_t p = b->phase[lane];
    if (h == 1u) {
        return 1u;
    }
    return p <= h - 1u ? (uint8_t)(1u + p) : (uint8_t)(1u + 2u * (h - 1u) - p);
}

uint16_t tiles_bounce_step(tiles_bounce_t *b) {
    uint16_t landed = 0u;
    for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
        uint8_t period = tiles_bounce_period(b->height[lane]);
        if (period == 0u) {
            continue;
        }
        b->phase[lane] = (uint8_t)((b->phase[lane] + 1u) % period);
        if (b->phase[lane] == 0u) {
            landed |= (uint16_t)(1u << lane);
        }
    }
    return landed;
}
