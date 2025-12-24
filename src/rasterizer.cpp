#include "rasterizer.hpp"
#include "render3d.hpp"
#include <cstring>

using namespace picosystem;

// Fixed-point factor (must match render3d.cpp)
#define FIXED_POINT_FACTOR 1024

// Double-buffered triangle lists
static RasterTriangle triangle_list1[MAX_TRIANGLES];
static RasterTriangle triangle_list2[MAX_TRIANGLES];

// Pointers for double buffering
// Core 0 fills "next", Core 1 reads "current"
static RasterTriangle* triangle_list_current = triangle_list1;
static RasterTriangle* triangle_list_next = triangle_list2;

// Triangle counts for each list
static volatile uint32_t triangle_count_current = 0;
static uint32_t triangle_count_next = 0;

// Forward declaration
static void rasterize_single_triangle(const RasterTriangle& tri, color_t* buffer);

void rasterizer_init() {
    triangle_count_current = 0;
    triangle_count_next = 0;
}

bool rasterizer_submit_triangle(const RasterTriangle& tri) {
    if (triangle_count_next >= MAX_TRIANGLES) {
        return false;  // List is full
    }
    triangle_list_next[triangle_count_next++] = tri;
    return true;
}

uint32_t rasterizer_get_triangle_count() {
    return triangle_count_next;
}

void rasterizer_begin_frame() {
    triangle_count_next = 0;
}

uint32_t rasterizer_end_frame() {
    // Single-threaded fallback: rasterize synchronously to SCREEN
    memset(depth_buffer_render, 0xFF, RASTER_SCREEN_WIDTH * RASTER_SCREEN_HEIGHT);

    for (uint32_t i = 0; i < triangle_count_next; i++) {
        rasterize_single_triangle(triangle_list_next[i], nullptr);
    }

    triangle_count_next = 0;
    return 0;
}

bool rasterizer_is_busy() {
    return false;
}

// === Multicore API ===

void rasterizer_render_to_buffer(uint32_t count, color_t* buffer) {
    // Clear depth buffer (Core 1 uses depth_buffer_render)
    memset(depth_buffer_render, 0xFF, RASTER_SCREEN_WIDTH * RASTER_SCREEN_HEIGHT);

    // Clear color buffer with sky gradient
    for (int y = 0; y < RASTER_SCREEN_HEIGHT; y++) {
        // Sky gradient (same as in game.cpp but using rgb_to_color)
        int r = 40 + y / 6;
        int g = 60 + y / 4;
        int b = 120 + y / 3;
        color_t sky_color = rgb_to_color(r, g, b);
        color_t* row = buffer + y * RASTER_SCREEN_WIDTH;
        for (int x = 0; x < RASTER_SCREEN_WIDTH; x++) {
            row[x] = sky_color;
        }
    }

    // Rasterize all triangles from the "current" list
    for (uint32_t i = 0; i < count; i++) {
        rasterize_single_triangle(triangle_list_current[i], buffer);
    }
}

void rasterizer_swap_lists() {
    // Swap the triangle list pointers
    RasterTriangle* temp = triangle_list_current;
    triangle_list_current = triangle_list_next;
    triangle_list_next = temp;
    
    // Transfer the count and reset next
    triangle_count_current = triangle_count_next;
    triangle_count_next = 0;
}

// Rasterize a single triangle
static void rasterize_single_triangle(const RasterTriangle& tri, color_t* buffer) {
    int32_t x1 = tri.x1, y1 = tri.y1;
    int32_t x2 = tri.x2, y2 = tri.y2;
    int32_t x3 = tri.x3, y3 = tri.y3;

    // Calculate area for barycentric coordinates
    int32_t area = (x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1);

    // Backface culling
    if (area <= 0) return;

    // Bounding box
    int32_t x_large = x1, x_small = x1;
    int32_t y_large = y1, y_small = y1;

    if (x2 > x_large) x_large = x2;
    if (x3 > x_large) x_large = x3;
    if (x2 < x_small) x_small = x2;
    if (x3 < x_small) x_small = x3;

    if (y2 > y_large) y_large = y2;
    if (y3 > y_large) y_large = y3;
    if (y2 < y_small) y_small = y2;
    if (y3 < y_small) y_small = y3;

    // Clip to screen
    if (x_large >= RASTER_SCREEN_WIDTH) x_large = RASTER_SCREEN_WIDTH - 1;
    if (x_small < 0) x_small = 0;
    if (y_large >= RASTER_SCREEN_HEIGHT) y_large = RASTER_SCREEN_HEIGHT - 1;
    if (y_small < 0) y_small = 0;

    if (x_large < x_small || y_large < y_small) return;

    // Z values
    int32_t z1 = tri.z1; if (z1 < 1) z1 = 1; if (z1 > FIXED_POINT_FACTOR) z1 = FIXED_POINT_FACTOR;
    int32_t z2 = tri.z2; if (z2 < 1) z2 = 1; if (z2 > FIXED_POINT_FACTOR) z2 = FIXED_POINT_FACTOR;
    int32_t z3 = tri.z3; if (z3 < 1) z3 = 1; if (z3 > FIXED_POINT_FACTOR) z3 = FIXED_POINT_FACTOR;

    // Inverse Z for perspective-correct interpolation
    int32_t zi1 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z1;
    int32_t zi2 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z2;
    int32_t zi3 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z3;

    // Rasterize
    for (int32_t y = y_small; y <= y_large; y++) {
        int8_t skipline = 0;

        for (int32_t x = x_small; x <= x_large; x++) {
            int32_t edge1 = (x - x2) * (y3 - y2) - (y - y2) * (x3 - x2);
            if (edge1 < 0) { if (skipline == 1) break; continue; }

            int32_t edge2 = (x - x3) * (y1 - y3) - (y - y3) * (x1 - x3);
            if (edge2 < 0) { if (skipline == 1) break; continue; }

            int32_t edge3 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
            if (edge3 < 0) { if (skipline == 1) break; continue; }

            skipline = 1;

            // Barycentric weights
            int32_t w1 = (FIXED_POINT_FACTOR * edge1) / area;
            int32_t w2 = (FIXED_POINT_FACTOR * edge2) / area;
            int32_t w3 = FIXED_POINT_FACTOR - (w1 + w2);

            // Interpolate Z
            int32_t z_interp = w1 * zi1 + w2 * zi2 + w3 * zi3;
            if (z_interp <= 0) continue;

            int32_t z = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z_interp;

            // Depth test (8-bit)
            int32_t z_scaled = z * 255 / FIXED_POINT_FACTOR;
            uint8_t z8 = (uint8_t)(z_scaled > 255 ? 255 : (z_scaled < 0 ? 0 : z_scaled));

            int idx = y * RASTER_SCREEN_WIDTH + x;

            if (z8 > depth_buffer_render[idx]) continue;
            depth_buffer_render[idx] = z8;

            // Interpolate color (Gouraud shading)
            int r = (int)((w1 * tri.r1 + w2 * tri.r2 + w3 * tri.r3) / FIXED_POINT_FACTOR);
            int g = (int)((w1 * tri.g1 + w2 * tri.g2 + w3 * tri.g3) / FIXED_POINT_FACTOR);
            int b = (int)((w1 * tri.b1 + w2 * tri.b2 + w3 * tri.b3) / FIXED_POINT_FACTOR);

            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;

            if (buffer) {
                // Multicore path: write directly to provided framebuffer
                buffer[idx] = rgb_to_color(r, g, b);
            } else {
                // Single-threaded path: use picosystem pen/pixel
                pen(r >> 4, g >> 4, b >> 4);
                pixel(x, y);
            }
        }
    }
}
