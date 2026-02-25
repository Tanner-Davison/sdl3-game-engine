// Part of ECS
#pragma once
#include <SDL3/SDL.h>
#include <vector>

inline constexpr float PLAYER_HIT_DAMAGE    = 15.0f;
inline constexpr float PLAYER_INVINCIBILITY = 1.5f;
inline constexpr float PLAYER_MAX_HEALTH    = 100.0f;
inline constexpr int   PLAYER_SPRITE_WIDTH  = 66;
inline constexpr int   PLAYER_SPRITE_HEIGHT = 92;
inline constexpr int   SLIME_SPRITE_WIDTH   = 36;
inline constexpr int   SLIME_SPRITE_HEIGHT  = 26;
inline constexpr float GRAVITY_DURATION     = 60.0f;  // 1 minute
inline constexpr float GRAVITY_FORCE        = 600.0f; // pixels/sec^2
inline constexpr float JUMP_FORCE           = 800.0f; // pixels/sec
inline constexpr float MAX_FALL_SPEED       = 900.0f;
// Position and size in the world
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
};

// Movement direction and speed
struct Velocity {
    float dx    = 0.0f;
    float dy    = 0.0f;
    float speed = 150.0f;
};

// Animation state
struct AnimationState {
    int   currentFrame = 0;
    int   totalFrames  = 0;
    float timer        = 0.0f;
    float fps          = 12.0f;
    bool  looping      = true;
};

// What to draw
struct Renderable {
    SDL_Surface*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
};

// Tag: marks the player entity - no data needed
struct PlayerTag {};

// Health
struct Health {
    float current = PLAYER_MAX_HEALTH;
    float max     = PLAYER_MAX_HEALTH;
};

// Collision Collider
struct Collider {
    int w = 0;
    int h = 0;
};

struct InvincibilityTimer {
    float remaining    = 0.0f;
    float duration     = PLAYER_INVINCIBILITY;
    bool  isInvincible = false;
};

enum class GravityDir { DOWN, UP, LEFT, RIGHT };

struct GravityState {
    bool       active      = false;
    float      timer       = 0.0f;
    bool       isGrounded  = false;  // true when touching the gravity wall
    float      velocity    = 0.0f;  // velocity along gravity axis
    bool       jumpHeld    = false;
    GravityDir direction   = GravityDir::DOWN;
};
