#include "render3d.hpp"
#include "rasterizer.hpp"
#include <algorithm>

#define FIXED_POINT_FACTOR 1024
#define ZFAR 400.0f
#define ZNEAR 0.25f
#define CAMERA_FOVX 180.0f
#define CAMERA_FOVY 180.0f
#define PI 3.14159265f

uint8_t depth_buffer_a[DEPTH_WIDTH * DEPTH_HEIGHT];
uint8_t depth_buffer_b[DEPTH_WIDTH * DEPTH_HEIGHT];
uint8_t* depth_buffer_render = depth_buffer_a;   // Core 1 writes here
uint8_t* depth_buffer_display = depth_buffer_b;  // Core 0 reads here

static float camera_position[3] = {0.0f, 0.0f, 0.0f};
static float camera_pitch = 0.0f;
static float camera_yaw = 0.0f;
static float mat_camera[4][4];
static float mat_projection[4][4];
static int32_t mat_vp[4][4];

static inline int32_t float_to_fixed(float in) { return (int32_t)(in * FIXED_POINT_FACTOR); }

static void mat_mul(float mat1[4][4], float mat2[4][4], float out[4][4]) {
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        out[y][x] = 0;
        for (int z = 0; z < 4; z++) out[y][x] += mat1[y][z] * mat2[z][x];
    }
}

static void mat_convert_float_fixed(float mat_in[4][4], int32_t mat_out[4][4]) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        mat_out[i][j] = float_to_fixed(mat_in[i][j]);
}

void render3d_init() {
    float fx = atanf((CAMERA_FOVX * PI / 180.0f) * 0.5f);
    float fy = atanf((CAMERA_FOVY * PI / 180.0f) * 0.5f);
    mat_projection[0][0] = fx; mat_projection[0][1] = 0; mat_projection[0][2] = 0; mat_projection[0][3] = 0;
    mat_projection[1][0] = 0; mat_projection[1][1] = fy; mat_projection[1][2] = 0; mat_projection[1][3] = 0;
    mat_projection[2][0] = 0; mat_projection[2][1] = 0;
    mat_projection[2][2] = -((ZFAR + ZNEAR) / (ZFAR - ZNEAR));
    mat_projection[2][3] = -((2.0f * ZFAR * ZNEAR) / (ZFAR - ZNEAR));
    mat_projection[3][0] = 0; mat_projection[3][1] = 0; mat_projection[3][2] = -1.0f; mat_projection[3][3] = 0;
    render3d_clear();
}

void render3d_begin_frame() { rasterizer_begin_frame(); }
uint32_t render3d_end_frame() { return 0; }
void render3d_clear() {
    memset(depth_buffer_a, 0xFF, sizeof(depth_buffer_a));
    memset(depth_buffer_b, 0xFF, sizeof(depth_buffer_b));
}

void render3d_swap_depth_buffers() {
    uint8_t* temp = depth_buffer_render;
    depth_buffer_render = depth_buffer_display;
    depth_buffer_display = temp;
}

static float dot_product3(float v1[3], float v2[3]) { return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]; }

static void update_camera() {
    float cosPitch = cosf(camera_pitch), sinPitch = sinf(camera_pitch);
    float cosYaw = cosf(camera_yaw), sinYaw = sinf(camera_yaw);
    float xaxis[3] = { cosYaw, 0, -sinYaw };
    float yaxis[3] = { sinYaw*sinPitch, cosPitch, cosYaw*sinPitch };
    float zaxis[3] = { sinYaw*cosPitch, -sinPitch, cosPitch*cosYaw };
    mat_camera[0][0] = xaxis[0]; mat_camera[0][1] = xaxis[1]; mat_camera[0][2] = xaxis[2];
    mat_camera[0][3] = -dot_product3(xaxis, camera_position);
    mat_camera[1][0] = yaxis[0]; mat_camera[1][1] = yaxis[1]; mat_camera[1][2] = yaxis[2];
    mat_camera[1][3] = -dot_product3(yaxis, camera_position);
    mat_camera[2][0] = zaxis[0]; mat_camera[2][1] = zaxis[1]; mat_camera[2][2] = zaxis[2];
    mat_camera[2][3] = -dot_product3(zaxis, camera_position);
    mat_camera[3][0] = 0; mat_camera[3][1] = 0; mat_camera[3][2] = 0; mat_camera[3][3] = 1;
}

static void render_view_projection() {
    float mat_vp_float[4][4];
    mat_mul(mat_projection, mat_camera, mat_vp_float);
    mat_convert_float_fixed(mat_vp_float, mat_vp);
}

void render3d_third_person_camera(float px, float py, float pz, float pyaw) {
    float cam_dist = 8.0f, cam_height = 4.0f;
    camera_position[0] = px - sinf(pyaw) * cam_dist;
    camera_position[1] = py + cam_height;
    camera_position[2] = pz - cosf(pyaw) * cam_dist;
    float dx = px - camera_position[0], dy = (py + 1.0f) - camera_position[1], dz = pz - camera_position[2];
    camera_yaw = atan2f(dx, dz);
    camera_pitch = atan2f(dy, sqrtf(dx*dx + dz*dz));
    update_camera();
    render_view_projection();
}

static bool project_vertex(float wx, float wy, float wz, int32_t& sx, int32_t& sy, int32_t& sz) {
    int32_t fx = float_to_fixed(wx), fy = float_to_fixed(wy), fz = float_to_fixed(wz);
    int32_t w = ((mat_vp[3][0]*fx) + (mat_vp[3][1]*fy) + (mat_vp[3][2]*fz) + (mat_vp[3][3]*FIXED_POINT_FACTOR)) / FIXED_POINT_FACTOR;
    if (w <= 0) return false;
    int32_t cx = ((mat_vp[0][0]*fx) + (mat_vp[0][1]*fy) + (mat_vp[0][2]*fz) + (mat_vp[0][3]*FIXED_POINT_FACTOR)) / w;
    int32_t cy = ((mat_vp[1][0]*fx) + (mat_vp[1][1]*fy) + (mat_vp[1][2]*fz) + (mat_vp[1][3]*FIXED_POINT_FACTOR)) / w;
    int32_t cz = ((mat_vp[2][0]*fx) + (mat_vp[2][1]*fy) + (mat_vp[2][2]*fz) + (mat_vp[2][3]*FIXED_POINT_FACTOR)) / w;
    if (cz <= 0 || cz > FIXED_POINT_FACTOR) return false;
    sx = (cx + FIXED_POINT_FACTOR) * (SCREEN_WIDTH - 1) / FIXED_POINT_FACTOR / 2;
    sy = SCREEN_HEIGHT - ((cy + FIXED_POINT_FACTOR) * (SCREEN_HEIGHT - 1)) / FIXED_POINT_FACTOR / 2;
    sz = cz;
    return true;
}

void render3d_triangle(const VertexScreen& v0, const VertexScreen& v1, const VertexScreen& v2) {
    RasterTriangle tri;
    tri.x1 = v0.x; tri.y1 = v0.y; tri.x2 = v1.x; tri.y2 = v1.y; tri.x3 = v2.x; tri.y3 = v2.y;
    tri.z1 = v0.z; tri.z2 = v1.z; tri.z3 = v2.z;
    tri.r1 = v0.r; tri.g1 = v0.g; tri.b1 = v0.b;
    tri.r2 = v1.r; tri.g2 = v1.g; tri.b2 = v1.b;
    tri.r3 = v2.r; tri.g3 = v2.g; tri.b3 = v2.b;
    tri.pad = 0;
    rasterizer_submit_triangle(tri);
}

static const float cube_verts[8][3] = {
    {-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f}, {0.5f, 1.0f, -0.5f}, {-0.5f, 1.0f, -0.5f},
    {-0.5f, 0.0f,  0.5f}, {0.5f, 0.0f,  0.5f}, {0.5f, 1.0f,  0.5f}, {-0.5f, 1.0f,  0.5f}
};
static const uint8_t cube_faces[6][4] = {{0,3,2,1},{5,6,7,4},{4,7,3,0},{1,2,6,5},{3,7,6,2},{4,0,1,5}};

void render3d_cube(float px, float py, float pz, float szx, float szy, float szz,
                   uint8_t r_top, uint8_t g_top, uint8_t b_top, uint8_t r_side, uint8_t g_side, uint8_t b_side) {
    VertexScreen sv[8]; bool visible[8];
    for (int i = 0; i < 8; i++) {
        float wx = px + cube_verts[i][0]*szx, wy = py + cube_verts[i][1]*szy, wz = pz + cube_verts[i][2]*szz;
        int32_t scx, scy, scz;
        visible[i] = project_vertex(wx, wy, wz, scx, scy, scz);
        if (visible[i]) { sv[i].x = scx; sv[i].y = scy; sv[i].z = scz; }
    }
    for (int face = 0; face < 6; face++) {
        const uint8_t* f = cube_faces[face];
        if (!visible[f[0]] || !visible[f[1]] || !visible[f[2]] || !visible[f[3]]) continue;
        uint8_t r, g, b;
        if (face == 4) { r = r_top; g = g_top; b = b_top; }
        else if (face == 5) { r = r_side/2; g = g_side/2; b = b_side/2; }
        else { float sh = (face==0)?0.7f:(face==1)?0.9f:(face==2)?0.6f:1.0f; r=(uint8_t)(r_side*sh); g=(uint8_t)(g_side*sh); b=(uint8_t)(b_side*sh); }
        VertexScreen v0=sv[f[0]], v1=sv[f[1]], v2=sv[f[2]], v3=sv[f[3]];
        v0.r=r; v0.g=g; v0.b=b; v1.r=r; v1.g=g; v1.b=b; v2.r=r; v2.g=g; v2.b=b; v3.r=r; v3.g=g; v3.b=b;
        if (face != 4 && face != 5) {
            v1.r=std::min(255,r+30); v1.g=std::min(255,g+30); v1.b=std::min(255,b+30);
            v2.r=std::min(255,r+30); v2.g=std::min(255,g+30); v2.b=std::min(255,b+30);
        }
        render3d_triangle(v0, v1, v2); render3d_triangle(v0, v2, v3);
    }
}

void render3d_billboard(float wx, float wy, float wz, BillboardDrawFunc draw_func, float base_size, color_t* fb) {
    int32_t sx, sy, sz;
    if (!project_vertex(wx, wy, wz, sx, sy, sz)) return;
    if (sx < -50 || sx >= SCREEN_WIDTH+50 || sy < -50 || sy >= SCREEN_HEIGHT+50) return;
    float dx = wx - camera_position[0], dy = wy - camera_position[1], dz = wz - camera_position[2];
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);
    if (dist < 0.5f) return;
    float scale = base_size * 40.0f / dist;
    if (scale < 0.5f) return;
    int32_t z_scaled = sz * 255 / FIXED_POINT_FACTOR;
    uint8_t z8 = (z_scaled > 255) ? 255 : (uint8_t)z_scaled;
    draw_func((int)sx, (int)sy, scale, z8, fb);
}
