#include "game.hpp"
#include "render3d.hpp"
#include "city.hpp"
#include <cstdlib>
#include <cmath>

using namespace blit;

// Screen dimensions (120x120 for performance)
const int SCREEN_W = 120;
const int SCREEN_H = 120;

// Player state (3D world coordinates)
struct Player3D {
    float x, y, z;       // World position (y is up)
    float vx, vz;        // Velocity (no vertical velocity in this version)
    float yaw;           // Facing direction (rotation around Y axis)
    bool facing_right;
    int anim_frame;
    uint32_t anim_timer;
} player;

// Game state
int score = 0;

// Physics constants
const float MOVE_SPEED = 0.01f;
const float FRICTION = 0.95f;
const float TURN_SPEED = 0.03f;
const float PLAYER_RADIUS = 0.5f;

// Chicken billboard drawing function
static void draw_chicken_billboard(int cx, int cy, float scale, uint8_t depth);

void init() {
    set_screen_mode(ScreenMode::lores);
    
    // Initialize 3D renderer
    render3d_init();
    
    // Initialize city
    city_init(12345);
    
    // Initialize player at starting position
    player.x = 5.0f;
    player.y = 0.0f;
    player.z = 0.0f;
    player.vx = 0.0f;
    player.vz = 0.0f;
    player.yaw = 0.0f;
    player.facing_right = true;
    player.anim_frame = 0;
    player.anim_timer = 0;
    
    score = 0;
}

void render(uint32_t time) {
    // Clear screen with sky gradient
    for (int y = 0; y < SCREEN_H; y++) {
        // Sky gradient from dark blue at top to lighter at horizon
        int r = 40 + y / 6;
        int g = 60 + y / 4;
        int b = 120 + y / 3;
        screen.pen = Pen(r, g, b);
        screen.rectangle(Rect(0, y, SCREEN_W, 1));
    }
    
    // Clear depth buffer
    render3d_clear();
    
    // Set up third-person camera
    render3d_third_person_camera(Vec3(player.x, player.y, player.z), player.yaw);
    
    // Draw ground plane
    // Simple ground as a large flat quad at y=0
    {
        // Calculate player's grid cell
        int player_grid_x = (int)floorf(player.x / 4.0f);
        int player_grid_z = (int)floorf(player.z / 4.0f);
        
        // Draw ground as multiple colored tiles for visual interest
        for (int gx = -5; gx <= 5; gx++) {
            for (int gz = -5; gz <= 5; gz++) {
                int grid_x = player_grid_x + gx;
                int grid_z = player_grid_z + gz;
                
                float tile_x = grid_x * 4.0f + 2.0f;  // Center of tile
                float tile_z = grid_z * 4.0f + 2.0f;
                
                // Checkerboard pattern based on integer grid coords
                bool dark = ((grid_x + grid_z) & 1) == 0;
                uint8_t r = dark ? 60 : 80;
                uint8_t g = dark ? 60 : 80;
                uint8_t b = dark ? 70 : 90;
                
                // Simple flat cube for ground tile
                render3d_cube(Vec3(tile_x, -0.5f, tile_z), Vec3(4.0f, 0.5f, 4.0f),
                r, g, b, r, g, b);
            }
        }
    }
    
    // Render city buildings
    city_render();
    
    // Render gems
    city_render_gems(time);
    
    // Render player as billboard
    Vec3 player_pos(player.x, player.y + 0.5f, player.z);  // Offset up from ground
    render3d_billboard(player_pos, draw_chicken_billboard, 1.5f);
    
    // Draw UI overlay
    screen.pen = Pen(0, 0, 0, 180);
    screen.rectangle(Rect(0, 0, SCREEN_W, 18));
    
    screen.pen = Pen(255, 255, 255);
    screen.text("Score: " + std::to_string(score), minimal_font, Point(5, 5));
    
    int distance = (int)(player.x);
    screen.text("Dist: " + std::to_string(distance), minimal_font, Point(150, 5));
    
    // Controls hint at bottom
    screen.pen = Pen(0, 0, 0, 120);
    screen.rectangle(Rect(0, SCREEN_H - 12, SCREEN_W, 12));
    screen.pen = Pen(200, 200, 200);
    screen.text("Arrows:Move  Collect gems!", minimal_font, Point(30, SCREEN_H - 10));
}

void update(uint32_t time) {
    // Store previous position for collision resolution
    float prev_x = player.x;
    float prev_z = player.z;
    
    // Player input - tank controls style
    // Left/Right rotates, Up/Down moves forward/backward
    if (buttons & Button::DPAD_LEFT) {
        player.yaw += TURN_SPEED;
    }
    if (buttons & Button::DPAD_RIGHT) {
        player.yaw -= TURN_SPEED;
    }
    
    // Calculate forward direction based on yaw
    // sinf/cosf give us direction vector in XZ plane
    float forward_x = sinf(player.yaw);
    float forward_z = cosf(player.yaw);
    
    if (buttons & Button::DPAD_UP) {
        player.vx -= forward_x * MOVE_SPEED;
        player.vz -= forward_z * MOVE_SPEED;
    }
    if (buttons & Button::DPAD_DOWN) {
        player.vx += forward_x * MOVE_SPEED * 0.5f;  // Slower backwards
        player.vz += forward_z * MOVE_SPEED * 0.5f;
    }
    
    // Apply friction
    player.vx *= FRICTION;
    player.vz *= FRICTION;
    
    // Apply velocity
    player.x += player.vx;
    player.z += player.vz;
    
    // Keep player in the "street" area (between buildings)
    if (player.z < -2.5f) player.z = -2.5f;
    if (player.z > 2.5f) player.z = 2.5f;
    
    // Check collision with buildings
    if (city_check_collision(player.x, player.z, PLAYER_RADIUS)) {
        // Revert to previous position
        player.x = prev_x;
        player.z = prev_z;
        player.vx = 0;
        player.vz = 0;
    }
    
    // Prevent going backwards past start
    if (player.x < 1.0f) {
        player.x = 1.0f;
        player.vx = 0;
    }
    
    // Update animation
    float speed = sqrtf(player.vx * player.vx + player.vz * player.vz);
    if (speed > 0.01f) {
        player.anim_timer += (uint32_t)(speed * 1000);
        if (player.anim_timer > 200) {
            player.anim_timer = 0;
            player.anim_frame = 1 - player.anim_frame;
        }
        // Update facing direction based on movement
        player.facing_right = player.vx > 0 || (player.vx == 0 && forward_x > 0);
    }
    
    // Update city chunks based on player position
    city_update_chunks(player.x);
    
    // Collect gems
    int points = city_collect_gem(player.x, player.z, 1.5f);
    score += points;
}

// Chicken sprite drawing for billboard
static void draw_chicken_billboard(int cx, int cy, float scale, uint8_t depth) {
    if (scale < 0.2f) return;  // Too small to draw
    
    // Scale the 8x8 chicken sprite
    int size = (int)(8 * scale);
    if (size < 4) size = 4;
    
    // Colors
    Pen white(255, 255, 255);
    Pen orange(255, 165, 0);
    Pen red(255, 50, 50);
    Pen black(0, 0, 0);
    Pen yellow(255, 220, 100);
    
    // Simple scaled chicken - draw each "pixel" as a scaled rectangle
    auto put_scaled_pixel = [&](int px, int py, Pen color) {
        int flip = player.facing_right ? 1 : -1;
        int fx = player.facing_right ? px : (7 - px);
        
        int x1 = cx + (int)((fx - 4) * scale);
        int y1 = cy + (int)((py - 4) * scale);
        int x2 = x1 + (int)(scale) + 1;
        int y2 = y1 + (int)(scale) + 1;
        
        for (int y = y1; y < y2; y++) {
            for (int x = x1; x < x2; x++) {
                if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
                    if (depth_test(x, y, depth)) {
                        screen.pen = color;
                        screen.pixel(Point(x, y));
                    }
                }
            }
        }
    };
    
    // Draw chicken sprite (same design as 2D version)
    // Head (red comb)
    put_scaled_pixel(2, 0, red);
    put_scaled_pixel(3, 0, red);
    
    // Head body
    put_scaled_pixel(1, 1, white);
    put_scaled_pixel(2, 1, white);
    put_scaled_pixel(3, 1, white);
    put_scaled_pixel(4, 1, white);
    
    // Eye and beak
    put_scaled_pixel(1, 2, white);
    put_scaled_pixel(2, 2, black);
    put_scaled_pixel(3, 2, white);
    put_scaled_pixel(0, 2, orange);
    
    // Neck/wattle
    put_scaled_pixel(1, 3, red);
    put_scaled_pixel(2, 3, white);
    put_scaled_pixel(3, 3, white);
    put_scaled_pixel(4, 3, yellow);
    
    // Body
    put_scaled_pixel(2, 4, white);
    put_scaled_pixel(3, 4, white);
    put_scaled_pixel(4, 4, white);
    put_scaled_pixel(5, 4, white);
    put_scaled_pixel(6, 4, yellow);
    
    put_scaled_pixel(2, 5, white);
    put_scaled_pixel(3, 5, white);
    put_scaled_pixel(4, 5, white);
    put_scaled_pixel(5, 5, white);
    put_scaled_pixel(6, 5, yellow);
    put_scaled_pixel(7, 5, yellow);
    
    // Legs (animated)
    if (player.anim_frame == 0) {
        put_scaled_pixel(3, 6, orange);
        put_scaled_pixel(4, 6, orange);
        put_scaled_pixel(3, 7, orange);
        put_scaled_pixel(4, 7, orange);
    } else {
        put_scaled_pixel(2, 6, orange);
        put_scaled_pixel(5, 6, orange);
        put_scaled_pixel(2, 7, orange);
        put_scaled_pixel(5, 7, orange);
    }
}
