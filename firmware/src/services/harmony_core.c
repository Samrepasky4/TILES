#include "harmony_core.h"

#include <string.h>

/* A chord tone's role, for key fitting: the semitone range it may move
 * within and still be that role. */
typedef enum { ROLE_3RD, ROLE_4TH, ROLE_5TH, ROLE_6TH, ROLE_7TH, ROLE_9TH } role_t;

static const int8_t ROLE_MIN[] = {3, 5, 6, 8, 9, 13};
static const int8_t ROLE_MAX[] = {4, 6, 8, 9, 11, 15};

typedef struct {
    int8_t interval;
    role_t role;
} tone_t;

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
