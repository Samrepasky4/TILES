#pragma once

/* Harmony mode: a chord machine laid out the Orchid's way. The chord maths
 * (qualities, extensions, key fitting, voicing) is services/harmony_core.h;
 * this file is the hardware side. services/op_mode.c owns the mode switch.
 *
 *   columns 1-3   the 12 roots, chromatic: C bottom-left, three per row
 *                 going up (C C# D / D# E F / F# G G# / A A# B). Push one:
 *                 its chord plays (strike velocity) until you lift. They
 *                 feel like melodic pads: a tick on touch, a kick on the
 *                 strike, a buzz that follows pressure while held.
 *   column 4      voicing, tap to pick (bottom to top): root position, 1st
 *                 inversion, 2nd inversion, open. The chord sits at that
 *                 inversion with its root in C3-B3.
 *   column 5      quality while held (bottom to top): major, minor, sus4,
 *                 diminished; the last one pressed wins; none = major.
 *   column 6      extensions while held, any together (bottom to top): 6,
 *                 7, maj7, 9. Maj + 7 = dominant 7, Dim + 6 = dim7,
 *                 Dim + 7 = m7b5, Maj7 + 9 = maj9.
 *   "-" / "+"     the chords an octave down / up (two each way).
 *
 * Hold the quality and extensions, then push a root; changing them (or the
 * voicing) while the chord sounds changes it, holding the notes the two
 * share. The root also sounds an octave below as the bass. Pressing harder
 * sends channel pressure. MIDI channel 12 (shared with chord mode; one mode
 * plays at a time).
 *
 * Key lock: with a scale picked (circle + triangle, the scale picker), roots
 * outside it go dark and silent, and every chord is fitted to the scale
 * (harmony_core.h). Chromatic = free. The layout never moves.
 *
 * Lights: the key note in the colour scheme's root colour (pink by
 * default), its fifth in the fifth colour, the others dim; the sounding root
 * white and the chord's other notes amber; the picked voicing, the quality
 * in use and the held extensions bright (major half-bright when no quality
 * is held); teal underglow. */

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

/* Every scan while the mode is shown. */
void tiles_harmony_handle_input(uint32_t now_ms);
void tiles_harmony_render(uint32_t now_ms, float beat_flash, float diamond_led);
