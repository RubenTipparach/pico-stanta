#include "render3d.hpp"
#include <algorithm>

// Fixed-point factor (must be power of 2 for efficiency) - matching Pico3D
#define FIXED_POINT_FACTOR 1024

// Global depth buffer (full-res, 8-bit = 57.6KB)
uint8_t depth_buffer[DEPTH_WIDTH * DEPTH_HEIGHT];

// Projection constants (matching Pico3D style)
#define ZFAR 400.0f
#define ZNEAR 0.25f
#define CAMERA_FOVX 180.0f
#define CAMERA_FOVY 180.0f
#define PI 3.14159265f

// Camera state
static float camera_position[3] = {0.0f, 0.0f, 0.0f};
static float camera_pitch = 0.0f;
static float camera_yaw = 0.0f;

// Matrices (float for construction)
static float mat_camera[4][4];
static float mat_projection[4][4];

// Combined view-projection matrix (fixed-point for rendering)
static int32_t mat_vp[4][4];

// Helper functions - matching Pico3D exactly
static inline int32_t float_to_fixed(float in) {
    return (int32_t)(in * FIXED_POINT_FACTOR);
}

static void mat_mul(float mat1[4][4], float mat2[4][4], float out[4][4]) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            out[y][x] = 0;
            for (int z = 0; z < 4; z++) {
                out[y][x] += mat1[y][z] * mat2[z][x];
            }
        }
    }
}

static void mat_convert_float_fixed(float mat_in[4][4], int32_t mat_out[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mat_out[i][j] = float_to_fixed(mat_in[i][j]);
        }
    }
}

// Global matrices for SDK compatibility
Mat4 view_matrix;
Mat4 proj_matrix;
Mat4 vp_matrix;

void render3d_init() {
    // Build perspective projection matrix (Pico3D style - using atan for FOV)
    float fx = atanf((CAMERA_FOVX * PI / 180.0f) * 0.5f);
    float fy = atanf((CAMERA_FOVY * PI / 180.0f) * 0.5f);
    
    // Initialize projection matrix - exactly as Pico3D does
    mat_projection[0][0] = fx;
    mat_projection[0][1] = 0.0f;
    mat_projection[0][2] = 0.0f;
    mat_projection[0][3] = 0.0f;
    
    mat_projection[1][0] = 0.0f;
    mat_projection[1][1] = fy;
    mat_projection[1][2] = 0.0f;
    mat_projection[1][3] = 0.0f;
    
    mat_projection[2][0] = 0.0f;
    mat_projection[2][1] = 0.0f;
    mat_projection[2][2] = -((ZFAR + ZNEAR) / (ZFAR - ZNEAR));
    mat_projection[2][3] = -((2.0f * ZFAR * ZNEAR) / (ZFAR - ZNEAR));
    
    mat_projection[3][0] = 0.0f;
    mat_projection[3][1] = 0.0f;
    mat_projection[3][2] = -1.0f;
    mat_projection[3][3] = 0.0f;
    
    render3d_clear();
}

void render3d_clear() {
    // Clear half-res depth buffer to max depth (0xFF = far)
    memset(depth_buffer, 0xFF, sizeof(depth_buffer));
}

static float dot_product3(float vec1[3], float vec2[3]) {
    return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2];
}

// Camera update - matching Pico3D exactly
static void update_camera() {
    float cosPitch = cosf(camera_pitch);
    float sinPitch = sinf(camera_pitch);
    float cosYaw = cosf(camera_yaw);
    float sinYaw = sinf(camera_yaw);
    
    float xaxis[3] = { cosYaw, 0, -sinYaw };
    float yaxis[3] = { sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    float zaxis[3] = { sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };
    
    mat_camera[0][0] = xaxis[0];
    mat_camera[0][1] = xaxis[1];
    mat_camera[0][2] = xaxis[2];
    mat_camera[0][3] = -dot_product3(xaxis, camera_position);
    
    mat_camera[1][0] = yaxis[0];
    mat_camera[1][1] = yaxis[1];
    mat_camera[1][2] = yaxis[2];
    mat_camera[1][3] = -dot_product3(yaxis, camera_position);
    
    mat_camera[2][0] = zaxis[0];
    mat_camera[2][1] = zaxis[1];
    mat_camera[2][2] = zaxis[2];
    mat_camera[2][3] = -dot_product3(zaxis, camera_position);
    
    mat_camera[3][0] = 0;
    mat_camera[3][1] = 0;
    mat_camera[3][2] = 0;
    mat_camera[3][3] = 1;
}

static void render_view_projection() {
    float mat_vp_float[4][4];
    mat_mul(mat_projection, mat_camera, mat_vp_float);
    mat_convert_float_fixed(mat_vp_float, mat_vp);
}

void render3d_set_camera(const Vec3& pos, const Vec3& target) {
    camera_position[0] = pos.x;
    camera_position[1] = pos.y;
    camera_position[2] = pos.z;
    
    // Calculate yaw and pitch from target
    float dx = target.x - pos.x;
    float dy = target.y - pos.y;
    float dz = target.z - pos.z;
    
    camera_yaw = atan2f(dx, dz);
    float dist_xz = sqrtf(dx * dx + dz * dz);
    camera_pitch = atan2f(dy, dist_xz);
    
    update_camera();
    render_view_projection();
}

void render3d_third_person_camera(const Vec3& player_pos, float player_yaw) {
    float cam_dist = 8.0f;
    float cam_height = 4.0f;
    
    float cos_yaw = cosf(player_yaw);
    float sin_yaw = sinf(player_yaw);
    
    Vec3 cam_pos;
    cam_pos.x = player_pos.x - sin_yaw * cam_dist;
    cam_pos.y = player_pos.y + cam_height;
    cam_pos.z = player_pos.z - cos_yaw * cam_dist;
    
    Vec3 look_target = player_pos + Vec3(0, 1.0f, 0);
    
    render3d_set_camera(cam_pos, look_target);
}

// Project a single vertex using the fixed-point VP matrix
// Returns screen coordinates and Z value in FIXED_POINT_FACTOR range (0 to FIXED_POINT_FACTOR)
// Matches Pico3D render_triangle.cpp projection
static bool project_vertex(float wx, float wy, float wz, int32_t& sx, int32_t& sy, int32_t& sz) {
    // Convert world coords to fixed-point
    int32_t fx = float_to_fixed(wx);
    int32_t fy = float_to_fixed(wy);
    int32_t fz = float_to_fixed(wz);
    
    // Calculate W component (for perspective divide) - exactly as Pico3D
    int32_t w = ((mat_vp[3][0] * fx) + (mat_vp[3][1] * fy) +
    (mat_vp[3][2] * fz) + (mat_vp[3][3] * FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;
    
    // Behind camera check
    if (w <= 0) return false;
    
    // Calculate clip-space coordinates with perspective divide - matching Pico3D
    int32_t cx = ((mat_vp[0][0] * fx) + (mat_vp[0][1] * fy) +
    (mat_vp[0][2] * fz) + (mat_vp[0][3] * FIXED_POINT_FACTOR)) / w;
    int32_t cy = ((mat_vp[1][0] * fx) + (mat_vp[1][1] * fy) +
    (mat_vp[1][2] * fz) + (mat_vp[1][3] * FIXED_POINT_FACTOR)) / w;
    int32_t cz = ((mat_vp[2][0] * fx) + (mat_vp[2][1] * fy) +
    (mat_vp[2][2] * fz) + (mat_vp[2][3] * FIXED_POINT_FACTOR)) / w;
    
    // Z frustum culling - Pico3D expects z in range (0, FIXED_POINT_FACTOR]
    if (cz <= 0 || cz > FIXED_POINT_FACTOR) return false;
    
    // Convert to screen coordinates - exactly as Pico3D render_triangle.cpp
    sx = (cx + FIXED_POINT_FACTOR) * (SCREEN_WIDTH - 1) / FIXED_POINT_FACTOR / 2;
    sy = SCREEN_HEIGHT - ((cy + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1)) / FIXED_POINT_FACTOR / 2;
    sz = cz;  // Store Z directly in FIXED_POINT_FACTOR range
    
    return true;
}

bool render3d_project(const Vec3& world_pos, const Mat4& mvp, VertexScreen& out) {
    int32_t sx, sy, sz;
    if (!project_vertex(world_pos.x, world_pos.y, world_pos.z, sx, sy, sz)) {
        return false;
    }
    
    out.x = (int16_t)sx;
    out.y = (int16_t)sy;
    out.z = (uint16_t)sz;  // Z is already in proper range
    
    return true;
}

// Render a single triangle using edge function rasterization
// This is a direct port of Pico3D's render_rasterize.cpp approach
void render3d_triangle(const VertexScreen& v0, const VertexScreen& v1, const VertexScreen& v2) {
    // Use int32_t for all coordinates to avoid overflow in edge function calculations
    int32_t x1 = v0.x, y1 = v0.y;
    int32_t x2 = v1.x, y2 = v1.y;
    int32_t x3 = v2.x, y3 = v2.y;

    // Precalculate area of triangle for barycentric coordinates - matching Pico3D
    int32_t area = (x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1);

    // Backface culling (area <= 0 means back-facing or degenerate)
    if (area <= 0) return;

    // Bounding box - matching Pico3D style exactly
    int32_t x_large = x1;
    int32_t x_small = x1;
    int32_t y_large = y1;
    int32_t y_small = y1;

    if (x2 > x_large) x_large = x2;
    if (x3 > x_large) x_large = x3;
    if (x2 < x_small) x_small = x2;
    if (x3 < x_small) x_small = x3;

    if (y2 > y_large) y_large = y2;
    if (y3 > y_large) y_large = y3;
    if (y2 < y_small) y_small = y2;
    if (y3 < y_small) y_small = y3;

    // Clip to screen (using inclusive bounds, so max is WIDTH-1 and HEIGHT-1)
    if (x_large >= SCREEN_WIDTH) x_large = SCREEN_WIDTH - 1;
    if (x_small < 0) x_small = 0;
    if (y_large >= SCREEN_HEIGHT) y_large = SCREEN_HEIGHT - 1;
    if (y_small < 0) y_small = 0;

    // Early out if completely off screen
    if (x_large < x_small || y_large < y_small) return;
    
    // Get Z values - these should be in range (0, FIXED_POINT_FACTOR]
    // Clamp to ensure valid range for inverse calculation
    int32_t z1 = (int32_t)v0.z; if (z1 < 1) z1 = 1; if (z1 > FIXED_POINT_FACTOR) z1 = FIXED_POINT_FACTOR;
    int32_t z2 = (int32_t)v1.z; if (z2 < 1) z2 = 1; if (z2 > FIXED_POINT_FACTOR) z2 = FIXED_POINT_FACTOR;
    int32_t z3 = (int32_t)v2.z; if (z3 < 1) z3 = 1; if (z3 > FIXED_POINT_FACTOR) z3 = FIXED_POINT_FACTOR;
    
    // Inverse Z coordinates - exactly as Pico3D
    int32_t zi1 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z1;
    int32_t zi2 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z2;
    int32_t zi3 = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z3;
    
    // Rasterize using edge functions
    // Use <= to include edge pixels (bounding box is inclusive)
    for (int32_t y = y_small; y <= y_large; y++) {
        int8_t skipline = 0;

        for (int32_t x = x_small; x <= x_large; x++) {
            // Edge function tests
            int32_t edge1 = (x - x2) * (y3 - y2) - (y - y2) * (x3 - x2);
            if (edge1 < 0) {
                if (skipline == 1) break;  // Exit inner loop
                continue;
            }

            int32_t edge2 = (x - x3) * (y1 - y3) - (y - y3) * (x1 - x3);
            if (edge2 < 0) {
                if (skipline == 1) break;
                continue;
            }

            int32_t edge3 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
            if (edge3 < 0) {
                if (skipline == 1) break;
                continue;
            }

            skipline = 1;
            
            // Calculate barycentric weights - exactly as Pico3D
            int32_t w1 = (FIXED_POINT_FACTOR * edge1) / area;
            int32_t w2 = (FIXED_POINT_FACTOR * edge2) / area;
            int32_t w3 = FIXED_POINT_FACTOR - (w1 + w2);
            
            // Interpolated Z coordinate - exactly as Pico3D
            int32_t z_interp = w1 * zi1 + w2 * zi2 + w3 * zi3;
            if (z_interp <= 0) continue;
            
            int32_t z = (FIXED_POINT_FACTOR * FIXED_POINT_FACTOR * FIXED_POINT_FACTOR) / z_interp;
            
            // Depth test using full-res buffer
            // Convert Z to 8-bit: lower values = closer
            int32_t z_scaled = z * 255 / FIXED_POINT_FACTOR;
            uint8_t z8 = (uint8_t)(z_scaled > 255 ? 255 : (z_scaled < 0 ? 0 : z_scaled));

            int idx = y * DEPTH_WIDTH + x;

            if (z8 > depth_buffer[idx]) {
                continue;
            }
            depth_buffer[idx] = z8;
            
            // Interpolate color (Gouraud shading)
            int r = (int)((w1 * v0.r + w2 * v1.r + w3 * v2.r) / FIXED_POINT_FACTOR);
            int g = (int)((w1 * v0.g + w2 * v1.g + w3 * v2.g) / FIXED_POINT_FACTOR);
            int b = (int)((w1 * v0.b + w2 * v1.b + w3 * v2.b) / FIXED_POINT_FACTOR);
            
            // Clamp and draw
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            
            screen.pen = Pen(r, g, b);
            screen.pixel(Point(x, y));
        }
    }
}

// Pre-defined cube vertices (unit cube, base at y=0)
static const float cube_verts[8][3] = {
    {-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f}, {0.5f, 1.0f, -0.5f}, {-0.5f, 1.0f, -0.5f},  // Front
    {-0.5f, 0.0f,  0.5f}, {0.5f, 0.0f,  0.5f}, {0.5f, 1.0f,  0.5f}, {-0.5f, 1.0f,  0.5f}   // Back
};

// Cube face definitions (vertex indices) - CCW winding when viewed from outside
static const uint8_t cube_faces[6][4] = {
    {0, 3, 2, 1},  // Front (Z-)
    {5, 6, 7, 4},  // Back (Z+)
    {4, 7, 3, 0},  // Left (X-)
    {1, 2, 6, 5},  // Right (X+)
    {3, 7, 6, 2},  // Top (Y+)
    {4, 0, 1, 5}   // Bottom (Y-)
};

void render3d_cube(const Vec3& pos, const Vec3& size,
    uint8_t r_top, uint8_t g_top, uint8_t b_top,
    uint8_t r_side, uint8_t g_side, uint8_t b_side) {
        // Transform and project all 8 vertices
        VertexScreen sv[8];
        bool visible[8];
        
        for (int i = 0; i < 8; i++) {
            float wx = pos.x + cube_verts[i][0] * size.x;
            float wy = pos.y + cube_verts[i][1] * size.y;
            float wz = pos.z + cube_verts[i][2] * size.z;
            
            int32_t sx, sy, sz;
            visible[i] = project_vertex(wx, wy, wz, sx, sy, sz);
            
            if (visible[i]) {
                sv[i].x = (int16_t)sx;
                sv[i].y = (int16_t)sy;
                sv[i].z = (uint16_t)sz;  // Z already in FIXED_POINT_FACTOR range
            }
        }
        
        // Render each face
        for (int face = 0; face < 6; face++) {
            const uint8_t* f = cube_faces[face];
            
            // Skip if any vertex is behind camera
            if (!visible[f[0]] || !visible[f[1]] || !visible[f[2]] || !visible[f[3]]) continue;
            
            // Determine color based on face
            uint8_t r, g, b;
            if (face == 4) {  // Top
                r = r_top; g = g_top; b = b_top;
            } else if (face == 5) {  // Bottom
                r = r_side / 2; g = g_side / 2; b = b_side / 2;
            } else {
                // Side faces with simple lighting
                float shade = 1.0f;
                if (face == 0) shade = 0.7f;   // Front
                if (face == 1) shade = 0.9f;   // Back
                if (face == 2) shade = 0.6f;   // Left (darkest)
                if (face == 3) shade = 1.0f;   // Right (brightest)
                r = (uint8_t)(r_side * shade);
                g = (uint8_t)(g_side * shade);
                b = (uint8_t)(b_side * shade);
            }
            
            // Set vertex colors
            VertexScreen v0 = sv[f[0]]; v0.r = r; v0.g = g; v0.b = b;
            VertexScreen v1 = sv[f[1]]; v1.r = r; v1.g = g; v1.b = b;
            VertexScreen v2 = sv[f[2]]; v2.r = r; v2.g = g; v2.b = b;
            VertexScreen v3 = sv[f[3]]; v3.r = r; v3.g = g; v3.b = b;
            
            // Add gradient for top vertices (Gouraud effect)
            if (face != 4 && face != 5) {
                // For side faces, vertices 1 and 2 are at the top
                v1.r = std::min(255, r + 30);
                v1.g = std::min(255, g + 30);
                v1.b = std::min(255, b + 30);
                v2.r = std::min(255, r + 30);
                v2.g = std::min(255, g + 30);
                v2.b = std::min(255, b + 30);
            }
            
            // Render two triangles per face
            render3d_triangle(v0, v1, v2);
            render3d_triangle(v0, v2, v3);
        }
    }
    
    void render3d_billboard(const Vec3& world_pos, BillboardDrawFunc draw_func, float base_size) {
        int32_t sx, sy, sz;
        if (!project_vertex(world_pos.x, world_pos.y, world_pos.z, sx, sy, sz)) {
            return;
        }
        
        // Skip if off screen
        if (sx < -50 || sx >= SCREEN_WIDTH + 50 || sy < -50 || sy >= SCREEN_HEIGHT + 50) {
            return;
        }
        
        // Calculate distance for scaling
        float dx = world_pos.x - camera_position[0];
        float dy = world_pos.y - camera_position[1];
        float dz = world_pos.z - camera_position[2];
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        if (dist < 0.5f) return;
        
        float scale = base_size * 40.0f / dist;
        if (scale < 0.5f) return;
        
        // Depth for billboard - convert from FIXED_POINT_FACTOR range to 0-255
        int32_t z_scaled = sz * 255 / FIXED_POINT_FACTOR;
        uint8_t z8 = (uint8_t)(z_scaled > 255 ? 255 : z_scaled);
        
        draw_func((int)sx, (int)sy, scale, z8);
    }
    