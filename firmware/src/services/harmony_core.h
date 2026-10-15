#pragma once

/* Harmony mode's chord maths, no hardware (services/harmony.c is the
 * glue): the twelve chord colours, fitting a chord to a scale (key lock),
 * and voicing it with the least movement from the last chord ("glide").
 * Host-tested in test/test_harmony_core.c.
 *
 * Colours, by pad: row 4 (bottom) first, then up; columns bright, dark,
 * ambiguous. The bottom row is the safest, each row up stranger:
 *    0 bright  major      1 dark   minor       2 open   sus2
 *    3 glow    maj9       4 deep   min9        5 float  7sus4
 *    6 dream   maj7#11    7 haze   min11       8 space  quartal (4ths)
 *    9 eerie   aug       10 noir   m(maj7)    11 grit   7#9 */

#include <stdbool.h>
#include <stdint.h>

#define TILES_HARMONY_NUM_COLOURS 12u
#define TILES_HARMONY_MAX_TONES 5u   /* root included */
#define TILES_HARMONY_SCALE_ALL 0x0FFFu /* every pitch class: no fitting */

/* The chord tones of `colour` on a root of pitch class `root_pc`, as
 * semitones above the root (the root itself first, 0), ascending, in the
 * colour's own spread. `scale_mask` bit n set = pitch class n is in the
 * scale; with TILES_HARMONY_SCALE_ALL nothing is fitted. Otherwise a tone
 * outside the scale moves a semitone to one inside, keeping its role (a
 * third stays a third, so major turns minor), down first; failing that, any
 * neighbour in the scale, then two semitones; a tone with nowhere to go is
 * dropped, as is one landing on another's pitch class. Returns the count. */
uint8_t tiles_harmony_chord(uint8_t colour, uint8_t root_pc, uint16_t scale_mask, int8_t *intervals_out);

typedef struct {
    uint8_t count;
    uint8_t notes[TILES_HARMONY_MAX_TONES]; /* MIDI, ascending */
} tiles_harmony_voicing_t;

/* Places the chord (root_pc + intervals, from tiles_harmony_chord()) within
 * low..high: of its inversions (bottom notes moved up an octave) at every
 * octave that fits, the one moving least from `prev` (each note to the
 * nearest old one and back); with no previous chord (count 0) or on a tie,
 * the one nearest the window's middle, so the register doesn't drift. */
void tiles_harmony_voice(uint8_t root_pc, const int8_t *intervals, uint8_t count,
                         const tiles_harmony_voicing_t *prev, uint8_t low, uint8_t high,
                         tiles_harmony_voicing_t *out);
