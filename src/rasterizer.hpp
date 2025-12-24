#pragma once

#include "picosystem.hpp"
#include <cstdint>

using namespace picosystem;

// Maximum triangles per frame (matches Pico3D)
#define MAX_TRIANGLES 1500

// Screen dimensions (must match render3d.hpp)
#define RASTER_SCREEN_WIDTH 120
#define RASTER_SCREEN_HEIGHT 120

// Compact triangle structure for rasterization (28 bytes)
struct RasterTriangle {
    int16_t x1, y1;           // Vertex 1 screen coords
    int16_t x2, y2;           // Vertex 2 screen coords
    int16_t x3, y3;           // Vertex 3 screen coords
    uint16_t z1, z2, z3;      // Depth values (in FIXED_POINT range)
    uint8_t r1, g1, b1;       // Vertex 1 color
    uint8_t r2, g2, b2;       // Vertex 2 color
    uint8_t r3, g3, b3;       // Vertex 3 color
    uint8_t pad;              // Padding for alignment
};

// Initialize the rasterizer (call once at startup)
void rasterizer_init();

// Submit a triangle to the current frame's triangle list
// Returns false if the list is full
bool rasterizer_submit_triangle(const RasterTriangle& tri);

// Get current triangle count in the frame being built
uint32_t rasterizer_get_triangle_count();

// Begin a new frame (resets triangle count for next list)
void rasterizer_begin_frame();

// End the current frame (single-threaded: rasterizes synchronously)
// Returns the rasterization time in microseconds
uint32_t rasterizer_end_frame();

// Check if rasterizer is busy (always false in single-threaded mode)
bool rasterizer_is_busy();

// === Multicore API (called from game.cpp) ===

// Render triangles directly to a framebuffer (called by Core 1)
// count: number of triangles to render from the "current" list
// buffer: pointer to the framebuffer (color_t array)
void rasterizer_render_to_buffer(uint32_t count, color_t* buffer);

// Swap the "current" and "next" triangle lists
// Called after Core 1 finishes rendering
void rasterizer_swap_lists();
