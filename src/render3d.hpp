#pragma once
#include "picosystem.hpp"
#include <cstring>
#include <cmath>
#include <cstdint>

using namespace picosystem;

// Screen constants (120x120 for performance)
constexpr int SCREEN_WIDTH = 120;
constexpr int SCREEN_HEIGHT = 120;
constexpr int DEPTH_WIDTH = 120;
constexpr int DEPTH_HEIGHT = 120;

// Screen-space vertex with Gouraud shading data
struct VertexScreen {
    int16_t x, y;
    uint16_t z;
    uint8_t r, g, b;
    uint8_t _pad;
};

// Double-buffered depth buffers (8-bit each = 14.4KB x 2)
// Core 1 writes to one while Core 0 reads from the other
extern uint8_t depth_buffer_a[DEPTH_WIDTH * DEPTH_HEIGHT];
extern uint8_t depth_buffer_b[DEPTH_WIDTH * DEPTH_HEIGHT];
extern uint8_t* depth_buffer_render;  // Core 1 writes here
extern uint8_t* depth_buffer_display; // Core 0 reads here for billboards

// Initialize the 3D renderer
void render3d_init();

// Begin/end frame
void render3d_begin_frame();
uint32_t render3d_end_frame();

// Clear depth buffer
void render3d_clear();

// Set camera position
void render3d_third_person_camera(float player_x, float player_y, float player_z, float player_yaw);

// Render a triangle
void render3d_triangle(const VertexScreen& v0, const VertexScreen& v1, const VertexScreen& v2);

// Render a cube
void render3d_cube(float px, float py, float pz, float sx, float sy, float sz,
                   uint8_t r_top, uint8_t g_top, uint8_t b_top,
                   uint8_t r_side, uint8_t g_side, uint8_t b_side);

// Render a billboard
// draw_func receives: x, y, scale, depth, and a framebuffer pointer (nullptr = use pen/pixel)
typedef void (*BillboardDrawFunc)(int x, int y, float scale, uint8_t depth, color_t* fb);
void render3d_billboard(float wx, float wy, float wz, BillboardDrawFunc draw_func, float base_size, color_t* fb = nullptr);

// Depth test helper (uses display buffer - safe for Core 0 billboards)
inline bool depth_test(int x, int y, uint8_t z) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return false;
    int idx = y * DEPTH_WIDTH + x;
    if (z < depth_buffer_display[idx]) {
        depth_buffer_display[idx] = z;
        return true;
    }
    return false;
}

// Swap depth buffers (call after render_sync swaps framebuffers)
void render3d_swap_depth_buffers();

// RGB to 4-bit color (picosystem format: ggggbbbbaaaarrrr)
inline color_t rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r >> 4) | (0xF << 4) | ((b >> 4) << 8) | ((g >> 4) << 12);
}
