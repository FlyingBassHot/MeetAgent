#include <math.h>
#include <string.h>

#include "glyph_atlas.h"
#include "glyph_frame.h"
#include "glyph_ramp.h"
#include "glyph_rasterizer.h"
#include "orbit_geometry.h"
#include "phosphor_lut.h"
#include "phosphor_core.h"
#include "phosphor_demo.h"
#include "phosphor_motion.h"
#include "phosphor_state.h"
#include "phosphor_palette.h"
#include "phosphor_scene.h"
#include "radial_field.h"
#include "satellite_layout.h"
#include "test_assert.h"

#define NEAR(a, b) (fabsf((a) - (b)) < 0.001f)

static void test_palette_rgb565(void)
{
    const phosphor_palette_t *palette = phosphor_palette_default();
    TEST_ASSERT_NOT_NULL(palette);
    TEST_ASSERT_EQ_INT(0x0861, phosphor_rgb565_from_hex(0x080C0A));
    TEST_ASSERT_EQ_INT(0x10C2, phosphor_rgb565_from_hex(0x101813));
    TEST_ASSERT_EQ_INT(0xA78D, phosphor_rgb565_from_hex(0xA7F36B));
    TEST_ASSERT_EQ_INT(0x6633, phosphor_rgb565_from_hex(0x62C79B));
    TEST_ASSERT_EQ_INT(0x5B8C, phosphor_rgb565_from_hex(0x587364));
    TEST_ASSERT_EQ_INT(0xF5AB, phosphor_rgb565_from_hex(0xF2B75E));
    TEST_ASSERT_EQ_INT(0xF36C, phosphor_rgb565_from_hex(0xF06C67));
    TEST_ASSERT_EQ_INT(0xDFF8, phosphor_rgb565_from_hex(0xD9FFC3));

    TEST_ASSERT_EQ_INT(0x0861, palette->background);
    TEST_ASSERT_EQ_INT(0x10C2, palette->surface);
    TEST_ASSERT_EQ_INT(0xA78D, palette->primary);
    TEST_ASSERT_EQ_INT(0x6633, palette->secondary);
    TEST_ASSERT_EQ_INT(0x5B8C, palette->muted);
    TEST_ASSERT_EQ_INT(0xF5AB, palette->warning);
    TEST_ASSERT_EQ_INT(0xF36C, palette->error);
    TEST_ASSERT_EQ_INT(0xDFF8, palette->highlight);
    TEST_ASSERT_EQ_INT(0xA78D,
                       phosphor_color565(palette, GLYPH_COLOR_PRIMARY));
    TEST_ASSERT_EQ_INT(0xF36C, phosphor_color565(NULL, GLYPH_COLOR_ERROR));
    TEST_ASSERT_EQ_INT(0x0000, phosphor_scale565(0xA78D, 0));
    TEST_ASSERT_EQ_INT(0xA78D, phosphor_scale565(0xA78D, 255));
    uint16_t previous = 0;
    for (int i = 0; i <= 255; ++i) {
        uint16_t scaled = phosphor_scale565(0xA78D, (uint8_t)i);
        TEST_ASSERT_TRUE(scaled >= previous);
        previous = scaled;
    }
}

static void test_glyph_ramp(void)
{
    const char *ramp = glyph_ramp();
    TEST_ASSERT_NOT_NULL(ramp);
    TEST_ASSERT_STR_EQ(" .,:-=+*#%@", ramp);
    TEST_ASSERT_EQ_INT(' ', glyph_ramp_char(0));
    TEST_ASSERT_EQ_INT('@', glyph_ramp_char(255));
    TEST_ASSERT_EQ_INT(0, glyph_ramp_index(0));
    TEST_ASSERT_EQ_INT(10, glyph_ramp_index(255));

    uint8_t previous = 0;
    for (int i = 0; i <= 255; ++i) {
        uint8_t index = glyph_ramp_index((uint8_t)i);
        char ch = glyph_ramp_char((uint8_t)i);
        TEST_ASSERT_TRUE(index >= previous);
        TEST_ASSERT_TRUE(index <= 10);
        TEST_ASSERT_TRUE(ch != '\0');
        if (ch != ' ') {
            TEST_ASSERT_TRUE(glyph_atlas_contains(ch));
        }
        TEST_ASSERT_EQ_INT(glyph_atlas_index(ch), glyph_ramp_atlas((uint8_t)i));
        previous = index;
    }
}

static void test_glyph_atlas_coverage(void)
{
    TEST_ASSERT_EQ_INT(18, PHOSPHOR_CELL_W);
    TEST_ASSERT_EQ_INT(27, PHOSPHOR_CELL_H);
    TEST_ASSERT_EQ_INT(24, PHOSPHOR_GRID_W);
    TEST_ASSERT_EQ_INT(16, PHOSPHOR_GRID_H);
    TEST_ASSERT_EQ_INT(432, PHOSPHOR_CANVAS_W);
    const char *required = " .,:-=+*#%@/\\|<>^v;[]{}?_!o0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZx";
    for (size_t i = 0; required[i] != '\0'; ++i) {
        TEST_ASSERT_TRUE(glyph_atlas_contains(required[i]));
    }
    TEST_ASSERT_FALSE(glyph_atlas_contains('~'));
    TEST_ASSERT_EQ_INT(' ', glyph_atlas_char_at(0));
    uint8_t greater_bits = 0;
    uint8_t underscore_bits = 0;
    uint8_t zero_bits = 0;
    uint8_t one_bits = 0;
    for (int y = 0; y < GLYPH_ATLAS_H; ++y) {
        for (int x = 0; x < GLYPH_ATLAS_W; ++x) {
            TEST_ASSERT_FALSE(glyph_atlas_pixel(0, (uint8_t)x, (uint8_t)y));
            greater_bits |= glyph_atlas_pixel(glyph_atlas_index('>'),
                                               (uint8_t)x, (uint8_t)y);
            underscore_bits |= glyph_atlas_pixel(glyph_atlas_index('_'),
                                                  (uint8_t)x, (uint8_t)y);
            zero_bits |= glyph_atlas_pixel(glyph_atlas_index('0'),
                                            (uint8_t)x, (uint8_t)y);
            one_bits |= glyph_atlas_pixel(glyph_atlas_index('1'),
                                           (uint8_t)x, (uint8_t)y);
        }
    }
    TEST_ASSERT_TRUE(greater_bits != 0);
    TEST_ASSERT_TRUE(underscore_bits != 0);
    TEST_ASSERT_TRUE(zero_bits != 0);
    TEST_ASSERT_TRUE(one_bits != 0);
}

static void test_orbit_geometry(void)
{
    const phosphor_orbit_t *orbits = phosphor_orbit_defaults();
    TEST_ASSERT_NOT_NULL(orbits);
    TEST_ASSERT_TRUE(NEAR(orbits[0].radius, 80.0f));
    TEST_ASSERT_TRUE(NEAR(orbits[1].radius, 140.0f));
    TEST_ASSERT_TRUE(NEAR(orbits[2].radius, 192.0f));
    TEST_ASSERT_TRUE(NEAR(orbits[0].tilt, 1.00f));
    TEST_ASSERT_TRUE(NEAR(orbits[1].tilt, 0.25f));
    TEST_ASSERT_TRUE(NEAR(orbits[2].tilt, 1.30f));
    TEST_ASSERT_TRUE(NEAR(orbits[0].rotation, -0.55f));
    TEST_ASSERT_TRUE(NEAR(orbits[1].rotation, 0.85f));
    TEST_ASSERT_TRUE(NEAR(orbits[2].rotation, -0.65f));
    TEST_ASSERT_TRUE(NEAR(orbits[0].phase, 0.10f));
    TEST_ASSERT_TRUE(NEAR(orbits[1].phase, 1.90f));
    TEST_ASSERT_TRUE(NEAR(orbits[2].phase, 3.80f));
    TEST_ASSERT_TRUE(orbits[0].samples >= 80);
    TEST_ASSERT_TRUE(orbits[0].samples <= 100);
    TEST_ASSERT_TRUE(orbits[2].samples >= 80);
    TEST_ASSERT_TRUE(orbits[2].samples <= 100);

    for (int o = 0; o < 3; ++o) {
        for (int i = 0; i < 64; ++i) {
            float theta = (float)i * 6.2831853f / 64.0f;
            phosphor_point_t p;
            phosphor_orbit_point(&orbits[o], theta, 216.0f, 216.0f, &p);
            TEST_ASSERT_TRUE(p.x >= 8.0f && p.x <= 424.0f);
            TEST_ASSERT_TRUE(p.y >= 8.0f && p.y <= 424.0f);
        }
    }

    float dx = 0.0f;
    float dy = 0.0f;
    phosphor_orbit_tangent(&orbits[0], 0.0f, &dx, &dy);
    TEST_ASSERT_TRUE(fabsf(dx) + fabsf(dy) > 0.0f);
    TEST_ASSERT_EQ_INT('-', phosphor_orbit_glyph(1.0f, 0.0f));
    TEST_ASSERT_EQ_INT('|', phosphor_orbit_glyph(0.0f, 1.0f));
    TEST_ASSERT_TRUE(phosphor_orbit_glyph(1.0f, 1.0f) != '-');
    TEST_ASSERT_TRUE(phosphor_orbit_glyph(1.0f, 1.0f) != '|');

    uint8_t col = 99;
    uint8_t row = 99;
    TEST_ASSERT_TRUE(phosphor_point_to_cell(0.0f, 0.0f, &col, &row));
    TEST_ASSERT_EQ_INT(0, col);
    TEST_ASSERT_EQ_INT(0, row);
    TEST_ASSERT_TRUE(phosphor_point_to_cell(431.0f, 431.0f, &col, &row));
    TEST_ASSERT_EQ_INT(23, col);
    TEST_ASSERT_EQ_INT(15, row);
    TEST_ASSERT_FALSE(phosphor_point_to_cell(-1.0f, 0.0f, &col, &row));
    TEST_ASSERT_FALSE(phosphor_point_to_cell(0.0f, 432.0f, &col, &row));
}

static void test_radial_field(void)
{
    TEST_ASSERT_TRUE(NEAR(phosphor_smooth_falloff(0.0f, 10.0f), 1.0f));
    TEST_ASSERT_TRUE(NEAR(phosphor_smooth_falloff(10.0f, 10.0f), 0.0f));
    TEST_ASSERT_TRUE(NEAR(phosphor_smooth_falloff(20.0f, 10.0f), 0.0f));
    TEST_ASSERT_EQ_INT(' ', phosphor_intensity_to_char(0.0f));
    TEST_ASSERT_EQ_INT('@', phosphor_intensity_to_char(1.0f));
    TEST_ASSERT_EQ_INT(' ', phosphor_intensity_to_char(-1.0f));
    TEST_ASSERT_EQ_INT('@', phosphor_intensity_to_char(2.0f));

    glyph_frame_t frame;
    glyph_frame_clear(&frame);
    phosphor_radial_field_apply(&frame, 216.0f, 216.0f, 0.0f, 45.0f, 120,
                                GLYPH_COLOR_SECONDARY);
    bool found = false;
    for (int r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (int c = 0; c < PHOSPHOR_GRID_W; ++c) {
            if (frame.cells[r][c].glyph != 0) {
                found = true;
            }
        }
    }
    TEST_ASSERT_TRUE(found);
}

static void test_satellite_layout(void)
{
    phosphor_satellite_layout_t layout;
    phosphor_satellite_layout_static(&layout);
    TEST_ASSERT_EQ_INT(2, layout.count);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SATELLITE_RUNNING, layout.items[0].status);
    TEST_ASSERT_EQ_INT(PHOSPHOR_SATELLITE_WARNING, layout.items[1].status);
    TEST_ASSERT_TRUE(!NEAR(layout.items[0].angle, layout.items[1].angle));

    const phosphor_orbit_t *orbits = phosphor_orbit_defaults();
    uint8_t c0 = 0;
    uint8_t r0 = 0;
    uint8_t c1 = 0;
    uint8_t r1 = 0;
    TEST_ASSERT_TRUE(phosphor_satellite_cell(&orbits[2], &layout.items[0],
                                            216.0f, 216.0f, &c0, &r0));
    TEST_ASSERT_TRUE(phosphor_satellite_cell(&orbits[2], &layout.items[1],
                                            216.0f, 216.0f, &c1, &r1));
    TEST_ASSERT_TRUE(c0 != c1 || r0 != r1);
    float x = (float)c0 * PHOSPHOR_CELL_W + PHOSPHOR_CELL_W / 2.0f;
    float y = (float)r0 * PHOSPHOR_CELL_H + PHOSPHOR_CELL_H / 2.0f;
    float d = sqrtf((x - 216.0f) * (x - 216.0f) +
                    (y - 216.0f) * (y - 216.0f));
    TEST_ASSERT_TRUE(d > 80.0f);
}

static void test_glyph_frame_priority(void)
{
    glyph_frame_t frame;
    glyph_frame_clear(&frame);
    uint8_t col = 0;
    uint8_t row = 0;
    TEST_ASSERT_FALSE(phosphor_point_to_cell(-1.0f, 0.0f, &col, &row));

    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('-'),
                                     GLYPH_COLOR_MUTED, 80,
                                     GLYPH_FLAG_BACK));
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('='),
                                     GLYPH_COLOR_SECONDARY, 100,
                                     GLYPH_FLAG_FRONT));
    TEST_ASSERT_EQ_INT(glyph_atlas_index('='), frame.cells[1][1].glyph);
    TEST_ASSERT_FALSE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('+'),
                                      GLYPH_COLOR_MUTED, 255,
                                      GLYPH_FLAG_BACK));
    TEST_ASSERT_EQ_INT(glyph_atlas_index('='), frame.cells[1][1].glyph);

    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('?'),
                                     GLYPH_COLOR_WARNING, 255,
                                     GLYPH_FLAG_SATELLITE));
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('>'),
                                     GLYPH_COLOR_HIGHLIGHT, 255,
                                     GLYPH_FLAG_NUCLEUS));
    TEST_ASSERT_FALSE(glyph_frame_set(&frame, 1, 1, glyph_atlas_index('!'),
                                      GLYPH_COLOR_ERROR, 255,
                                      GLYPH_FLAG_SATELLITE));
    TEST_ASSERT_EQ_INT(glyph_atlas_index('>'), frame.cells[1][1].glyph);
    TEST_ASSERT_EQ_INT(GLYPH_FLAG_NUCLEUS, frame.cells[1][1].flags);
    TEST_ASSERT_FALSE(glyph_frame_set(&frame, PHOSPHOR_GRID_W, 0,
                                      glyph_atlas_index('.'), GLYPH_COLOR_MUTED,
                                      1, GLYPH_FLAG_FIELD));

    glyph_frame_clear(&frame);
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 2, 2, glyph_atlas_index('.'),
                                     GLYPH_COLOR_MUTED, 10,
                                     GLYPH_FLAG_FIELD));
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 2, 2, glyph_atlas_index('-'),
                                     GLYPH_COLOR_MUTED, 80,
                                     GLYPH_FLAG_BACK));
    TEST_ASSERT_EQ_INT(glyph_atlas_index('-'), frame.cells[2][2].glyph);
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 2, 2, glyph_atlas_index('='),
                                     GLYPH_COLOR_PRIMARY, 90,
                                     GLYPH_FLAG_FRONT));
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 2, 2, glyph_atlas_index('?'),
                                     GLYPH_COLOR_WARNING, 255,
                                     GLYPH_FLAG_SATELLITE));
    TEST_ASSERT_TRUE(glyph_frame_set(&frame, 2, 2, glyph_atlas_index('>'),
                                     GLYPH_COLOR_HIGHLIGHT, 255,
                                     GLYPH_FLAG_NUCLEUS));
    TEST_ASSERT_EQ_INT(glyph_atlas_index('>'), frame.cells[2][2].glyph);

    glyph_frame_clear(&frame);
    TEST_ASSERT_TRUE(glyph_frame_set_scaled(
        &frame, PHOSPHOR_GRID_W - 1, PHOSPHOR_GRID_H - 1,
        glyph_atlas_index('>'), GLYPH_COLOR_HIGHLIGHT, 255,
        GLYPH_FLAG_NUCLEUS, 2));
    TEST_ASSERT_FALSE(glyph_frame_set_scaled(
        &frame, 0, 0, glyph_atlas_index('>'), GLYPH_COLOR_HIGHLIGHT, 255,
        GLYPH_FLAG_NUCLEUS, 3));
    uint16_t edge[16] = {0};
    phosphor_scene_render_rgb565(edge, 4, 4);
}

static void test_motion_and_scene(void)
{
    const phosphor_motion_profile_t *idle = phosphor_motion_idle_profile();
    const phosphor_motion_profile_t *working =
        phosphor_motion_working_profile();
    TEST_ASSERT_TRUE(NEAR(idle->speeds[0], 0.08f));
    TEST_ASSERT_TRUE(NEAR(idle->speeds[1], -0.045f));
    TEST_ASSERT_TRUE(NEAR(idle->speeds[2], 0.025f));
    TEST_ASSERT_TRUE(NEAR(working->speeds[0], 0.25f));
    TEST_ASSERT_TRUE(NEAR(working->speeds[1], -0.62f));
    TEST_ASSERT_TRUE(NEAR(working->speeds[2], 0.36f));
    TEST_ASSERT_TRUE(NEAR(working->density, 0.85f));
    TEST_ASSERT_TRUE(working->density > idle->density);
    TEST_ASSERT_TRUE(working->brightness >= idle->brightness);

    phosphor_lut_init();
    TEST_ASSERT_TRUE(fabsf(phosphor_lut_sin(1.5707963f) - 1.0f) < 0.01f);
    TEST_ASSERT_TRUE(fabsf(phosphor_lut_cos(0.0f) - 1.0f) < 0.01f);

    phosphor_motion_state_t a;
    phosphor_motion_state_t b;
    phosphor_motion_init(&a, PHOSPHOR_MODE_IDLE);
    phosphor_motion_init(&b, PHOSPHOR_MODE_IDLE);
    for (uint32_t t = 16; t <= 1000; t += 16) {
        phosphor_motion_update(&a, t, PHOSPHOR_MODE_IDLE);
        phosphor_motion_update(&b, t, PHOSPHOR_MODE_IDLE);
    }
    TEST_ASSERT_TRUE(fabsf(a.orbit_phase[0] - b.orbit_phase[0]) < 0.0001f);
    TEST_ASSERT_TRUE(fabsf(a.density - b.density) < 0.0001f);
    TEST_ASSERT_TRUE(fabsf(a.breath - b.breath) < 0.0001f);
    /* Wrapped phase is in [0, 2pi); negative middle speed appears as ~6.24. */
    TEST_ASSERT_TRUE(a.orbit_phase[1] > 6.0f);

    phosphor_motion_state_t transition;
    phosphor_motion_init(&transition, PHOSPHOR_MODE_IDLE);
    for (uint32_t t = 16; t <= 500; t += 16) {
        phosphor_motion_update(&transition, t, PHOSPHOR_MODE_WORKING);
    }
    TEST_ASSERT_TRUE(transition.mode_mix > 0.4f);
    TEST_ASSERT_TRUE(transition.mode_mix < 0.6f);
    TEST_ASSERT_TRUE(transition.density > idle->density);
    TEST_ASSERT_TRUE(transition.density < working->density);
    for (uint32_t t = 516; t <= 1500; t += 16) {
        phosphor_motion_update(&transition, t, PHOSPHOR_MODE_WORKING);
    }
    TEST_ASSERT_TRUE(transition.mode_mix >= 0.999f);
    TEST_ASSERT_TRUE(transition.activity_energy > 0.5f);

    phosphor_motion_state_t c;
    phosphor_motion_state_t d;
    phosphor_motion_init(&c, PHOSPHOR_MODE_WORKING);
    phosphor_motion_init(&d, PHOSPHOR_MODE_WORKING);
    for (uint32_t t = 16; t <= 1000; t += 16) {
        phosphor_motion_update(&c, t, PHOSPHOR_MODE_WORKING);
        phosphor_motion_update(&d, t, PHOSPHOR_MODE_WORKING);
    }
    TEST_ASSERT_EQ_INT(0, memcmp(&c, &d, sizeof(c)));

    glyph_frame_t idle_frame;
    glyph_frame_t working_frame;
    phosphor_motion_state_t motion;
    phosphor_motion_init(&motion, PHOSPHOR_MODE_IDLE);
    phosphor_motion_update(&motion, 1000, PHOSPHOR_MODE_IDLE);
    phosphor_scene_build_frame_motion(&idle_frame, &motion);
    phosphor_motion_init(&motion, PHOSPHOR_MODE_WORKING);
    phosphor_motion_update(&motion, 1000, PHOSPHOR_MODE_WORKING);
    phosphor_scene_build_frame_motion(&working_frame, &motion);
    TEST_ASSERT_TRUE(memcmp(&idle_frame, &working_frame,
                            sizeof(idle_frame)) != 0);
    int idle_nonempty = 0;
    int working_nonempty = 0;
    for (int r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (int col = 0; col < PHOSPHOR_GRID_W; ++col) {
            idle_nonempty += idle_frame.cells[r][col].glyph != 0;
            working_nonempty += working_frame.cells[r][col].glyph != 0;
        }
    }
    TEST_ASSERT_TRUE(idle_nonempty > 0);
    TEST_ASSERT_TRUE(working_nonempty > 0);
}

static void test_band_neighbors(void)
{
    glyph_frame_t frame;
    phosphor_motion_state_t idle;
    phosphor_motion_state_t working;
    phosphor_motion_init(&idle, PHOSPHOR_MODE_IDLE);
    phosphor_motion_init(&working, PHOSPHOR_MODE_WORKING);

    phosphor_scene_build_frame_motion(&frame, &idle);
    int idle_inner = 0;
    int idle_outer = 0;
    for (int r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (int c = 0; c < PHOSPHOR_GRID_W; ++c) {
            idle_inner += (frame.cells[r][c].flags &
                           GLYPH_FLAG_INNER_BAND) != 0;
            idle_outer += (frame.cells[r][c].flags &
                           GLYPH_FLAG_OUTER_BAND) != 0;
        }
    }
    TEST_ASSERT_TRUE(idle_inner > 0);
    TEST_ASSERT_TRUE(idle_outer > 0);

    phosphor_scene_build_frame_motion(&frame, &working);
    int working_inner = 0;
    int working_outer = 0;
    for (int r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (int c = 0; c < PHOSPHOR_GRID_W; ++c) {
            working_inner += (frame.cells[r][c].flags &
                              GLYPH_FLAG_INNER_BAND) != 0;
            working_outer += (frame.cells[r][c].flags &
                              GLYPH_FLAG_OUTER_BAND) != 0;
        }
    }
    TEST_ASSERT_TRUE(working_inner >= idle_inner);
    TEST_ASSERT_TRUE(working_outer >= idle_outer);
}

static void add_state_session(phosphor_input_state_t *in, const char *id,
                              phosphor_session_status_t status)
{
    if (in->session_count >= PHOSPHOR_MAX_SESSIONS) {
        return;
    }
    phosphor_session_input_t *s = &in->sessions[in->session_count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->session_id, id, sizeof(s->session_id) - 1);
    s->status = status;
}

static void test_state_resolver(void)
{
    phosphor_state_resolver_t resolver;
    phosphor_visual_state_t visual;
    phosphor_input_state_t input;
    phosphor_state_resolver_init(&resolver);

    memset(&input, 0, sizeof(input));
    input.node_online = true;
    add_state_session(&input, "run", PHOSPHOR_SESSION_RUNNING);
    add_state_session(&input, "wait", PHOSPHOR_SESSION_WAITING);
    input.approval_count = 1;
    phosphor_state_resolver_update(&resolver, &input, 1000, &visual);
    TEST_ASSERT_EQ_INT(PHOSPHOR_ACTIVITY_WORKING, visual.activity);
    TEST_ASSERT_TRUE((visual.attention & PHOSPHOR_ATTENTION_APPROVAL) != 0);
    TEST_ASSERT_EQ_INT(2, visual.satellite_count);

    input.error_count = 1;
    add_state_session(&input, "err", PHOSPHOR_SESSION_ERROR);
    phosphor_state_resolver_update(&resolver, &input, 1200, &visual);
    TEST_ASSERT_TRUE((visual.attention & PHOSPHOR_ATTENTION_APPROVAL) != 0);
    TEST_ASSERT_TRUE((visual.attention & PHOSPHOR_ATTENTION_ERROR) != 0);
    TEST_ASSERT_EQ_INT(3, visual.satellite_count);

    input.completed_event_id = 10;
    phosphor_state_resolver_update(&resolver, &input, 1300, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);
    TEST_ASSERT_EQ_INT(0, visual.completion_elapsed_ms);
    input.completed_event_id = 10;
    phosphor_state_resolver_update(&resolver, &input, 1400, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);
    TEST_ASSERT_EQ_INT(100, visual.completion_elapsed_ms);
    phosphor_state_resolver_update(&resolver, &input, 2200, &visual);
    TEST_ASSERT_FALSE(visual.completion_active);
    input.completed_event_id = 11;
    phosphor_state_resolver_update(&resolver, &input, 2300, &visual);
    TEST_ASSERT_TRUE(visual.completion_active);

    input.error_event_id = 20;
    phosphor_state_resolver_update(&resolver, &input, 2400, &visual);
    TEST_ASSERT_TRUE(visual.error_glitch_active);
    input.error_event_id = 20;
    phosphor_state_resolver_update(&resolver, &input, 2600, &visual);
    TEST_ASSERT_TRUE(visual.error_glitch_active);
    phosphor_state_resolver_update(&resolver, &input, 2700, &visual);
    TEST_ASSERT_FALSE(visual.error_glitch_active);

    phosphor_state_resolver_update(&resolver, &input, 2800, &visual);
    TEST_ASSERT_EQ_INT(PHOSPHOR_ACTIVITY_WORKING, visual.activity);
    input.node_online = false;
    input.completed_event_id = 12;
    phosphor_state_resolver_update(&resolver, &input, 2900, &visual);
    TEST_ASSERT_EQ_INT(PHOSPHOR_ACTIVITY_OFFLINE, visual.activity);
    TEST_ASSERT_EQ_INT(0, visual.attention);
    TEST_ASSERT_FALSE(visual.completion_active);
    TEST_ASSERT_EQ_INT(0, visual.satellite_count);

    /* Stable slots + overflow. */
    phosphor_state_resolver_init(&resolver);
    memset(&input, 0, sizeof(input));
    input.node_online = true;
    add_state_session(&input, "a", PHOSPHOR_SESSION_RUNNING);
    add_state_session(&input, "b", PHOSPHOR_SESSION_WAITING);
    add_state_session(&input, "c", PHOSPHOR_SESSION_ERROR);
    add_state_session(&input, "d", PHOSPHOR_SESSION_COMPLETED);
    add_state_session(&input, "e", PHOSPHOR_SESSION_IDLE);
    add_state_session(&input, "f", PHOSPHOR_SESSION_RUNNING);
    phosphor_state_resolver_update(&resolver, &input, 0, &visual);
    TEST_ASSERT_EQ_INT(4, visual.satellite_count);
    TEST_ASSERT_EQ_INT(2, visual.overflow_count);
    uint8_t slot_a = 255;
    for (int i = 0; i < 4; ++i) {
        if (strcmp(visual.satellites[i].session_id, "a") == 0) {
            slot_a = visual.satellites[i].slot;
        }
    }
    TEST_ASSERT_TRUE(slot_a != 255);
    phosphor_session_input_t tmp_session = input.sessions[0];
    input.sessions[0] = input.sessions[3];
    input.sessions[3] = tmp_session;
    phosphor_state_resolver_update(&resolver, &input, 100, &visual);
    uint8_t slot_a2 = 255;
    for (int i = 0; i < 4; ++i) {
        if (strcmp(visual.satellites[i].session_id, "a") == 0) {
            slot_a2 = visual.satellites[i].slot;
        }
    }
    TEST_ASSERT_EQ_INT(slot_a, slot_a2);
}

static void test_core_six_states(void)
{
    phosphor_demo_phase_t phases[6] = {
        PHOSPHOR_DEMO_IDLE, PHOSPHOR_DEMO_WORKING,
        PHOSPHOR_DEMO_APPROVAL, PHOSPHOR_DEMO_ERROR,
        PHOSPHOR_DEMO_COMPLETED, PHOSPHOR_DEMO_OFFLINE,
    };
    phosphor_core_metrics_t m[6];
    glyph_frame_t frames[6];
    for (int p = 0; p < 6; ++p) {
        phosphor_state_resolver_t resolver;
        phosphor_input_state_t input;
        phosphor_visual_state_t visual;
        phosphor_state_resolver_init(&resolver);
        phosphor_demo_build_input(phases[p], 1, &input);
        phosphor_state_resolver_update(&resolver, &input, 0, &visual);
        uint32_t t = phases[p] == PHOSPHOR_DEMO_COMPLETED ? 300u : 1000u;
        phosphor_state_resolver_update(&resolver, &input, t, &visual);
        phosphor_core_build_frame(&frames[p], &visual, &m[p]);
    }
    TEST_ASSERT_TRUE(m[2].approval_gap_cells > 0);
    TEST_ASSERT_EQ_INT(7, m[3].error_break_cells);
    TEST_ASSERT_EQ_INT(3, m[4].completion_wave_layers);
    TEST_ASSERT_TRUE(m[5].offline_removed_cells > 0);
    int idle_cells = 0;
    int working_cells = 0;
    for (int r = 0; r < PHOSPHOR_GRID_H; ++r) {
        for (int c = 0; c < PHOSPHOR_GRID_W; ++c) {
            idle_cells += frames[0].cells[r][c].glyph != 0;
            working_cells += frames[1].cells[r][c].glyph != 0;
        }
    }
    TEST_ASSERT_TRUE(working_cells > idle_cells);
    TEST_ASSERT_TRUE(memcmp(&frames[0], &frames[1], sizeof(frames[0])) != 0);
    TEST_ASSERT_TRUE(memcmp(&frames[1], &frames[2], sizeof(frames[1])) != 0);
    TEST_ASSERT_TRUE(memcmp(&frames[1], &frames[3], sizeof(frames[1])) != 0);
    TEST_ASSERT_TRUE(memcmp(&frames[1], &frames[4], sizeof(frames[1])) != 0);
    TEST_ASSERT_TRUE(memcmp(&frames[1], &frames[5], sizeof(frames[1])) != 0);
}

static void test_scene_nucleus_and_rasterizer(void)
{
    glyph_frame_t frame;
    phosphor_scene_build_frame(&frame);
    const glyph_cell_t *n1 = &frame.cells[PHOSPHOR_NUCLEUS_ROW]
                                        [PHOSPHOR_NUCLEUS_COL];
    const glyph_cell_t *n2 = &frame.cells[PHOSPHOR_NUCLEUS_ROW]
                                        [PHOSPHOR_NUCLEUS_COL + 2];
    TEST_ASSERT_TRUE(n1->glyph != 0);
    TEST_ASSERT_TRUE(n2->glyph != 0);
    TEST_ASSERT_EQ_INT(GLYPH_FLAG_NUCLEUS, n1->flags);
    TEST_ASSERT_EQ_INT(GLYPH_FLAG_NUCLEUS, n2->flags);
    TEST_ASSERT_EQ_INT(2, n1->scale);
    TEST_ASSERT_EQ_INT(2, n2->scale);

    uint16_t pixels[PHOSPHOR_CANVAS_W * PHOSPHOR_CANVAS_H];
    phosphor_scene_render_rgb565(pixels, PHOSPHOR_CANVAS_W,
                                 PHOSPHOR_CANVAS_H);
    TEST_ASSERT_EQ_INT(phosphor_palette_default()->background, pixels[0]);

    bool non_background = false;
    for (size_t i = 0; i < sizeof(pixels) / sizeof(pixels[0]); ++i) {
        if (pixels[i] != phosphor_palette_default()->background) {
            non_background = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(non_background);

    uint16_t small[4] = {0};
    phosphor_scene_render_rgb565(small, 1, 1);
    TEST_ASSERT_EQ_INT(phosphor_palette_default()->background, small[0]);
}

void test_phosphor_run(void)
{
    test_palette_rgb565();
    test_glyph_ramp();
    test_glyph_atlas_coverage();
    test_orbit_geometry();
    test_radial_field();
    test_satellite_layout();
    test_glyph_frame_priority();
    test_motion_and_scene();
    test_state_resolver();
    test_core_six_states();
    test_band_neighbors();
    test_scene_nucleus_and_rasterizer();
}
