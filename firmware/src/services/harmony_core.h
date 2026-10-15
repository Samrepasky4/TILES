#pragma once

/* Harmony mode's chord maths, no hardware (services/harmony.c is the
 * glue): chords built the Orchid's way from a quality plus extensions,
 * fitted to a scale (key lock), and voiced at a fixed inversion.
 * Host-tested in test/test_harmony_core.c. */

#include <stdbool.h>
#include <stdint.h>

#define TILES_HARMONY_MAX_TONES 7u      /* root included: a triad + 4 extensions */
#define TILES_HARMONY_SCALE_ALL 0x0FFFu /* every pitch class: no fitting */

/* Quality 0 major, 1 minor, 2 sus4, 3 diminished; extension bits below, any
 * together. */
#define TILES_HARMONY_NUM_QUALITIES 4u
#define TILES_HARMONY_EXT_6 0x1u
#define TILES_HARMONY_EXT_7 0x2u /* minor seventh: 7, m7, m7b5, 7sus4 */
#define TILES_HARMONY_EXT_MAJ7 0x4u
#define TILES_HARMONY_EXT_9 0x8u

/* The chord tones on a root of pitch class `root_pc`, as semitones above the
 * root (the root itself first, 0), ascending. `scale_mask` bit n set = pitch
 * class n is in the scale; with TILES_HARMONY_SCALE_ALL nothing is fitted.
 * Otherwise a tone outside the scale moves a semitone to one inside, keeping
 * its role (a third stays a third, so major turns minor), down first; failing
 * that, any neighbour in the scale, then two semitones; a tone with nowhere
 * to go is dropped, as is one landing on another's pitch class. Returns the
 * count. */
uint8_t tiles_harmony_built_chord(uint8_t quality, uint8_t ext_mask, uint8_t root_pc, uint16_t scale_mask,
                                  int8_t *intervals_out);

typedef struct {
    uint8_t count;
    uint8_t notes[TILES_HARMONY_MAX_TONES]; /* MIDI, ascending */
} tiles_harmony_voicing_t;

/* The chord (root_pc + intervals) voiced with the root in low..low+11, the
 * lowest `inversion` tones (0-2) an octave up; `open` spreads it: a triad's
 * middle note up an octave (root, fifth, tenth), a bigger chord's
 * second-highest note down an octave (drop 2). */
void tiles_harmony_voice_fixed(uint8_t root_pc, const int8_t *intervals, uint8_t count, uint8_t inversion,
                               bool open, uint8_t low, tiles_harmony_voicing_t *out);
