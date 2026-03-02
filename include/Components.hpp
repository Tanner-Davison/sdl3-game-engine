// Part of ECS
#pragma once
#include <SDL3/SDL.h>
#include <vector>

inline constexpr int   PLAYER_STAND_WIDTH   = 32;
inline constexpr int   PLAYER_STAND_HEIGHT  = 60;
inline constexpr int   PLAYER_DUCK_WIDTH    = 44;
inline constexpr int   PLAYER_DUCK_HEIGHT   = 32;
inline constexpr float PLAYER_HIT_DAMAGE    = 15.0f;
inline constexpr float PLAYER_INVINCIBILITY = 1.5f;
inline constexpr float PLAYER_MAX_HEALTH    = 100.0f;
inline constexpr int   PLAYER_SPRITE_WIDTH  = 72;
inline constexpr int   PLAYER_SPRITE_HEIGHT = 97;
inline constexpr int   SLIME_SPRITE_WIDTH   = 36;
inline constexpr int   SLIME_SPRITE_HEIGHT  = 26;
inline constexpr float GRAVITY_DURATION     = 5.0f;   // 5 seconds
inline constexpr float GRAVITY_FORCE        = 800.0f; // pixels/sec^2
inline constexpr float JUMP_FORCE           = 450.0f; // pixels/sec
inline constexpr float MAX_FALL_SPEED       = 1000.0f;
inline constexpr float PLAYER_SPEED         = 300.0f;
inline constexpr int   GRAVITYSLUGSCOUNT    = 4;
inline constexpr int   COIN_COUNT           = 8;
inline constexpr int   COIN_SIZE            = 40;
// Position and size in the world
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
};

// Movement direction and speed
struct Velocity {
    float dx    = 0.0f;
    float dy    = 0.0f;
    float speed = PLAYER_SPEED;
};

enum class AnimationID { IDLE, WALK, JUMP, HURT, DUCK, FRONT, NONE };

// Animation state
struct AnimationState {
    int         currentFrame = 0;
    int         totalFrames  = 0;
    float       timer        = 0.0f;
    float       fps          = 12.0f;
    bool        looping      = true;
    AnimationID currentAnim  = AnimationID::NONE;
};

// What to draw
struct Renderable {
    SDL_Surface*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
};

// Tag: marks the player entity - no data needed
struct PlayerTag {};

// Tag: marks an enemy entity - no data needed
struct EnemyTag {};

// Tag: marks a collectible coin entity
struct CoinTag {};

// Tag: marks a stomped/dead enemy — no longer harmful, acts as a platform
struct DeadTag {};

// Tag: marks a solid tile/prop — blocks player movement
struct TileTag {};

// Render offset — draws the sprite offset from Transform position
// Used to center large sprites over their collision box
struct RenderOffset {
    int x = 0;
    int y = 0;
};

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

// Holds all animation frame sets and their source sheets for an entity.
// sheet pointers are non-owning — the SpriteSheet objects must outlive this.
struct AnimationSet {
    std::vector<SDL_Rect> idle;
    SDL_Surface*          idleSheet = nullptr;
    std::vector<SDL_Rect> walk;
    SDL_Surface*          walkSheet = nullptr;
    std::vector<SDL_Rect> jump;
    SDL_Surface*          jumpSheet = nullptr;
    std::vector<SDL_Rect> hurt;
    SDL_Surface*          hurtSheet = nullptr;
    std::vector<SDL_Rect> duck;
    SDL_Surface*          duckSheet = nullptr;
    std::vector<SDL_Rect> front;
    SDL_Surface*          frontSheet = nullptr;
};

enum class GravityDir { DOWN, UP, LEFT, RIGHT };

// Per-frame flip cache for RenderSystem.
// Stores one pre-flipped SDL_Surface* per animation frame, built lazily on
// first use and reused every subsequent frame. Invalidated when the animation
// set changes (detected by frame count mismatch).
struct FlipCache {
    std::vector<SDL_Surface*> frames; // indexed by AnimationState::currentFrame

    FlipCache() = default;

    // Non-copyable
    FlipCache(const FlipCache&)            = delete;
    FlipCache& operator=(const FlipCache&) = delete;

    // Movable
    FlipCache(FlipCache&& o) noexcept : frames(std::move(o.frames)) {}
    FlipCache& operator=(FlipCache&& o) noexcept {
        if (this != &o) {
            for (auto* s : frames)
                if (s)
                    SDL_DestroySurface(s);
            frames = std::move(o.frames);
        }
        return *this;
    }

    ~FlipCache() {
        for (auto* s : frames)
            if (s)
                SDL_DestroySurface(s);
    }
};

struct GravityState {
    bool       active          = true;
    float      timer           = 0.0f;
    bool       isGrounded      = true;
    float      velocity        = 0.0f;
    bool       jumpHeld        = false;
    bool       isCrouching     = false;
    GravityDir direction       = GravityDir::DOWN;
    float      punishmentTimer = 0.0f; // counts down after a hit; gravity locked off until 0
};
