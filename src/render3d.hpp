#pragma once
#include "32blit.hpp"
#include <cstring>
#include <cmath>

using namespace blit;

// Screen constants (120x120 for performance)
constexpr int SCREEN_WIDTH = 120;
constexpr int SCREEN_HEIGHT = 120;
constexpr int DEPTH_WIDTH = 120;
constexpr int DEPTH_HEIGHT = 120;

// Projection constants
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;
constexpr float FOV = 60.0f * 3.14159265f / 180.0f;

// Screen-space vertex with Gouraud shading data
struct VertexScreen {
    int16_t x, y;      // Screen coordinates
    uint16_t z;        // Depth (0 = near, 65535 = far)
    uint8_t r, g, b;   // Vertex color for Gouraud shading
    uint8_t _pad;      // Padding for alignment
};

// 3D vertex with position and color
struct Vertex3D {
    Vec3 pos;
    uint8_t r, g, b;
};

// Triangle indices
struct Triangle {
    uint8_t v0, v1, v2;
};

// Simple mesh structure
struct Mesh3D {
    Vertex3D* vertices;
    Triangle* triangles;
    uint8_t vertex_count;
    uint8_t triangle_count;
};

// Camera state
struct Camera3D {
    Vec3 position;
    Vec3 target;       // Look-at point
    float yaw;         // Rotation around Y axis
};

// Global depth buffer (full resolution, 8-bit = 57.6KB)
extern uint8_t depth_buffer[DEPTH_WIDTH * DEPTH_HEIGHT];

// Global matrices
extern Mat4 view_matrix;
extern Mat4 proj_matrix;
extern Mat4 vp_matrix;  // View * Projection

// Initialize the 3D renderer
void render3d_init();

// Clear depth buffer and optionally the screen
void render3d_clear();

// Set camera position and target
void render3d_set_camera(const Vec3& pos, const Vec3& target);

// Build third-person camera from player position
void render3d_third_person_camera(const Vec3& player_pos, float player_yaw);

// Transform a 3D point to screen space
// Returns false if the point is behind the camera
bool render3d_project(const Vec3& world_pos, const Mat4& mvp, VertexScreen& out);

// Render a single triangle with Gouraud shading
void render3d_triangle(const VertexScreen& v0, const VertexScreen& v1, const VertexScreen& v2);

// Render a mesh with given model matrix
void render3d_mesh(const Mesh3D& mesh, const Mat4& model);

// Render a colored cube at position with given size and colors
void render3d_cube(const Vec3& pos, const Vec3& size,
                   uint8_t r_top, uint8_t g_top, uint8_t b_top,
                   uint8_t r_side, uint8_t g_side, uint8_t b_side);

// Render a billboard sprite at world position
// sprite_func is called with (screen_x, screen_y, scale, depth) to draw the sprite
typedef void (*BillboardDrawFunc)(int x, int y, float scale, uint8_t depth);
void render3d_billboard(const Vec3& world_pos, BillboardDrawFunc draw_func, float base_size);

// Depth test helper (inline for performance) - full resolution
inline bool depth_test(int x, int y, uint8_t z) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return false;
    int idx = y * DEPTH_WIDTH + x;
    if (z < depth_buffer[idx]) {
        depth_buffer[idx] = z;
        return true;
    }
    return false;
}

// Depth test without write (for reading depth)
inline uint8_t depth_read(int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return 0xFF;
    return depth_buffer[y * DEPTH_WIDTH + x];
}

// Pack RGB to RGB565
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
