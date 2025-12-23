#pragma once
#include "render3d.hpp"

// City generation constants
constexpr int CITY_CHUNK_WIDTH = 10;   // Tiles per chunk
constexpr int MAX_BUILDINGS = 32;      // Maximum buildings in memory
constexpr float TILE_SIZE_3D = 2.0f;   // World units per tile

// Building structure
struct Building {
    float x, z;              // World position
    float width, depth;      // Size in world units
    float height;            // Height in world units
    uint8_t r_roof, g_roof, b_roof;    // Roof color
    uint8_t r_wall, g_wall, b_wall;    // Wall color
    bool active;             // Is this building slot in use?
    int chunk_id;            // Which chunk owns this building
};

// Gem structure for 3D
struct Gem3D {
    float x, y, z;           // World position
    uint8_t type;            // Color type (0=red, 1=green, 2=blue)
    bool collected;
    bool active;
    int chunk_id;
};

// Maximum gems
constexpr int MAX_GEMS_3D = 50;

// Global building and gem arrays
extern Building buildings[MAX_BUILDINGS];
extern Gem3D gems_3d[MAX_GEMS_3D];
extern int active_building_count;

// City generation seed
extern uint32_t city_seed;

// Current loaded chunk range
extern int city_chunk_left;
extern int city_chunk_right;

// Initialize city system
void city_init(uint32_t seed);

// Generate buildings for a chunk
void city_generate_chunk(int chunk_id);

// Remove buildings from a chunk
void city_remove_chunk(int chunk_id);

// Update loaded chunks based on camera position
void city_update_chunks(float camera_x);

// Render all visible buildings
void city_render();

// Render all visible gems
void city_render_gems(uint32_t time);

// Check collision with buildings at a world position
bool city_check_collision(float x, float z, float radius);

// Collect gem if player is close enough, returns points scored
int city_collect_gem(float player_x, float player_z, float collect_radius);

// Simple deterministic random for city generation
uint32_t city_random(uint32_t& seed);
