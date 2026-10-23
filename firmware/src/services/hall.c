#include "hall.h"

#include "board_pins.h"
#include "pad_config.h"
#include "rest_guard.h"

#include "tca9548a.h"
#include "tmag5273.h"

#include "touch.h"

#include "pico/time.h"

#include <stdio.h>

static tiles_tca9548a_t s_hall_muxes[TILES_NUM_HALL_MUXES]; /* index 0/1/2 = mux 1/2/3 */
static bool s_pad_init_ok[TILES_NUM_PADS];
static tiles_hall_sample_t s_pad_sample[TILES_NUM_PADS];
/* Each pad's rest Z and the rules that keep it true (rest_guard.h). */
static tiles_rest_guard_t s_rest[TILES_NUM_PADS];
static uint8_t s_scan_cursor;
/* Release follow-up (hall.h): touched last call, and when a released pad
 * stops being read every call (0 = not following). */
static bool s_pad_was_touched[TILES_NUM_PADS];
static uint32_t s_pad_follow_until_ms[TILES_NUM_PADS];

static int mux_index_for_addr(uint8_t mux_i2c_addr) {
    if (mux_i2c_addr == TILES_I2C0_ADDR_HALL_MUX1) {
        return 0;
    }
    if (mux_i2c_addr == TILES_I2C0_ADDR_HALL_MUX2) {
        return 1;
    }
    if (mux_i2c_addr == TILES_I2C0_ADDR_HALL_MUX3) {
        return 2;
    }
    return -1;
}

static void disable_all_hall_muxes(void) {
    for (uint8_t i = 0; i < TILES_NUM_HALL_MUXES; i++) {
        tiles_tca9548a_disable_all(&s_hall_muxes[i]);
    }
}

/* Disables every Hall channel, then opens only this pad's channel, so at
 * most one channel across the three muxes is open. */
static bool select_pad(const tiles_pad_config_t *cfg) {
    disable_all_hall_muxes();

    int idx = mux_index_for_addr(cfg->hall.mux_i2c_addr);
    if (idx < 0) {
        return false;
    }
    return tiles_tca9548a_select_channel(&s_hall_muxes[idx], cfg->hall.mux_channel);
}

/* Select, read, deselect one pad; stores the timestamped sample. Used by
 * both the touched-pad pass and the round-robin. */
static void read_pad(uint8_t pad_index /* 0-23 */) {
    const tiles_pad_config_t *cfg = board_pad_config((uint8_t)(pad_index + 1u));
    if (cfg == NULL || !select_pad(cfg)) {
        s_pad_sample[pad_index].valid = false;
        disable_all_hall_muxes();
        return;
    }

    tiles_tmag5273_t dev = {.bus = i2c0, .addr = cfg->hall.sensor_i2c_addr};
    tiles_tmag5273_sample_t raw;
    bool ok = tiles_tmag5273_read_xyz(&dev, &raw);

    disable_all_hall_muxes();

    if (ok) {
        s_pad_sample[pad_index].x = raw.x;
        s_pad_sample[pad_index].y = raw.y;
        s_pad_sample[pad_index].z = raw.z;
        s_pad_sample[pad_index].sample_time_ms = to_ms_since_boot(get_absolute_time());
    }
    s_pad_sample[pad_index].valid = ok;
}

/* For a pad just read by the background pass (untouched): its rest guard
 * drifts, re-zeroes or confirms (rest_guard.h). */
static void update_rest_guard(uint8_t pad_index) {
    if (!s_pad_init_ok[pad_index] || !s_pad_sample[pad_index].valid) {
        return;
    }
    tiles_rest_guard_t *g = &s_rest[pad_index];
    if (tiles_touch_is_touched((uint8_t)(pad_index + 1u))) {
        tiles_rest_guard_touched(g); /* can't normally happen here */
        return;
    }
    bool was_confirmed = g->confirmed;
    int16_t old = g->baseline;
    if (tiles_rest_guard_untouched(g, s_pad_sample[pad_index].z, s_pad_sample[pad_index].sample_time_ms)) {
        printf("[hall] pad %u %s (rest moved %d)\n", (unsigned)(pad_index + 1u),
               was_confirmed ? "re-zeroed" : "confirmed at rest", (int)(g->baseline - old));
    }
}

bool tiles_hall_init(void) {
    tiles_tca9548a_init(&s_hall_muxes[0], i2c0, TILES_I2C0_ADDR_HALL_MUX1);
    tiles_tca9548a_init(&s_hall_muxes[1], i2c0, TILES_I2C0_ADDR_HALL_MUX2);
    tiles_tca9548a_init(&s_hall_muxes[2], i2c0, TILES_I2C0_ADDR_HALL_MUX3);
    disable_all_hall_muxes();

    bool all_ok = true;
    s_scan_cursor = 0;

    for (uint8_t i = 0; i < TILES_NUM_PADS; i++) {
        s_pad_sample[i] = (tiles_hall_sample_t){0};
        s_pad_init_ok[i] = false;
        tiles_rest_guard_set(&s_rest[i], 0, true);

        const tiles_pad_config_t *cfg = board_pad_config((uint8_t)(i + 1u));
        if (cfg == NULL || !select_pad(cfg)) {
            all_ok = false;
            disable_all_hall_muxes();
            continue;
        }

        bool ok = tiles_tmag5273_identify(i2c0, cfg->hall.sensor_i2c_addr);
        if (ok) {
            tiles_tmag5273_t dev;
            ok = tiles_tmag5273_init(&dev, i2c0, cfg->hall.sensor_i2c_addr);

            /* Baseline: this pad is assumed at rest now (see the header); the
             * rest guard repairs it if it wasn't. */
            if (ok) {
                tiles_tmag5273_sample_t raw;
                if (tiles_tmag5273_read_xyz(&dev, &raw)) {
                    tiles_rest_guard_set(&s_rest[i], raw.z, true);
                }
            }
        }

        s_pad_init_ok[i] = ok;
        all_ok = all_ok && ok;

        disable_all_hall_muxes();
    }

    return all_ok;
}

bool tiles_hall_last_init_ok(uint8_t logical_pad) {
    if (logical_pad < 1u || logical_pad > TILES_NUM_PADS) {
        return false;
    }
    return s_pad_init_ok[logical_pad - 1u];
}

void tiles_hall_scan(void) {
    /* Priority pass: every touched, initialized pad is read this call, and
     * every pad just released until its key reads up (see hall.h). */
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    for (uint8_t i = 0; i < TILES_NUM_PADS; i++) {
        uint8_t pad = (uint8_t)(i + 1u);
        bool touched = tiles_touch_is_touched(pad);
        if (s_pad_was_touched[i] && !touched) {
            s_pad_follow_until_ms[i] = now_ms + TILES_HALL_RELEASE_FOLLOW_MS;
            if (s_pad_follow_until_ms[i] == 0u) {
                s_pad_follow_until_ms[i] = 1u;
            }
        }
        s_pad_was_touched[i] = touched;
        bool following = !touched && s_pad_follow_until_ms[i] != 0u;
        if (following && (int32_t)(now_ms - s_pad_follow_until_ms[i]) >= 0) {
            s_pad_follow_until_ms[i] = 0u;
            following = false;
        }
        if (!s_pad_init_ok[i] || (!touched && !following)) {
            continue;
        }
        read_pad(i);
        if (following && tiles_hall_get_depth(pad) < TILES_HALL_KEY_UP_DEPTH) {
            s_pad_follow_until_ms[i] = 0u; /* key is up: back to the round-robin */
        }
    }

    /* Background pass: one untouched, initialized pad per call. */
    for (uint8_t attempts = 0; attempts < TILES_NUM_PADS; attempts++) {
        uint8_t pad_index = s_scan_cursor;
        s_scan_cursor = (uint8_t)((s_scan_cursor + 1u) % TILES_NUM_PADS);

        if (!s_pad_init_ok[pad_index] || tiles_touch_is_touched((uint8_t)(pad_index + 1u))) {
            continue; /* covered by the priority pass, or not initialized */
        }

        read_pad(pad_index);
        update_rest_guard(pad_index);
        return;
    }
    /* Everything touched or nothing initialized: no background read this call. */
}

tiles_hall_sample_t tiles_hall_get_sample(uint8_t logical_pad) {
    if (logical_pad < 1u || logical_pad > TILES_NUM_PADS) {
        return (tiles_hall_sample_t){0};
    }
    return s_pad_sample[logical_pad - 1u];
}

bool tiles_hall_recapture_baseline(void) {
    bool all_ok = true;
    for (uint8_t i = 0; i < TILES_NUM_PADS; i++) {
        if (!s_pad_init_ok[i]) {
            continue;
        }
        read_pad(i);
        if (!s_pad_sample[i].valid) {
            all_ok = false;
        } else if (tiles_touch_is_touched((uint8_t)(i + 1u))) {
            /* A finger on it: this reading may be pressed. Keep the old
             * baseline, untrusted, until the pad is seen at rest. */
            tiles_rest_guard_set(&s_rest[i], s_rest[i].baseline, false);
        } else {
            tiles_rest_guard_set(&s_rest[i], s_pad_sample[i].z, true);
        }
    }
    return all_ok;
}

uint16_t tiles_hall_get_depth(uint8_t logical_pad) {
    if (logical_pad < 1u || logical_pad > TILES_NUM_PADS) {
        return 0u;
    }
    uint8_t i = (uint8_t)(logical_pad - 1u);
    if (!s_pad_init_ok[i] || !s_pad_sample[i].valid) {
        return 0u;
    }
    return tiles_rest_guard_depth(&s_rest[i], s_pad_sample[i].z);
}

tiles_hall_rest_info_t tiles_hall_get_rest_info(void) {
    tiles_hall_rest_info_t info = {0u, 0u};
    for (uint8_t i = 0; i < TILES_NUM_PADS; i++) {
        info.rezeros += s_rest[i].rezeros;
        if (s_pad_init_ok[i] && !s_rest[i].confirmed) {
            info.unconfirmed++;
        }
    }
    return info;
}
