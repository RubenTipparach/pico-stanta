#include "city.hpp"
#include <cmath>
#include <algorithm>

using namespace picosystem;

// Global arrays
Building buildings[MAX_BUILDINGS];
Gem3D gems_3d[MAX_GEMS_3D];
int active_building_count = 0;

// City state
uint32_t city_seed = 12345;
int city_chunk_left = 0;
int city_chunk_right = 0;

// Building color palettes
static const uint8_t building_colors[][3] = {
    {180, 100, 100},  // Red-ish brick
    {100, 130, 180},  // Blue-ish
    {150, 150, 120},  // Beige
    {120, 160, 120},  // Green-ish
    {180, 150, 100},  // Orange-ish
    {140, 140, 160},  // Purple-ish
};

static const uint8_t roof_colors[][3] = {
    {120, 60, 60},    // Dark red
    {60, 80, 120},    // Dark blue
    {100, 100, 80},   // Dark beige
    {80, 110, 80},    // Dark green
    {130, 100, 60},   // Dark orange
    {100, 100, 120},  // Dark purple
};

uint32_t city_random(uint32_t& seed) {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

void city_init(uint32_t seed) {
    city_seed = seed;

    for (int i = 0; i < MAX_BUILDINGS; i++) {
        buildings[i].active = false;
    }

    for (int i = 0; i < MAX_GEMS_3D; i++) {
        gems_3d[i].active = false;
    }

    active_building_count = 0;
    city_chunk_left = -1;
    city_chunk_right = 2;

    for (int c = city_chunk_left; c <= city_chunk_right; c++) {
        city_generate_chunk(c);
    }
}

static int find_free_building_slot() {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (!buildings[i].active) return i;
    }
    return -1;
}

static int find_free_gem_slot() {
    for (int i = 0; i < MAX_GEMS_3D; i++) {
        if (!gems_3d[i].active) return i;
    }
    return -1;
}

void city_generate_chunk(int chunk_id) {
    uint32_t chunk_seed = city_seed + chunk_id * 7919;

    float chunk_start_x = chunk_id * CITY_CHUNK_WIDTH * TILE_SIZE_3D;

    for (int tx = 0; tx < CITY_CHUNK_WIDTH; tx++) {
        if (city_random(chunk_seed) % 4 == 0) continue;

        float world_x = chunk_start_x + tx * TILE_SIZE_3D;

        // Buildings on left side (negative Z)
        if (city_random(chunk_seed) % 3 != 0) {
            int slot = find_free_building_slot();
            if (slot >= 0) {
                Building& b = buildings[slot];
                b.x = world_x;
                b.z = -4.0f - (city_random(chunk_seed) % 3);
                b.width = 1.5f + (city_random(chunk_seed) % 100) / 100.0f;
                b.depth = 1.5f + (city_random(chunk_seed) % 100) / 100.0f;
                b.height = 2.0f + (city_random(chunk_seed) % 8);

                int color_idx = city_random(chunk_seed) % 6;
                b.r_wall = building_colors[color_idx][0];
                b.g_wall = building_colors[color_idx][1];
                b.b_wall = building_colors[color_idx][2];
                b.r_roof = roof_colors[color_idx][0];
                b.g_roof = roof_colors[color_idx][1];
                b.b_roof = roof_colors[color_idx][2];

                b.active = true;
                b.chunk_id = chunk_id;
                active_building_count++;
            }
        }

        // Buildings on right side (positive Z)
        if (city_random(chunk_seed) % 3 != 0) {
            int slot = find_free_building_slot();
            if (slot >= 0) {
                Building& b = buildings[slot];
                b.x = world_x;
                b.z = 4.0f + (city_random(chunk_seed) % 3);
                b.width = 1.5f + (city_random(chunk_seed) % 100) / 100.0f;
                b.depth = 1.5f + (city_random(chunk_seed) % 100) / 100.0f;
                b.height = 2.0f + (city_random(chunk_seed) % 8);

                int color_idx = city_random(chunk_seed) % 6;
                b.r_wall = building_colors[color_idx][0];
                b.g_wall = building_colors[color_idx][1];
                b.b_wall = building_colors[color_idx][2];
                b.r_roof = roof_colors[color_idx][0];
                b.g_roof = roof_colors[color_idx][1];
                b.b_roof = roof_colors[color_idx][2];

                b.active = true;
                b.chunk_id = chunk_id;
                active_building_count++;
            }
        }

        // Spawn gems on the street
        if (city_random(chunk_seed) % 5 == 0) {
            int slot = find_free_gem_slot();
            if (slot >= 0) {
                Gem3D& g = gems_3d[slot];
                g.x = world_x + (city_random(chunk_seed) % 100) / 50.0f - 1.0f;
                g.y = 0.5f;
                g.z = (city_random(chunk_seed) % 100) / 50.0f - 1.0f;
                g.type = city_random(chunk_seed) % 3;
                g.collected = false;
                g.active = true;
                g.chunk_id = chunk_id;
            }
        }
    }
}

void city_remove_chunk(int chunk_id) {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].active && buildings[i].chunk_id == chunk_id) {
            buildings[i].active = false;
            active_building_count--;
        }
    }

    for (int i = 0; i < MAX_GEMS_3D; i++) {
        if (gems_3d[i].active && gems_3d[i].chunk_id == chunk_id) {
            gems_3d[i].active = false;
        }
    }
}

void city_update_chunks(float camera_x) {
    int camera_chunk = (int)(camera_x / (CITY_CHUNK_WIDTH * TILE_SIZE_3D));
    int desired_left = camera_chunk - 1;
    int desired_right = camera_chunk + 2;

    while (city_chunk_left < desired_left) {
        city_remove_chunk(city_chunk_left);
        city_chunk_left++;
    }

    while (city_chunk_right > desired_right) {
        city_remove_chunk(city_chunk_right);
        city_chunk_right--;
    }

    while (city_chunk_left > desired_left) {
        city_chunk_left--;
        city_generate_chunk(city_chunk_left);
    }

    while (city_chunk_right < desired_right) {
        city_chunk_right++;
        city_generate_chunk(city_chunk_right);
    }
}

void city_render() {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (!buildings[i].active) continue;

        const Building& b = buildings[i];

        render3d_cube(b.x, 0, b.z, b.width, b.height, b.depth,
                      b.r_roof, b.g_roof, b.b_roof,
                      b.r_wall, b.g_wall, b.b_wall);
    }
}

// Gem billboard drawing function
static void draw_gem_3d(int cx, int cy, float scale, uint8_t depth, uint8_t type, uint32_t time, color_t* fb) {
    static const uint8_t gem_colors[3][3] = {
        {255, 50, 50},   // Red
        {50, 255, 50},   // Green
        {50, 150, 255}   // Blue
    };

    uint8_t r = gem_colors[type][0];
    uint8_t g = gem_colors[type][1];
    uint8_t b = gem_colors[type][2];

    int bob = (int)(sinf(time / 200.0f) * 2 * scale);
    cy += bob;

    int size = (int)(3 * scale);
    if (size < 1) size = 1;

    for (int dy = -size; dy <= size; dy++) {
        int width = size - abs(dy);
        for (int dx = -width; dx <= width; dx++) {
            int px = cx + dx;
            int py = cy + dy;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                if (depth_test(px, py, depth)) {
                    uint8_t rr = r, gg = g, bb = b;
                    if (abs(dx) <= 1 && dy < 0) {
                        rr = std::min(255, r + 50);
                        gg = std::min(255, g + 50);
                        bb = std::min(255, b + 50);
                    }
                    if (fb) {
                        fb[py * SCREEN_WIDTH + px] = rgb_to_color(rr, gg, bb);
                    } else {
                        pen(rr >> 4, gg >> 4, bb >> 4);
                        pixel(px, py);
                    }
                }
            }
        }
    }
}

static uint32_t gem_render_time = 0;
static uint8_t current_gem_type = 0;
static color_t* gem_framebuffer = nullptr;

static void gem_draw_callback(int x, int y, float scale, uint8_t depth, color_t* fb) {
    draw_gem_3d(x, y, scale, depth, current_gem_type, gem_render_time, fb);
}

void city_render_gems(uint32_t time, color_t* fb) {
    gem_render_time = time;
    gem_framebuffer = fb;

    for (int i = 0; i < MAX_GEMS_3D; i++) {
        if (!gems_3d[i].active || gems_3d[i].collected) continue;

        const Gem3D& g = gems_3d[i];
        current_gem_type = g.type;

        render3d_billboard(g.x, g.y, g.z, gem_draw_callback, 1.0f, fb);
    }
}

bool city_check_collision(float x, float z, float radius) {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (!buildings[i].active) continue;

        const Building& b = buildings[i];

        float half_w = b.width / 2 + radius;
        float half_d = b.depth / 2 + radius;

        float dx = x - b.x;
        float dz = z - b.z;

        if (fabsf(dx) < half_w && fabsf(dz) < half_d) {
            return true;
        }
    }
    return false;
}

int city_collect_gem(float player_x, float player_z, float collect_radius) {
    int points = 0;

    for (int i = 0; i < MAX_GEMS_3D; i++) {
        if (!gems_3d[i].active || gems_3d[i].collected) continue;

        Gem3D& g = gems_3d[i];

        float dx = player_x - g.x;
        float dz = player_z - g.z;
        float dist_sq = dx * dx + dz * dz;

        if (dist_sq < collect_radius * collect_radius) {
            g.collected = true;
            points += (g.type + 1) * 10;
        }
    }

    return points;
}
