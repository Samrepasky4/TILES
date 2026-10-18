#pragma once

/* Bouncing notes mode: every column is a lane where a ball bounces; it plays
 * the lane's note each time it lands on the bottom pad. The physics is
 * services/bounce_core.h; this file is the hardware side. services/op_mode.c
 * owns the mode switch, the shared buttons and the clock.
 *
 *   pads          the column's ball drops from that pad's height (bottom =
 *                 1 .. top = 4) and keeps bouncing; the same height again
 *                 stops it (off). Higher drops land less often and louder:
 *                 heights 1-4 land every 1, 2, 4, 6 eighths.
 *   circle + "-"/"+"  scroll one column: 12 lanes, 6 shown. All pads flash
 *                 pink at either end.
 *   "-" / "+"     stop / start as in the sequencer ("-" twice drops every
 *                 ball back to its top; "+" with no clock coming in starts
 *                 at once and restarts the clock).
 *   circle        tap tempo.
 *   square        the Feel menu only (no pitch bend toggle here).
 *
 * Lane pitches: the scale (circle + triangle) from the key note up, lane 1 the
 * key note itself, at the octave set elsewhere; chromatic = 12 semitones.
 * Notes on MIDI channel 11 (shared with the games, which never run at the
 * same time). The balls keep bouncing when you switch modes, like the
 * sequencer; "-" stops them.
 *
 * Lights: a lane's drop height dim, its ball bright, a landing flashes white;
 * each lane its own hue, deep blue on lane 1 through violet and magenta to
 * hot pink on lane 12, so scrolling shows where you are; an empty lane shows a faint
 * bottom pad. Violet underglow. */

#include "midi_clock.h"

#include <stdbool.h>
#include <stdint.h>

/* The mode's color (menu pad, underglow): violet, the experimental family. */
#define TILES_BOUNCE_COLOR_R 0.5f
#define TILES_BOUNCE_COLOR_G 0.0f
#define TILES_BOUNCE_COLOR_B 1.0f

void tiles_bounce_mode_init(void);
/* Entering: a finger already down isn't a tap. Leaving: nothing stops. */
void tiles_bounce_mode_enter(void);

/* Every scan, shown or not: steps the balls on the shared clock. */
void tiles_bounce_mode_advance(tiles_midi_clock_state_t clock);

/* Every scan while the mode is shown. */
void tiles_bounce_mode_handle_input(uint32_t now_ms);
void tiles_bounce_mode_render(uint32_t now_ms, float beat_flash, float diamond_led, bool clock_running);

/* Transport, for op_mode.c's "-"/"+". start(true): every ball lands now (beat
 * one) and bounces from there; start(false): carry on. */
bool tiles_bounce_mode_is_running(void);
void tiles_bounce_mode_start(bool from_floor);
void tiles_bounce_mode_pause(void);
void tiles_bounce_mode_rewind(void);
/* Ends any sounding notes (a panic, or a mode that needs the channel). */
void tiles_bounce_mode_end_all_notes(void);

/* Circle + "-"/"+": one column left (-1) or right (+1). */
void tiles_bounce_mode_scroll(int8_t direction);
