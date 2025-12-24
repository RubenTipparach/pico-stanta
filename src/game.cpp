#include "picosystem.hpp"
#include "pico/multicore.h"
#include "hardware/structs/bus_ctrl.h"
#include "render3d.hpp"
#include "rasterizer.hpp"
#include "city.hpp"
#include <cstdlib>
#include <cmath>
#include <cstring>

using namespace picosystem;

const int SCREEN_W = 120;
const int SCREEN_H = 120;

struct Player3D {
    float x, y, z;
    float vx, vz;
    float yaw;
    bool facing_right;
    int anim_frame;
    uint32_t anim_timer;
} player;

int score = 0;

const float MOVE_SPEED = 0.01f;
const float FRICTION = 0.95f;
const float TURN_SPEED = 0.03f;
const float PLAYER_RADIUS = 0.5f;

static color_t framebuffer[SCREEN_W * SCREEN_H] __attribute__ ((aligned (4))) = { };
static buffer_t *FRAMEBUFFER = nullptr;

static volatile uint32_t core1_time = 0;
static volatile bool core1_initialized = false;

// Performance tracking
static uint32_t core0_time_us = 0;
static uint32_t core1_time_us = 0;
static uint32_t last_triangle_count = 0;
static const uint32_t TARGET_FRAME_US = 16667; // 60 FPS = 16.667ms

static void draw_chicken_billboard(int cx, int cy, float scale, uint8_t depth, color_t* fb);

static void core1_entry() {
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;
    core1_initialized = true;

    while (1) {
        uint32_t num_triangles = multicore_fifo_pop_blocking();
        uint32_t start_time = time_us();
        rasterizer_render_to_buffer(num_triangles, FRAMEBUFFER->data);
        uint32_t end_time = time_us();
        core1_time = end_time - start_time;
        multicore_fifo_push_blocking(core1_time);
    }
}

static void render_sync() {
    // Wait for Core 1 to finish the previous frame (blocking)
    uint32_t result_time = multicore_fifo_pop_blocking();
    core1_time_us = result_time;

    // Swap framebuffers - display what Core 1 just rendered
    buffer_t *TEMP_FB = SCREEN;
    SCREEN = FRAMEBUFFER;
    FRAMEBUFFER = TEMP_FB;
    target(SCREEN);

    // Swap depth buffers - Core 0 can now read from what Core 1 just wrote
    render3d_swap_depth_buffers();

    // Get triangle count BEFORE swapping (swap resets the count!)
    last_triangle_count = rasterizer_get_triangle_count();

    // Swap triangle lists and send new work to Core 1
    rasterizer_swap_lists();
    multicore_fifo_push_blocking(last_triangle_count);
}

void init() {
    FRAMEBUFFER = buffer(SCREEN_W, SCREEN_H, framebuffer);
    multicore_launch_core1(core1_entry);
    while (!core1_initialized) { tight_loop_contents(); }

    // Initial handshake: send 0 triangles so Core 1 starts its loop
    multicore_fifo_push_blocking(0);
    // Wait for Core 1 to finish rendering nothing
    multicore_fifo_pop_blocking();
    // Send another 0 so Core 1 is ready for the first real frame
    multicore_fifo_push_blocking(0);

    render3d_init();
    city_init(12345);

    player.x = 5.0f; player.y = 0.0f; player.z = 0.0f;
    player.vx = 0.0f; player.vz = 0.0f;
    player.yaw = 0.0f;
    player.facing_right = true;
    player.anim_frame = 0;
    player.anim_timer = 0;
    score = 0;
}

void update(uint32_t tick) {
    float prev_x = player.x;
    float prev_z = player.z;

    if (button(LEFT)) player.yaw += TURN_SPEED;
    if (button(RIGHT)) player.yaw -= TURN_SPEED;

    float forward_x = sinf(player.yaw);
    float forward_z = cosf(player.yaw);

    if (button(UP)) {
        player.vx -= forward_x * MOVE_SPEED;
        player.vz -= forward_z * MOVE_SPEED;
    }
    if (button(DOWN)) {
        player.vx += forward_x * MOVE_SPEED * 0.5f;
        player.vz += forward_z * MOVE_SPEED * 0.5f;
    }

    player.vx *= FRICTION;
    player.vz *= FRICTION;
    player.x += player.vx;
    player.z += player.vz;

    if (player.z < -2.5f) player.z = -2.5f;
    if (player.z > 2.5f) player.z = 2.5f;

    if (city_check_collision(player.x, player.z, PLAYER_RADIUS)) {
        player.x = prev_x;
        player.z = prev_z;
        player.vx = 0;
        player.vz = 0;
    }

    if (player.x < 1.0f) { player.x = 1.0f; player.vx = 0; }

    float speed = sqrtf(player.vx * player.vx + player.vz * player.vz);
    if (speed > 0.01f) {
        player.anim_timer += (uint32_t)(speed * 1000);
        if (player.anim_timer > 200) {
            player.anim_timer = 0;
            player.anim_frame = 1 - player.anim_frame;
        }
        player.facing_right = player.vx > 0 || (player.vx == 0 && forward_x > 0);
    }

    city_update_chunks(player.x);
    int points = city_collect_gem(player.x, player.z, 1.5f);
    score += points;
}

void draw(uint32_t tick) {
    uint32_t frame_start = time_us();
    render_sync();
    render3d_begin_frame();

    // Sky gradient is now drawn by Core 1 in rasterizer_render_to_buffer

    render3d_third_person_camera(player.x, player.y, player.z, player.yaw);

    {
        int player_grid_x = (int)floorf(player.x / 4.0f);
        int player_grid_z = (int)floorf(player.z / 4.0f);
        for (int gx = -5; gx <= 5; gx++) {
            for (int gz = -5; gz <= 5; gz++) {
                int grid_x = player_grid_x + gx;
                int grid_z = player_grid_z + gz;
                float tile_x = grid_x * 4.0f + 2.0f;
                float tile_z = grid_z * 4.0f + 2.0f;
                bool dark = ((grid_x + grid_z) & 1) == 0;
                uint8_t cr = dark ? 60 : 80;
                uint8_t cg = dark ? 60 : 80;
                uint8_t cb = dark ? 70 : 90;
                render3d_cube(tile_x, -0.5f, tile_z, 4.0f, 0.5f, 4.0f, cr, cg, cb, cr, cg, cb);
            }
        }
    }

    city_render();
    // Render gems to SCREEN (which contains Core 1's rendered geometry)
    city_render_gems(time(), SCREEN->data);
    // TODO: Re-enable chicken billboard once colors are fixed
    // render3d_billboard(player.x, player.y + 0.5f, player.z, draw_chicken_billboard, 1.5f, SCREEN->data);
    render3d_end_frame();

    // Measure Core 0 time (scene building)
    core0_time_us = time_us() - frame_start;

    // Calculate CPU percentages
    int cpu0_pct = (int)(core0_time_us * 100 / TARGET_FRAME_US);
    int cpu1_pct = (int)(core1_time_us * 100 / TARGET_FRAME_US);

    // Top bar - Score
    pen(0, 0, 0); alpha(11);
    frect(0, 0, SCREEN_W, 12);
    alpha();
    pen(15, 15, 15);
    text("Score: " + str((int32_t)score), 2, 2);

    // Bottom bar - Performance stats
    pen(0, 0, 0); alpha(10);
    frect(0, SCREEN_H - 18, SCREEN_W, 18);
    alpha();

    // CPU usage (color coded: green < 50%, yellow 50-80%, red > 80%)
    int max_cpu = cpu0_pct > cpu1_pct ? cpu0_pct : cpu1_pct;
    if (max_cpu < 50) pen(4, 15, 4);
    else if (max_cpu < 80) pen(15, 15, 4);
    else pen(15, 4, 4);

    text("C0:" + str((int32_t)cpu0_pct) + "% C1:" + str((int32_t)cpu1_pct) + "%", 2, SCREEN_H - 16);

    pen(10, 10, 12);
    text("Tri:" + str((int32_t)last_triangle_count), 2, SCREEN_H - 8);
}

static void draw_chicken_billboard(int cx, int cy, float scale, uint8_t depth, color_t* fb) {
    if (scale < 0.2f) return;

    auto put_scaled_pixel = [&](int px, int py, uint8_t r, uint8_t g, uint8_t b) {
        int fx = player.facing_right ? px : (7 - px);
        int x1 = cx + (int)((fx - 4) * scale);
        int y1 = cy + (int)((py - 4) * scale);
        int x2 = x1 + (int)(scale) + 1;
        int y2 = y1 + (int)(scale) + 1;
        for (int yy = y1; yy < y2; yy++) {
            for (int xx = x1; xx < x2; xx++) {
                if (xx >= 0 && xx < SCREEN_W && yy >= 0 && yy < SCREEN_H) {
                    if (depth_test(xx, yy, depth)) {
                        if (fb) {
                            fb[yy * SCREEN_W + xx] = rgb_to_color(r * 17, g * 17, b * 17);
                        } else {
                            pen(r, g, b);
                            pixel(xx, yy);
                        }
                    }
                }
            }
        }
    };

    put_scaled_pixel(2, 0, 15, 3, 3); put_scaled_pixel(3, 0, 15, 3, 3);
    put_scaled_pixel(1, 1, 15, 15, 15); put_scaled_pixel(2, 1, 15, 15, 15);
    put_scaled_pixel(3, 1, 15, 15, 15); put_scaled_pixel(4, 1, 15, 15, 15);
    put_scaled_pixel(1, 2, 15, 15, 15); put_scaled_pixel(2, 2, 0, 0, 0);
    put_scaled_pixel(3, 2, 15, 15, 15); put_scaled_pixel(0, 2, 15, 10, 0);
    put_scaled_pixel(1, 3, 15, 3, 3); put_scaled_pixel(2, 3, 15, 15, 15);
    put_scaled_pixel(3, 3, 15, 15, 15); put_scaled_pixel(4, 3, 15, 13, 6);
    put_scaled_pixel(2, 4, 15, 15, 15); put_scaled_pixel(3, 4, 15, 15, 15);
    put_scaled_pixel(4, 4, 15, 15, 15); put_scaled_pixel(5, 4, 15, 15, 15);
    put_scaled_pixel(6, 4, 15, 13, 6);
    put_scaled_pixel(2, 5, 15, 15, 15); put_scaled_pixel(3, 5, 15, 15, 15);
    put_scaled_pixel(4, 5, 15, 15, 15); put_scaled_pixel(5, 5, 15, 15, 15);
    put_scaled_pixel(6, 5, 15, 13, 6); put_scaled_pixel(7, 5, 15, 13, 6);

    if (player.anim_frame == 0) {
        put_scaled_pixel(3, 6, 15, 10, 0); put_scaled_pixel(4, 6, 15, 10, 0);
        put_scaled_pixel(3, 7, 15, 10, 0); put_scaled_pixel(4, 7, 15, 10, 0);
    } else {
        put_scaled_pixel(2, 6, 15, 10, 0); put_scaled_pixel(5, 6, 15, 10, 0);
        put_scaled_pixel(2, 7, 15, 10, 0); put_scaled_pixel(5, 7, 15, 10, 0);
    }
}
