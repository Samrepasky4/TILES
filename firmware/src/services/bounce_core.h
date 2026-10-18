#pragma once

/* Bouncing notes' ball physics, no hardware (services/bounce.c is the
 * glue). Each of 12 lanes holds a ball dropped from a height of 1-4 pads (0
 * = off). It moves one pad per step, falls to the floor, plays its note
 * there, and climbs back to its height: a ball from height h lands every
 * 2(h-1) steps (h = 1: every step), so lanes at different heights make
 * polyrhythms (heights 4 and 3 land 6 and 4 steps apart: 3 against 2).
 * Host-tested in test/test_bounce_core.c. */

#include <stdbool.h>
#include <stdint.h>

#define TILES_BOUNCE_LANES 12u
#define TILES_BOUNCE_MAX_HEIGHT 4u

typedef struct {
    uint8_t height[TILES_BOUNCE_LANES]; /* 0 = off, 1-4 */
    uint8_t phase[TILES_BOUNCE_LANES];  /* steps since the last landing */
} tiles_bounce_t;

void tiles_bounce_clear(tiles_bounce_t *b);

/* Steps between landings for height h (0 for off). */
uint8_t tiles_bounce_period(uint8_t height);

/* Drops lane's ball from `height` (0 = off): it starts at the top, falling,
 * so it lands height-1 steps later. */
void tiles_bounce_set_height(tiles_bounce_t *b, uint8_t lane, uint8_t height);

/* Every ball back to the top of its height, falling (a rewind). */
void tiles_bounce_drop_all(tiles_bounce_t *b);

/* Where lane's ball is: 1 = the floor .. its height; 0 if off. */
uint8_t tiles_bounce_ball_height(const tiles_bounce_t *b, uint8_t lane);

/* One step: every ball moves; returns the lanes that land (bit n = lane n). */
uint16_t tiles_bounce_step(tiles_bounce_t *b);
