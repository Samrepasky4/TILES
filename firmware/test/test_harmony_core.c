/*
 * services/harmony_core.c -- Harmony mode's chords (a quality plus
 * extensions, the Orchid's way), key fitting and fixed voicings. Run with
 * test/run.sh.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_core.h"

#define C_MAJOR 0x0AB5u      /* C D E F G A B */
#define C_PENTATONIC 0x0295u /* C D E G A */
#define ALL TILES_HARMONY_SCALE_ALL

static bool chord_is(uint8_t quality, uint8_t ext, uint8_t root, uint16_t mask, const int8_t *want, uint8_t n) {
    int8_t got[TILES_HARMONY_MAX_TONES];
    uint8_t count = tiles_harmony_built_chord(quality, ext, root, mask, got);
    return count == n && memcmp(got, want, n) == 0;
}

static bool voiced_as(uint8_t root, const int8_t *iv, uint8_t n, uint8_t inversion, bool open,
                      const uint8_t *want) {
    tiles_harmony_voicing_t v;
    tiles_harmony_voice_fixed(root, iv, n, inversion, open, 48, &v);
    return v.count == n && memcmp(v.notes, want, n) == 0;
}

int main(void) {
    /* 1. a quality plus extensions */
    assert(chord_is(0, 0, 0, ALL, (const int8_t[]){0, 4, 7}, 3));                                      /* C */
    assert(chord_is(0, TILES_HARMONY_EXT_7, 7, ALL, (const int8_t[]){0, 4, 7, 10}, 4));                /* G7 */
    assert(chord_is(1, TILES_HARMONY_EXT_7, 9, ALL, (const int8_t[]){0, 3, 7, 10}, 4));                /* Am7 */
    assert(chord_is(3, TILES_HARMONY_EXT_7, 11, ALL, (const int8_t[]){0, 3, 6, 10}, 4));               /* Bm7b5 */
    assert(chord_is(3, TILES_HARMONY_EXT_6, 0, ALL, (const int8_t[]){0, 3, 6, 9}, 4));                 /* Cdim7 */
    assert(chord_is(2, TILES_HARMONY_EXT_7, 7, ALL, (const int8_t[]){0, 5, 7, 10}, 4));                /* G7sus4 */
    assert(chord_is(0, TILES_HARMONY_EXT_MAJ7 | TILES_HARMONY_EXT_9, 0, ALL, (const int8_t[]){0, 4, 7, 11, 14}, 5));
    assert(chord_is(0, 0xF, 0, ALL, (const int8_t[]){0, 4, 7, 9, 10, 11, 14}, 7));                     /* all four */

    /* 2. key lock, C major: thirds stay thirds, fifths stay fifths */
    assert(chord_is(0, 0, 2, C_MAJOR, (const int8_t[]){0, 3, 7}, 3));                  /* D -> Dm */
    assert(chord_is(0, 0, 11, C_MAJOR, (const int8_t[]){0, 3, 6}, 3));                 /* B -> Bdim */
    assert(chord_is(1, 0, 0, C_MAJOR, (const int8_t[]){0, 4, 7}, 3));                  /* Cm -> C */
    assert(chord_is(0, TILES_HARMONY_EXT_7, 2, C_MAJOR, (const int8_t[]){0, 3, 7, 10}, 4)); /* D7 -> Dm7 */
    assert(chord_is(0, TILES_HARMONY_EXT_MAJ7, 5, C_MAJOR, (const int8_t[]){0, 4, 7, 11}, 4)); /* Fmaj7 stays */
    /* every quality and extension on every root of C major stays in C major */
    for (uint8_t q = 0; q < TILES_HARMONY_NUM_QUALITIES; q++) {
        for (uint8_t ext = 0; ext < 16u; ext++) {
            for (uint8_t root = 0; root < 12; root++) {
                if (!((C_MAJOR >> root) & 1u)) {
                    continue;
                }
                int8_t iv[TILES_HARMONY_MAX_TONES];
                uint8_t n = tiles_harmony_built_chord(q, ext, root, C_MAJOR, iv);
                assert(n >= 2 && iv[0] == 0);
                for (uint8_t i = 0; i < n; i++) {
                    assert((C_MAJOR >> ((root + iv[i]) % 12)) & 1u);
                    assert(i == 0 || iv[i] > iv[i - 1]);
                }
            }
        }
    }

    /* 3. pentatonic: no third for D, so the nearest note in the scale */
    assert(chord_is(0, 0, 2, C_PENTATONIC, (const int8_t[]){0, 5, 7}, 3)); /* D -> D G A */

    /* 4. voicings: root position, inversions, open */
    const int8_t triad[] = {0, 4, 7}, maj7[] = {0, 4, 7, 11};
    assert(voiced_as(0, triad, 3, 0, false, (const uint8_t[]){48, 52, 55}));
    assert(voiced_as(0, triad, 3, 1, false, (const uint8_t[]){52, 55, 60}));
    assert(voiced_as(0, triad, 3, 2, false, (const uint8_t[]){55, 60, 64}));
    assert(voiced_as(0, triad, 3, 0, true, (const uint8_t[]){48, 55, 64}));   /* root, fifth, tenth */
    assert(voiced_as(0, maj7, 4, 0, true, (const uint8_t[]){43, 48, 52, 59})); /* drop 2 */
    assert(voiced_as(9, triad, 3, 0, false, (const uint8_t[]){57, 61, 64}));  /* A in C3..B3 */

    printf("harmony_core: all tests pass\n");
    return 0;
}
