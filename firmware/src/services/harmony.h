#pragma once

/* Harmony mode: a chord machine for experimenting, no theory needed. The
 * chord maths (colours, key fitting, glide) is services/harmony_core.h;
 * this file is the hardware side. services/op_mode.c owns the mode switch.
 *
 *   columns 1-3   the 12 roots, chromatic: C bottom-left, three per row
 *                 going up (C C# D / D# E F / F# G G# / A A# B). Push one:
 *                 its chord plays (strike velocity) until you lift.
 *   columns 4-6   the 12 chord colours (harmony_core.h): bright, dark and
 *                 ambiguous columns, the bottom row safest, stranger going
 *                 up. Touch one to pick it; it stays picked. Touching one
 *                 while a chord sounds changes that chord at once, holding
 *                 the notes the two share.
 *   "-" / "+"     the chords an octave down / up (two each way).
 *
 * Every chord is voiced with the least movement from the last one (glide),
 * between C3 and C6 (an octave per "-"/"+"), with its root an octave below
 * as the bass. Pressing harder sends channel pressure. MIDI channel 12
 * (shared with chord mode; one mode plays at a time).
 *
 * Key lock: with a scale picked (circle + triangle, the scale picker), notes
 * outside it go dark and silent, and every chord is fitted to the scale
 * (harmony_core.h). Chromatic = free. The layout never moves.
 *
 * Lights: the key note in the colour scheme's root colour (pink by
 * default), its fifth in the fifth colour, the others dim; the sounding
 * root white and the chord's other notes in the colour's own hue; the
 * picked colour bright, the others dim; teal underglow.
 *
 * Layout B (an A/B test against the colours; the Orchid's way): the same
 * roots, then
 *   column 4      voicing, tap to pick: root position, 1st inversion, 2nd
 *                 inversion, open (bottom to top). No glide: the chord sits
 *                 at that inversion with its root in C3-B3.
 *   column 5      quality while held: major, minor, sus4, diminished
 *                 (bottom to top); the last one pressed wins; none = major.
 *   column 6      extensions while held, any together: 6, 7, maj7, 9.
 * Hold the quality and extensions, then push a root; changing them while
 * the chord sounds changes it. Light blue underglow. */

#include <stdbool.h>
#include <stdint.h>

/* The mode's color (menu pad, underglow): teal, not used by any other mode. */
#define TILES_HARMONY_COLOR_R 0.0f
#define TILES_HARMONY_COLOR_G 0.8f
#define TILES_HARMONY_COLOR_B 0.6f

void tiles_harmony_init(void);
/* Entering: a finger already down isn't a new press. */
void tiles_harmony_enter(void);
/* Leaving: ends the sounding chord. */
void tiles_harmony_leave(void);

/* Layout B (above) on or off; ends a sounding chord when it changes. */
void tiles_harmony_set_layout_b(bool on);

/* The menu pad's / underglow's color in layout B: light blue. */
#define TILES_HARMONY_B_COLOR_R 0.3f
#define TILES_HARMONY_B_COLOR_G 0.6f
#define TILES_HARMONY_B_COLOR_B 1.0f

/* Every scan while the mode is shown. */
void tiles_harmony_handle_input(uint32_t now_ms);
void tiles_harmony_render(uint32_t now_ms, float beat_flash, float diamond_led);
