#include "harmony.h"

#include "board_layout.h"
#include "board_pins.h"
#include "buttons.h"
#include "expression.h"
#include "hall.h"
#include "harmony_core.h"
#include "haptics.h"
#include "lighting.h"
#include "midi_channels.h"
#include "midi_out.h"
#include "note_map.h"
#include "touch.h"

#include <stdbool.h>
#include <string.h>

#define HARMONY_CHANNEL TILES_MIDI_CH_CHORD
/* Chord window and bass, before the octave shift: chord tones C3-C6, so
 * thirds stay clear of the muddy low register; the bass is the root in the
 * octave C2-B2. */
#define HARMONY_LOW_NOTE 48u
#define HARMONY_HIGH_NOTE 84u
#define HARMONY_BASS_LOW_NOTE 36u
#define HARMONY_MAX_OCTAVE 2
#define HARMONY_MAX_NOTES (TILES_HARMONY_MAX_TONES + 1u) /* + bass */

/* Light levels (fractions of full, before the power ceiling): first guesses. */
#define HARMONY_COLOUR_DIM 0.3f
#define HARMONY_CHORD_TONE_LEVEL 0.6f
#define HARMONY_OCTAVE_LED_STEP 0.5f

/* The colour columns' hues: bright = amber, dark = blue, ambiguous = purple. */
static const float COLUMN_RGB[3][3] = {
    {1.0f, 0.45f, 0.0f},
    {0.0f, 0.3f, 1.0f},
    {0.6f, 0.0f, 1.0f},
};

static uint8_t s_colour;   /* picked colour, 0-11 */
static int8_t s_octave;    /* -2..+2 */
static bool s_prev_touched[TILES_NUM_PADS];
static uint32_t s_touch_start_ms[TILES_NUM_PADS];
static float s_peak_depth[TILES_NUM_PADS];
static bool s_awaiting_strike[TILES_NUM_PADS];
static bool s_prev_minus, s_prev_plus;

/* The sounding chord (s_sounding_pad 0 = none) and the last one voiced,
 * which the next glides from even after a release. */
static uint8_t s_sounding_pad;
static uint8_t s_sounding_root_pc;
static uint8_t s_sounding_velocity;
static uint8_t s_notes[HARMONY_MAX_NOTES];
static uint8_t s_note_count;
static tiles_harmony_voicing_t s_last_voicing;
static uint8_t s_last_pressure;

/* Root pads: columns 1-3; colour pads: columns 4-6. Row 4 is the bottom. */
static bool pad_row_col(uint8_t pad, uint8_t *row, uint8_t *col) {
    for (uint8_t r = 1u; r <= TILES_GRID_MAX_ROW; r++) {
        for (uint8_t c = TILES_GRID_MIN_COL; c <= TILES_GRID_MAX_COL; c++) {
            if (board_pad_for_row_col(r, c) == pad) {
                *row = r;
                *col = c;
                return true;
            }
        }
    }
    return false;
}

static uint8_t root_pc_for(uint8_t row, uint8_t col) {
    return (uint8_t)((TILES_GRID_MAX_ROW - row) * 3u + (col - 1u));
}

static uint8_t colour_for(uint8_t row, uint8_t col) {
    return (uint8_t)((TILES_GRID_MAX_ROW - row) * 3u + (col - 4u));
}

/* The current scale's pitch classes (key included); all twelve when
 * chromatic. A pitch class is in the scale if quantizing leaves it be. */
static uint16_t scale_mask(void) {
    if (tiles_note_map_get_scale() == TILES_SCALE_CHROMATIC) {
        return TILES_HARMONY_SCALE_ALL;
    }
    uint16_t mask = 0u;
    for (uint8_t pc = 0; pc < 12u; pc++) {
        if (tiles_note_map_quantize_to_scale((uint8_t)(60u + pc)) == 60u + pc) {
            mask |= (uint16_t)(1u << pc);
        }
    }
    return mask != 0u ? mask : TILES_HARMONY_SCALE_ALL;
}

static void notes_off(void) {
    for (uint8_t i = 0; i < s_note_count; i++) {
        tiles_midi_note_off(HARMONY_CHANNEL, s_notes[i], 0u);
    }
    s_note_count = 0u;
}

/* Builds the chord for root_pc in the picked colour: voiced (glide), plus the
 * bass. */
static uint8_t build_chord(uint8_t root_pc, uint8_t *out) {
    int8_t intervals[TILES_HARMONY_MAX_TONES];
    uint8_t n = tiles_harmony_chord(s_colour, root_pc, scale_mask(), intervals);
    int shift = 12 * s_octave;
    tiles_harmony_voicing_t v;
    tiles_harmony_voice(root_pc, intervals, n, &s_last_voicing, (uint8_t)((int)HARMONY_LOW_NOTE + shift),
                        (uint8_t)((int)HARMONY_HIGH_NOTE + shift), &v);
    if (v.count == 0u) {
        return 0u;
    }
    s_last_voicing = v;
    uint8_t count = 0u;
    out[count++] = (uint8_t)((int)HARMONY_BASS_LOW_NOTE + shift + root_pc);
    for (uint8_t i = 0; i < v.count; i++) {
        out[count++] = v.notes[i];
    }
    return count;
}

static void chord_strike(uint8_t pad, uint8_t root_pc, uint8_t velocity) {
    notes_off();
    s_note_count = build_chord(root_pc, s_notes);
    for (uint8_t i = 0; i < s_note_count; i++) {
        tiles_midi_note_on(HARMONY_CHANNEL, s_notes[i], velocity);
    }
    s_sounding_pad = pad;
    s_sounding_root_pc = root_pc;
    s_sounding_velocity = velocity;
    s_last_pressure = 0u;
    tiles_haptics_trigger_kick(pad, velocity);
}

/* A new colour under a sounding chord: notes the two chords share keep
 * sounding, the rest change. */
static void chord_recolour(void) {
    uint8_t next[HARMONY_MAX_NOTES];
    uint8_t n = build_chord(s_sounding_root_pc, next);
    for (uint8_t i = 0; i < s_note_count; i++) {
        if (memchr(next, s_notes[i], n) == NULL) {
            tiles_midi_note_off(HARMONY_CHANNEL, s_notes[i], 0u);
        }
    }
    for (uint8_t i = 0; i < n; i++) {
        if (memchr(s_notes, next[i], s_note_count) == NULL) {
            tiles_midi_note_on(HARMONY_CHANNEL, next[i], s_sounding_velocity);
        }
    }
    memcpy(s_notes, next, n);
    s_note_count = n;
}

static void chord_release(void) {
    notes_off();
    if (s_last_pressure != 0u) {
        tiles_midi_send_channel_pressure(HARMONY_CHANNEL, 0u);
        s_last_pressure = 0u;
    }
    if (s_sounding_pad != 0u) {
        tiles_haptics_stop(s_sounding_pad);
    }
    s_sounding_pad = 0u;
}

void tiles_harmony_init(void) {
    s_colour = 0u;
    s_octave = 0;
    s_sounding_pad = 0u;
    s_note_count = 0u;
    s_last_voicing.count = 0u;
    s_last_pressure = 0u;
    tiles_harmony_enter();
}

void tiles_harmony_enter(void) {
    for (uint8_t pad = 1u; pad <= TILES_NUM_PADS; pad++) {
        s_prev_touched[pad - 1u] = tiles_touch_is_touched(pad);
        s_awaiting_strike[pad - 1u] = false;
    }
    s_prev_minus = tiles_button_is_pressed(TILES_MINUS_BUTTON_ID);
    s_prev_plus = tiles_button_is_pressed(TILES_PLUS_BUTTON_ID);
}

void tiles_harmony_leave(void) {
    chord_release();
}

void tiles_harmony_handle_input(uint32_t now_ms) {
    uint16_t mask = scale_mask();
    for (uint8_t pad = 1u; pad <= TILES_NUM_PADS; pad++) {
        uint8_t i = (uint8_t)(pad - 1u), row, col;
        bool touched = tiles_touch_is_touched(pad);
        bool was = s_prev_touched[i];
        s_prev_touched[i] = touched;
        if (!pad_row_col(pad, &row, &col)) {
            continue;
        }
        if (col >= 4u) {
            /* Colour: picked on touch; a sounding chord changes with it. */
            if (touched && !was) {
                s_colour = colour_for(row, col);
                tiles_haptics_trigger_touch_pulse(pad);
                if (s_sounding_pad != 0u) {
                    chord_recolour();
                }
            }
            continue;
        }
        uint8_t root_pc = root_pc_for(row, col);
        if (!((mask >> root_pc) & 1u)) {
            s_awaiting_strike[i] = false; /* outside the key: silent */
            continue;
        }
        /* Root: a strike (depth past the threshold) plays, as in chord mode. */
        if (touched && !was) {
            s_touch_start_ms[i] = now_ms;
            s_peak_depth[i] = (float)tiles_hall_get_depth(pad);
            s_awaiting_strike[i] = true;
        }
        if (touched && s_awaiting_strike[i]) {
            float depth = (float)tiles_hall_get_depth(pad);
            if (depth > s_peak_depth[i]) {
                s_peak_depth[i] = depth;
            }
            if (s_peak_depth[i] >= TILES_EXPRESSION_MIN_STRIKE_DEPTH_DELTA) {
                uint8_t velocity = tiles_expression_velocity_from_strike(now_ms - s_touch_start_ms[i], s_peak_depth[i]);
                chord_strike(pad, root_pc, velocity);
                s_awaiting_strike[i] = false;
            }
        }
        if (!touched && was) {
            s_awaiting_strike[i] = false;
            if (s_sounding_pad == pad) {
                chord_release();
            }
        }
    }

    /* Pressure on the sounding root: channel pressure, scaled like aftertouch. */
    if (s_sounding_pad != 0u) {
        uint32_t full = tiles_expression_get_aftertouch_sensitivity();
        uint32_t scaled = full != 0u ? (uint32_t)tiles_hall_get_depth(s_sounding_pad) * 127u / full : 0u;
        uint8_t pressure = (uint8_t)(scaled > 127u ? 127u : scaled);
        if (pressure != s_last_pressure) {
            s_last_pressure = pressure;
            tiles_midi_send_channel_pressure(HARMONY_CHANNEL, pressure);
        }
    }

    /* "-" / "+": the chords an octave down / up. */
    bool minus = tiles_button_is_pressed(TILES_MINUS_BUTTON_ID);
    bool plus = tiles_button_is_pressed(TILES_PLUS_BUTTON_ID);
    if (minus && !s_prev_minus && s_octave > -HARMONY_MAX_OCTAVE) {
        s_octave--;
        s_last_voicing.count = 0u; /* a new register: no glide from the old one */
    }
    if (plus && !s_prev_plus && s_octave < HARMONY_MAX_OCTAVE) {
        s_octave++;
        s_last_voicing.count = 0u;
    }
    s_prev_minus = minus;
    s_prev_plus = plus;
}

static void scheme_rgb(tiles_scheme_role_t role, float level, float *r, float *g, float *b) {
    uint32_t c = tiles_lighting_get_scheme_color(role);
    if (c == TILES_LIGHTING_COLOR_NONE) {
        c = tiles_lighting_get_scheme_color(TILES_SCHEME_NOTE);
    }
    if (c == TILES_LIGHTING_COLOR_NONE) {
        c = 0x363636u;
    }
    *r = (float)((c >> 16) & 0xFFu) / 255.0f * level;
    *g = (float)((c >> 8) & 0xFFu) / 255.0f * level;
    *b = (float)(c & 0xFFu) / 255.0f * level;
}

void tiles_harmony_render(uint32_t now_ms, float beat_flash, float diamond_led) {
    (void)now_ms;
    uint16_t mask = scale_mask();
    uint8_t key_pc = (uint8_t)tiles_note_map_get_key_offset();
    /* The sounding chord's pitch classes, to light its notes on the roots. */
    uint16_t chord_pcs = 0u;
    for (uint8_t i = 0; i < s_note_count; i++) {
        chord_pcs |= (uint16_t)(1u << (s_notes[i] % 12u));
    }
    const float *hue = COLUMN_RGB[s_colour % 3u];

    for (uint8_t row = 1u; row <= TILES_GRID_MAX_ROW; row++) {
        for (uint8_t col = TILES_GRID_MIN_COL; col <= TILES_GRID_MAX_COL; col++) {
            uint8_t pad = board_pad_for_row_col(row, col);
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (col >= 4u) {
                uint8_t colour = colour_for(row, col);
                float level = colour == s_colour ? 1.0f : HARMONY_COLOUR_DIM;
                r = COLUMN_RGB[col - 4u][0] * level;
                g = COLUMN_RGB[col - 4u][1] * level;
                b = COLUMN_RGB[col - 4u][2] * level;
            } else {
                uint8_t pc = root_pc_for(row, col);
                if (!((mask >> pc) & 1u)) {
                    /* outside the key: dark */
                } else if (s_sounding_pad == pad) {
                    r = g = b = 1.0f;
                } else if (s_sounding_pad != 0u && ((chord_pcs >> pc) & 1u)) {
                    r = hue[0] * HARMONY_CHORD_TONE_LEVEL;
                    g = hue[1] * HARMONY_CHORD_TONE_LEVEL;
                    b = hue[2] * HARMONY_CHORD_TONE_LEVEL;
                } else if (pc == key_pc) {
                    scheme_rgb(TILES_SCHEME_ROOT, 1.0f, &r, &g, &b);
                } else if (pc == (key_pc + 7u) % 12u) {
                    scheme_rgb(TILES_SCHEME_FIFTH, 1.0f, &r, &g, &b);
                } else {
                    scheme_rgb(TILES_SCHEME_NOTE, 1.0f, &r, &g, &b);
                }
            }
            tiles_lighting_set_standby_pad_rgb(pad, r, g, b);
        }
    }

    for (uint8_t col = TILES_GRID_MIN_COL; col <= TILES_GRID_MAX_COL; col++) {
        float level = 0.0f;
        if (col == TILES_CIRCLE_BUTTON_COL) {
            level = beat_flash;
        } else if (col == TILES_DIAMOND_BUTTON_COL) {
            level = diamond_led;
        } else if (col == TILES_MINUS_BUTTON_COL && s_octave < 0) {
            level = HARMONY_OCTAVE_LED_STEP * (float)(-s_octave);
        } else if (col == TILES_PLUS_BUTTON_COL && s_octave > 0) {
            level = HARMONY_OCTAVE_LED_STEP * (float)s_octave;
        }
        tiles_buttons_set_standby_led(board_button_for_col(col), level);
    }
    for (uint8_t i = 0; i < TILES_NUM_UNDERGLOW_ANCHORS; i++) {
        tiles_lighting_set_standby_underglow_rgb(i, TILES_HARMONY_COLOR_R, TILES_HARMONY_COLOR_G, TILES_HARMONY_COLOR_B);
    }
}
