/*
 * services/harmony_core.c -- Harmony mode's colours, key fitting and glide
 * voicing. Run with test/run.sh.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_core.h"

#define C_MAJOR 0x0AB5u      /* C D E F G A B */
#define C_PENTATONIC 0x0295u /* C D E G A */

static bool chord_is(uint8_t colour, uint8_t root, uint16_t mask, const int8_t *want, uint8_t n) {
    int8_t got[TILES_HARMONY_MAX_TONES];
    uint8_t count = tiles_harmony_chord(colour, root, mask, got);
    return count == n && memcmp(got, want, n) == 0;
}

static bool has_note(const tiles_harmony_voicing_t *v, uint8_t note) {
    for (uint8_t i = 0; i < v->count; i++) {
        if (v->notes[i] == note) {
            return true;
        }
    }
    return false;
}

int main(void) {
    /* 1. free (chromatic): the colours as they are */
    assert(chord_is(0, 0, TILES_HARMONY_SCALE_ALL, (const int8_t[]){0, 4, 7}, 3));         /* C major */
    assert(chord_is(1, 2, TILES_HARMONY_SCALE_ALL, (const int8_t[]){0, 3, 7}, 3));         /* D minor */
    assert(chord_is(6, 0, TILES_HARMONY_SCALE_ALL, (const int8_t[]){0, 4, 7, 11, 18}, 5)); /* Cmaj7#11 */
    assert(chord_is(8, 0, TILES_HARMONY_SCALE_ALL, (const int8_t[]){0, 5, 10, 15}, 4));    /* fourths */

    /* 2. key lock, C major: thirds stay thirds, fifths stay fifths */
    assert(chord_is(0, 2, C_MAJOR, (const int8_t[]){0, 3, 7}, 3));            /* D bright -> Dm */
    assert(chord_is(0, 11, C_MAJOR, (const int8_t[]){0, 3, 6}, 3));           /* B bright -> Bdim */
    assert(chord_is(1, 0, C_MAJOR, (const int8_t[]){0, 4, 7}, 3));            /* C dark -> C */
    assert(chord_is(9, 0, C_MAJOR, (const int8_t[]){0, 4, 7}, 3));            /* C eerie -> C */
    assert(chord_is(10, 9, C_MAJOR, (const int8_t[]){0, 3, 7, 10}, 4));       /* A noir -> Am7 */
    assert(chord_is(11, 7, C_MAJOR, (const int8_t[]){0, 4, 7, 10, 14}, 5));   /* G grit -> G9 */
    assert(chord_is(6, 5, C_MAJOR, (const int8_t[]){0, 4, 7, 11, 18}, 5));    /* F dream: already in C */
    assert(chord_is(6, 0, C_MAJOR, (const int8_t[]){0, 4, 7, 11, 17}, 5));    /* C dream: #11 -> 11 */

    /* every colour on every root of C major stays in C major, root first */
    for (uint8_t colour = 0; colour < TILES_HARMONY_NUM_COLOURS; colour++) {
        for (uint8_t root = 0; root < 12; root++) {
            if (!((C_MAJOR >> root) & 1u)) {
                continue;
            }
            int8_t iv[TILES_HARMONY_MAX_TONES];
            uint8_t n = tiles_harmony_chord(colour, root, C_MAJOR, iv);
            assert(n >= 2 && iv[0] == 0);
            for (uint8_t i = 0; i < n; i++) {
                assert((C_MAJOR >> ((root + iv[i]) % 12)) & 1u);
                assert(i == 0 || iv[i] > iv[i - 1]);
            }
        }
    }

    /* 3. pentatonic: no third for D, so the nearest note in the scale */
    assert(chord_is(0, 2, C_PENTATONIC, (const int8_t[]){0, 5, 7}, 3)); /* D bright -> D G A */

    /* 4. voicing: inside the window; the first chord sits near the middle */
    int8_t iv[TILES_HARMONY_MAX_TONES];
    tiles_harmony_voicing_t none = {0}, c_chord, f_chord, wide;
    uint8_t n = tiles_harmony_chord(0, 0, TILES_HARMONY_SCALE_ALL, iv);
    tiles_harmony_voice(0, iv, n, &none, 48, 84, &c_chord);
    assert(c_chord.count == 3);
    for (uint8_t i = 0; i < 3; i++) {
        assert(c_chord.notes[i] >= 48 && c_chord.notes[i] <= 84);
        assert(c_chord.notes[i] % 12 == 0 || c_chord.notes[i] % 12 == 4 || c_chord.notes[i] % 12 == 7);
    }
    assert(c_chord.notes[0] >= 58 && c_chord.notes[2] <= 76); /* near the middle (66) */

    /* glide: C -> F keeps the shared C where it was and moves the rest a little */
    uint8_t shared_c = c_chord.notes[0] % 12 == 0 ? c_chord.notes[0] : (c_chord.notes[1] % 12 == 0 ? c_chord.notes[1] : c_chord.notes[2]);
    n = tiles_harmony_chord(0, 5, TILES_HARMONY_SCALE_ALL, iv);
    tiles_harmony_voice(5, iv, n, &c_chord, 48, 84, &f_chord);
    assert(has_note(&f_chord, shared_c));
    for (uint8_t i = 0; i < f_chord.count; i++) {
        assert(f_chord.notes[i] + 4 >= c_chord.notes[0] && f_chord.notes[i] <= c_chord.notes[2] + 4);
    }

    /* a wide colour (two octaves of span) still fits, in order */
    n = tiles_harmony_chord(6, 0, TILES_HARMONY_SCALE_ALL, iv);
    tiles_harmony_voice(0, iv, n, &f_chord, 48, 84, &wide);
    assert(wide.count == 5);
    for (uint8_t i = 1; i < wide.count; i++) {
        assert(wide.notes[i] > wide.notes[i - 1] && wide.notes[i] <= 84 && wide.notes[0] >= 48);
    }

    /* a window too small for the chord gives nothing rather than a bad chord */
    tiles_harmony_voice(0, iv, n, &none, 60, 64, &wide);
    assert(wide.count == 0);

    /* 5. layout B: a quality plus extensions, the Orchid's way */
    int8_t got[TILES_HARMONY_MAX_TONES];
    assert(tiles_harmony_built_chord(0, TILES_HARMONY_EXT_7, 7, TILES_HARMONY_SCALE_ALL, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 4, 7, 10}, 4) == 0);                                 /* G7 */
    assert(tiles_harmony_built_chord(1, TILES_HARMONY_EXT_7, 9, TILES_HARMONY_SCALE_ALL, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 3, 7, 10}, 4) == 0);                                 /* Am7 */
    assert(tiles_harmony_built_chord(3, TILES_HARMONY_EXT_7, 11, TILES_HARMONY_SCALE_ALL, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 3, 6, 10}, 4) == 0);                                 /* Bm7b5 */
    assert(tiles_harmony_built_chord(3, TILES_HARMONY_EXT_6, 0, TILES_HARMONY_SCALE_ALL, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 3, 6, 9}, 4) == 0);                                  /* Cdim7 */
    assert(tiles_harmony_built_chord(2, TILES_HARMONY_EXT_7, 7, TILES_HARMONY_SCALE_ALL, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 5, 7, 10}, 4) == 0);                                 /* G7sus4 */
    assert(tiles_harmony_built_chord(0, TILES_HARMONY_EXT_MAJ7 | TILES_HARMONY_EXT_9, 0, TILES_HARMONY_SCALE_ALL, got) == 5 &&
           memcmp(got, (const int8_t[]){0, 4, 7, 11, 14}, 5) == 0);                             /* Cmaj9 */
    assert(tiles_harmony_built_chord(0, 0xF, 0, TILES_HARMONY_SCALE_ALL, got) == 7 &&
           memcmp(got, (const int8_t[]){0, 4, 7, 9, 10, 11, 14}, 7) == 0);                      /* everything */
    assert(tiles_harmony_built_chord(0, TILES_HARMONY_EXT_7, 2, C_MAJOR, got) == 4 &&
           memcmp(got, (const int8_t[]){0, 3, 7, 10}, 4) == 0);                                 /* D7 in C -> Dm7 */

    /* fixed voicings: root position, inversions, open */
    tiles_harmony_voicing_t v;
    int8_t triad[] = {0, 4, 7}, maj7[] = {0, 4, 7, 11};
    tiles_harmony_voice_fixed(0, triad, 3, 0, false, 48, &v);
    assert(v.count == 3 && memcmp(v.notes, (const uint8_t[]){48, 52, 55}, 3) == 0);
    tiles_harmony_voice_fixed(0, triad, 3, 1, false, 48, &v);
    assert(v.count == 3 && memcmp(v.notes, (const uint8_t[]){52, 55, 60}, 3) == 0);
    tiles_harmony_voice_fixed(0, triad, 3, 2, false, 48, &v);
    assert(v.count == 3 && memcmp(v.notes, (const uint8_t[]){55, 60, 64}, 3) == 0);
    tiles_harmony_voice_fixed(0, triad, 3, 0, true, 48, &v);
    assert(v.count == 3 && memcmp(v.notes, (const uint8_t[]){48, 55, 64}, 3) == 0);
    tiles_harmony_voice_fixed(0, maj7, 4, 0, true, 48, &v);
    assert(v.count == 4 && memcmp(v.notes, (const uint8_t[]){43, 48, 52, 59}, 4) == 0); /* drop 2 */
    tiles_harmony_voice_fixed(9, triad, 3, 0, false, 48, &v);
    assert(v.notes[0] == 57); /* A in C3..B3 */

    printf("harmony_core: all tests pass\n");
    return 0;
}
