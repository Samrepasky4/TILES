#include "harmony_core.h"

#include <stdlib.h>
#include <string.h>

/* A chord tone's role, for key fitting: the semitone range it may move
 * within and still be that role. */
typedef enum { ROLE_2ND, ROLE_3RD, ROLE_4TH, ROLE_5TH, ROLE_6TH, ROLE_7TH, ROLE_9TH, ROLE_11TH } role_t;

static const int8_t ROLE_MIN[] = {1, 3, 5, 6, 8, 9, 13, 17};
static const int8_t ROLE_MAX[] = {3, 4, 6, 8, 9, 11, 15, 18};

typedef struct {
    int8_t interval;
    role_t role;
} tone_t;

typedef struct {
    uint8_t count; /* tones above the root */
    tone_t tones[TILES_HARMONY_MAX_TONES - 1u];
} colour_t;

/* Index = colour (harmony_core.h). Intervals above the root, ascending. */
static const colour_t COLOURS[TILES_HARMONY_NUM_COLOURS] = {
    {2, {{4, ROLE_3RD}, {7, ROLE_5TH}}},                                       /* bright  major */
    {2, {{3, ROLE_3RD}, {7, ROLE_5TH}}},                                       /* dark    minor */
    {2, {{2, ROLE_2ND}, {7, ROLE_5TH}}},                                       /* open    sus2 */
    {4, {{4, ROLE_3RD}, {7, ROLE_5TH}, {11, ROLE_7TH}, {14, ROLE_9TH}}},       /* glow    maj9 */
    {4, {{3, ROLE_3RD}, {7, ROLE_5TH}, {10, ROLE_7TH}, {14, ROLE_9TH}}},       /* deep    min9 */
    {3, {{5, ROLE_4TH}, {7, ROLE_5TH}, {10, ROLE_7TH}}},                       /* float   7sus4 */
    {4, {{4, ROLE_3RD}, {7, ROLE_5TH}, {11, ROLE_7TH}, {18, ROLE_11TH}}},      /* dream   maj7#11 */
    {4, {{3, ROLE_3RD}, {10, ROLE_7TH}, {14, ROLE_9TH}, {17, ROLE_11TH}}},     /* haze    min11, no 5th */
    {3, {{5, ROLE_4TH}, {10, ROLE_7TH}, {15, ROLE_9TH}}},                      /* space   stacked 4ths */
    {2, {{4, ROLE_3RD}, {8, ROLE_5TH}}},                                       /* eerie   aug */
    {3, {{3, ROLE_3RD}, {7, ROLE_5TH}, {11, ROLE_7TH}}},                       /* noir    m(maj7) */
    {4, {{4, ROLE_3RD}, {7, ROLE_5TH}, {10, ROLE_7TH}, {15, ROLE_9TH}}},       /* grit    7#9 */
};

static bool in_scale(uint16_t mask, uint8_t root_pc, int interval) {
    int pc = ((int)root_pc + interval) % 12;
    if (pc < 0) {
        pc += 12;
    }
    return (mask >> pc) & 1u;
}

/* Where a tone outside the scale goes; false if nowhere. */
static bool fit_tone(uint16_t mask, uint8_t root_pc, const tone_t *t, int8_t *out) {
    const int8_t steps[] = {-1, 1};
    for (uint8_t pass = 0; pass < 2u; pass++) { /* pass 0: keep the role; pass 1: any */
        for (uint8_t k = 0; k < 2u; k++) {
            int cand = t->interval + steps[k];
            bool role_ok = cand >= ROLE_MIN[t->role] && cand <= ROLE_MAX[t->role];
            if (cand > 0 && (pass == 1u || role_ok) && in_scale(mask, root_pc, cand)) {
                *out = (int8_t)cand;
                return true;
            }
        }
    }
    for (uint8_t k = 0; k < 2u; k++) {
        int cand = t->interval + 2 * steps[k];
        if (cand > 0 && in_scale(mask, root_pc, cand)) {
            *out = (int8_t)cand;
            return true;
        }
    }
    return false;
}

/* The root, then each tone fitted to the scale; duplicates of a pitch class
 * dropped; ascending. */
static uint8_t assemble(const tone_t *tones, uint8_t count, uint8_t root_pc, uint16_t scale_mask,
                        int8_t *intervals_out) {
    root_pc = (uint8_t)(root_pc % 12u);
    uint8_t n = 0u;
    intervals_out[n++] = 0;
    for (uint8_t i = 0; i < count && n < TILES_HARMONY_MAX_TONES; i++) {
        int8_t iv = tones[i].interval;
        if ((scale_mask & TILES_HARMONY_SCALE_ALL) != TILES_HARMONY_SCALE_ALL && !in_scale(scale_mask, root_pc, iv) &&
            !fit_tone(scale_mask, root_pc, &tones[i], &iv)) {
            continue; /* nowhere in the scale to go */
        }
        bool clash = false;
        for (uint8_t j = 0; j < n; j++) {
            if ((iv - intervals_out[j]) % 12 == 0) {
                clash = true;
            }
        }
        if (!clash) {
            intervals_out[n++] = iv;
        }
    }
    /* Fitting can reorder tones (a 3rd and a 4th meeting); keep ascending. */
    for (uint8_t i = 1; i < n; i++) {
        for (uint8_t j = i; j > 0 && intervals_out[j] < intervals_out[j - 1u]; j--) {
            int8_t tmp = intervals_out[j];
            intervals_out[j] = intervals_out[j - 1u];
            intervals_out[j - 1u] = tmp;
        }
    }
    return n;
}

uint8_t tiles_harmony_chord(uint8_t colour, uint8_t root_pc, uint16_t scale_mask, int8_t *intervals_out) {
    if (colour >= TILES_HARMONY_NUM_COLOURS) {
        colour = 0u;
    }
    return assemble(COLOURS[colour].tones, COLOURS[colour].count, root_pc, scale_mask, intervals_out);
}

uint8_t tiles_harmony_built_chord(uint8_t quality, uint8_t ext_mask, uint8_t root_pc, uint16_t scale_mask,
                                  int8_t *intervals_out) {
    static const tone_t TRIADS[TILES_HARMONY_NUM_QUALITIES][2] = {
        {{4, ROLE_3RD}, {7, ROLE_5TH}}, /* major */
        {{3, ROLE_3RD}, {7, ROLE_5TH}}, /* minor */
        {{5, ROLE_4TH}, {7, ROLE_5TH}}, /* sus4 */
        {{3, ROLE_3RD}, {6, ROLE_5TH}}, /* diminished (with 6: dim7) */
    };
    tone_t tones[TILES_HARMONY_MAX_TONES];
    uint8_t n = 0u;
    if (quality >= TILES_HARMONY_NUM_QUALITIES) {
        quality = 0u;
    }
    tones[n++] = TRIADS[quality][0];
    tones[n++] = TRIADS[quality][1];
    if (ext_mask & TILES_HARMONY_EXT_6) {
        tones[n++] = (tone_t){9, ROLE_6TH};
    }
    if (ext_mask & TILES_HARMONY_EXT_7) {
        tones[n++] = (tone_t){10, ROLE_7TH};
    }
    if (ext_mask & TILES_HARMONY_EXT_MAJ7) {
        tones[n++] = (tone_t){11, ROLE_7TH};
    }
    if (ext_mask & TILES_HARMONY_EXT_9) {
        tones[n++] = (tone_t){14, ROLE_9TH};
    }
    return assemble(tones, n, root_pc, scale_mask, intervals_out);
}

/* Each note to the nearest note of the other chord, both ways. */
static int movement(const uint8_t *a, uint8_t na, const uint8_t *b, uint8_t nb) {
    int total = 0;
    for (uint8_t pass = 0; pass < 2u; pass++) {
        const uint8_t *x = pass ? b : a, *y = pass ? a : b;
        uint8_t nx = pass ? nb : na, ny = pass ? na : nb;
        for (uint8_t i = 0; i < nx; i++) {
            int best = 1000;
            for (uint8_t j = 0; j < ny; j++) {
                int d = abs((int)x[i] - (int)y[j]);
                if (d < best) {
                    best = d;
                }
            }
            total += best;
        }
    }
    return total;
}

void tiles_harmony_voice(uint8_t root_pc, const int8_t *intervals, uint8_t count,
                         const tiles_harmony_voicing_t *prev, uint8_t low, uint8_t high,
                         tiles_harmony_voicing_t *out) {
    out->count = 0u;
    if (count == 0u || count > TILES_HARMONY_MAX_TONES || high <= low) {
        return;
    }
    root_pc = (uint8_t)(root_pc % 12u);
    int centre2 = (int)low + (int)high; /* twice the window's middle */
    int best_cost = -1, best_centre_gap = 0;
    for (uint8_t inv = 0; inv < count; inv++) {
        /* Inversion inv: the lowest inv tones an octave up. */
        int shape[TILES_HARMONY_MAX_TONES];
        for (uint8_t i = 0; i < count; i++) {
            shape[i] = intervals[i] + (i < inv ? 12 : 0);
        }
        for (uint8_t i = 1; i < count; i++) {
            for (uint8_t j = i; j > 0 && shape[j] < shape[j - 1u]; j--) {
                int tmp = shape[j];
                shape[j] = shape[j - 1u];
                shape[j - 1u] = tmp;
            }
        }
        for (int octave = 0; octave <= 10; octave++) {
            int base = 12 * octave + (int)root_pc;
            if (base + shape[0] < (int)low || base + shape[count - 1u] > (int)high) {
                continue;
            }
            uint8_t notes[TILES_HARMONY_MAX_TONES];
            int sum = 0;
            for (uint8_t i = 0; i < count; i++) {
                notes[i] = (uint8_t)(base + shape[i]);
                sum += notes[i];
            }
            int centre_gap = abs(2 * sum - centre2 * (int)count); /* |mean - middle| * 2 * count */
            int cost = (prev != NULL && prev->count > 0u) ? movement(notes, count, prev->notes, prev->count) : 0;
            if (best_cost < 0 || cost < best_cost || (cost == best_cost && centre_gap < best_centre_gap)) {
                best_cost = cost;
                best_centre_gap = centre_gap;
                out->count = count;
                memcpy(out->notes, notes, count);
            }
        }
    }
}

void tiles_harmony_voice_fixed(uint8_t root_pc, const int8_t *intervals, uint8_t count, uint8_t inversion,
                               bool open, uint8_t low, tiles_harmony_voicing_t *out) {
    out->count = 0u;
    if (count == 0u || count > TILES_HARMONY_MAX_TONES) {
        return;
    }
    root_pc = (uint8_t)(root_pc % 12u);
    int base = (int)low + (((int)root_pc - (int)low) % 12 + 12) % 12; /* the root's note in low..low+11 */
    int notes[TILES_HARMONY_MAX_TONES];
    for (uint8_t i = 0; i < count; i++) {
        notes[i] = base + intervals[i] + (i < inversion && i < count - 1u ? 12 : 0);
    }
    for (uint8_t pass = 0; pass < 2u; pass++) {
        for (uint8_t i = 1; i < count; i++) {
            for (uint8_t j = i; j > 0 && notes[j] < notes[j - 1u]; j--) {
                int tmp = notes[j];
                notes[j] = notes[j - 1u];
                notes[j - 1u] = tmp;
            }
        }
        if (pass == 0u && open && count == 3u) {
            notes[1] += 12; /* a triad opens to root, fifth, tenth */
        } else if (pass == 0u && open && count > 3u) {
            notes[count - 2u] -= 12; /* drop 2 */
        } else {
            break;
        }
    }
    for (uint8_t i = 0; i < count; i++) {
        if (notes[i] < 0 || notes[i] > 127) {
            return;
        }
        out->notes[i] = (uint8_t)notes[i];
    }
    out->count = count;
}
