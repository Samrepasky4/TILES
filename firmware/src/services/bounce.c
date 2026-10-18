#include "bounce.h"

#include "board_layout.h"
#include "board_pins.h"
#include "bounce_core.h"
#include "buttons.h"
#include "haptics.h"
#include "lighting.h"
#include "midi_channels.h"
#include "midi_out.h"
#include "note_map.h"
#include "touch.h"

#include "pico/time.h"

#include <math.h>

#define BOUNCE_CHANNEL TILES_MIDI_CH_BOUNCE
#define BOUNCE_VISIBLE_LANES 6u
#define BOUNCE_MAX_OFFSET (TILES_BOUNCE_LANES - BOUNCE_VISIBLE_LANES)
#define BOUNCE_PULSES_PER_STEP 6u /* a sixteenth at 24 pulses per beat */
/* Lane 1's note before the key and octave: C3. */
#define BOUNCE_BASE_NOTE 48u
#define BOUNCE_PI 3.14159265358979323846f

/* Landing velocity by drop height (index = height): a higher drop lands
 * harder. First guesses. */
static const uint8_t VELOCITY_FOR_HEIGHT[TILES_BOUNCE_MAX_HEIGHT + 1u] = {0u, 84u, 98u, 112u, 127u};

/* Light levels and times: first guesses. */
#define BOUNCE_HEIGHT_LEVEL 0.35f
#define BOUNCE_EMPTY_LANE_LEVEL 0.1f
#define BOUNCE_LAND_FLASH_MS 70u
#define BOUNCE_END_FLASH_MS 220u
#define BOUNCE_TRANSPORT_LED_LEVEL 1.0f

static tiles_bounce_t s_balls;
static bool s_running;
static uint32_t s_step_pulse;   /* the clock pulse the last step fell on */
static uint8_t s_offset;        /* first lane shown, 0..BOUNCE_MAX_OFFSET */
static uint8_t s_sounding[TILES_BOUNCE_LANES]; /* note playing per lane, 0 = none */
static uint32_t s_landed_ms[TILES_BOUNCE_LANES];
static uint32_t s_end_flash_ms;
static bool s_rewound;
static bool s_prev_touched[TILES_NUM_PADS];

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

/* Lane k's note: the k-th note of the scale from the key note up (the key
 * note itself for lane 0), at the octave set elsewhere. */
static uint8_t lane_note(uint8_t lane) {
    int base = (int)BOUNCE_BASE_NOTE + tiles_note_map_get_key_offset() + 12 * tiles_note_map_get_octave_shift();
    uint8_t found = 0u;
    for (int note = base; note < 128; note++) {
        if (note < 0) {
            continue;
        }
        if (tiles_note_map_quantize_to_scale((uint8_t)note) == (uint8_t)note) {
            if (found == lane) {
                return (uint8_t)note;
            }
            found++;
        }
    }
    return 0u;
}

static void lane_off(uint8_t lane) {
    if (s_sounding[lane] != 0u) {
        tiles_midi_note_off(BOUNCE_CHANNEL, s_sounding[lane], 0u);
        s_sounding[lane] = 0u;
    }
}

void tiles_bounce_mode_end_all_notes(void) {
    for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
        lane_off(lane);
    }
}

/* The column's bottom pad, when the lane is on screen (0 if not). */
static uint8_t floor_pad_for(uint8_t lane) {
    if (lane < s_offset || lane >= s_offset + BOUNCE_VISIBLE_LANES) {
        return 0u;
    }
    return board_pad_for_row_col(TILES_GRID_MAX_ROW, (uint8_t)(lane - s_offset + 1u));
}

static void play_landings(uint16_t landed) {
    uint32_t now = now_ms();
    for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
        if (!((landed >> lane) & 1u)) {
            continue;
        }
        lane_off(lane);
        uint8_t note = lane_note(lane);
        if (note != 0u) {
            tiles_midi_note_on(BOUNCE_CHANNEL, note, VELOCITY_FOR_HEIGHT[s_balls.height[lane]]);
            s_sounding[lane] = note;
        }
        s_landed_ms[lane] = now | 1u;
        uint8_t pad = floor_pad_for(lane);
        if (pad != 0u) {
            tiles_haptics_trigger_touch_pulse(pad); /* feel the bounce */
        }
    }
}

void tiles_bounce_mode_init(void) {
    tiles_bounce_clear(&s_balls);
    s_running = false;
    s_offset = 0u;
    s_end_flash_ms = 0u;
    s_rewound = true;
    for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
        s_sounding[lane] = 0u;
        s_landed_ms[lane] = 0u;
    }
    tiles_bounce_mode_enter();
}

void tiles_bounce_mode_enter(void) {
    for (uint8_t pad = 1u; pad <= TILES_NUM_PADS; pad++) {
        s_prev_touched[pad - 1u] = tiles_touch_is_touched(pad);
    }
}

bool tiles_bounce_mode_is_running(void) {
    return s_running;
}

void tiles_bounce_mode_start(bool from_floor) {
    s_running = true;
    s_rewound = false;
    if (from_floor) {
        /* Beat one: every ball lands now, then bounces from the floor. */
        uint16_t all = 0u;
        for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
            s_balls.phase[lane] = 0u;
            if (s_balls.height[lane] != 0u) {
                all |= (uint16_t)(1u << lane);
            }
        }
        play_landings(all);
    }
}

void tiles_bounce_mode_pause(void) {
    s_running = false;
    tiles_bounce_mode_end_all_notes();
}

void tiles_bounce_mode_rewind(void) {
    tiles_bounce_drop_all(&s_balls);
    s_rewound = true;
}

void tiles_bounce_mode_scroll(int8_t direction) {
    int next = (int)s_offset + direction;
    if (next < 0) {
        next = 0;
    }
    if (next > (int)BOUNCE_MAX_OFFSET) {
        next = (int)BOUNCE_MAX_OFFSET;
    }
    s_offset = (uint8_t)next;
    if (s_offset == 0u || s_offset == BOUNCE_MAX_OFFSET) {
        s_end_flash_ms = now_ms() | 1u; /* at an end (or pushing past it) */
    }
}

void tiles_bounce_mode_advance(tiles_midi_clock_state_t clock) {
    if (!s_running || !clock.running) {
        s_step_pulse = clock.pulse_count; /* no catching up after a pause */
        return;
    }
    if (clock.start_edge) {
        s_step_pulse = clock.pulse_count;
        return;
    }
    while (clock.pulse_count - s_step_pulse >= BOUNCE_PULSES_PER_STEP) {
        s_step_pulse += BOUNCE_PULSES_PER_STEP;
        /* A note lasts one step, or until its lane lands again. */
        for (uint8_t lane = 0; lane < TILES_BOUNCE_LANES; lane++) {
            lane_off(lane);
        }
        play_landings(tiles_bounce_step(&s_balls));
    }
}

void tiles_bounce_mode_handle_input(uint32_t now) {
    (void)now;
    for (uint8_t row = 1u; row <= TILES_GRID_MAX_ROW; row++) {
        for (uint8_t col = TILES_GRID_MIN_COL; col <= TILES_GRID_MAX_COL; col++) {
            uint8_t pad = board_pad_for_row_col(row, col);
            bool touched = tiles_touch_is_touched(pad);
            bool was = s_prev_touched[pad - 1u];
            s_prev_touched[pad - 1u] = touched;
            if (!touched || was) {
                continue;
            }
            uint8_t lane = (uint8_t)(s_offset + col - 1u);
            uint8_t height = (uint8_t)(TILES_GRID_MAX_ROW + 1u - row); /* bottom row = 1 */
            if (s_balls.height[lane] == height) {
                tiles_bounce_set_height(&s_balls, lane, 0u); /* the same height again: off */
                lane_off(lane);
            } else {
                tiles_bounce_set_height(&s_balls, lane, height);
            }
            tiles_haptics_trigger_touch_pulse(pad);
        }
    }
}

static float slow_pulse(uint32_t now, float period_ms, float lo, float hi) {
    float raw = 0.5f + 0.5f * sinf(2.0f * BOUNCE_PI * (float)now / period_ms);
    return lo + (hi - lo) * raw;
}

/* A lane's colour: a purple that shades from blue-violet on lane 1 to
 * magenta-violet on lane 12 (stopping short of the melodic magenta and the
 * end flash's pink), so the page's colours show where you've scrolled to. */
#define BOUNCE_LANE_RED_FIRST 0.2f
#define BOUNCE_LANE_RED_LAST 0.85f
static void lane_rgb(uint8_t lane, float *r, float *g, float *b) {
    float t = (float)lane / (float)(TILES_BOUNCE_LANES - 1u);
    *r = BOUNCE_LANE_RED_FIRST + t * (BOUNCE_LANE_RED_LAST - BOUNCE_LANE_RED_FIRST);
    *g = 0.0f;
    *b = 1.0f;
}

void tiles_bounce_mode_render(uint32_t now, float beat_flash, float diamond_led, bool clock_running) {
    bool end_flash = s_end_flash_ms != 0u && now - s_end_flash_ms < BOUNCE_END_FLASH_MS;
    for (uint8_t col = TILES_GRID_MIN_COL; col <= TILES_GRID_MAX_COL; col++) {
        uint8_t lane = (uint8_t)(s_offset + col - 1u);
        uint8_t height = s_balls.height[lane];
        uint8_t ball = tiles_bounce_ball_height(&s_balls, lane);
        float lr, lg, lb;
        lane_rgb(lane, &lr, &lg, &lb);
        bool landing = s_landed_ms[lane] != 0u && now - s_landed_ms[lane] < BOUNCE_LAND_FLASH_MS;
        for (uint8_t row = 1u; row <= TILES_GRID_MAX_ROW; row++) {
            uint8_t pad = board_pad_for_row_col(row, col);
            uint8_t level_h = (uint8_t)(TILES_GRID_MAX_ROW + 1u - row);
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (end_flash) {
                r = 1.0f;
                b = 1.0f; /* the end of the lanes: pink */
            } else if (height != 0u && ball == level_h && level_h == 1u && landing) {
                r = g = b = 1.0f;
            } else if (height != 0u && ball == level_h) {
                r = lr;
                g = lg;
                b = lb;
            } else if (height != 0u && level_h == height) {
                r = lr * BOUNCE_HEIGHT_LEVEL;
                g = lg * BOUNCE_HEIGHT_LEVEL;
                b = lb * BOUNCE_HEIGHT_LEVEL;
            } else if (height == 0u && level_h == 1u) {
                r = lr * BOUNCE_EMPTY_LANE_LEVEL;
                g = lg * BOUNCE_EMPTY_LANE_LEVEL;
                b = lb * BOUNCE_EMPTY_LANE_LEVEL;
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
        } else if (col == TILES_MINUS_BUTTON_COL && !s_running) {
            /* "-": solid when rewound, slow pulse when paused, as in the sequencer. */
            level = s_rewound ? BOUNCE_TRANSPORT_LED_LEVEL : slow_pulse(now, 3000.0f, 0.03f, 0.35f);
        } else if (col == TILES_PLUS_BUTTON_COL && (s_running || clock_running)) {
            level = s_running ? BOUNCE_TRANSPORT_LED_LEVEL
                              : slow_pulse(now, tiles_midi_clock_get_ms_per_beat(), 0.0f, 1.0f);
        }
        tiles_buttons_set_standby_led(board_button_for_col(col), level);
    }
    for (uint8_t i = 0; i < TILES_NUM_UNDERGLOW_ANCHORS; i++) {
        tiles_lighting_set_standby_underglow_rgb(i, TILES_BOUNCE_COLOR_R, TILES_BOUNCE_COLOR_G, TILES_BOUNCE_COLOR_B);
    }
}
