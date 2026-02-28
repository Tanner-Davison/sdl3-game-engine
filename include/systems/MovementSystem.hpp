#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <entt/entt.hpp>

inline void MovementSystem(entt::registry& reg, float dt, int windowW) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag>();
    playerView.each([dt, keys](Transform& t, Velocity& v, GravityState& g) {
        if (!g.active) {
            // FREE MODE — friction on key release
            bool anyKeyHeld = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                              keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
            if (!anyKeyHeld) {
                constexpr float friction = 3.0f;
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f)
                    v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f)
                    v.dy = 0.0f;
            }
            t.x += v.dx * dt;
            t.y += v.dy * dt;
        } else {
            // GRAVITY MODE — movement perpendicular to gravity axis still works
            constexpr float friction = 3.0f;

            if (g.isCrouching) {
                // Crouching — bleed off existing velocity with friction, no new input
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f)
                    v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f)
                    v.dy = 0.0f;
                t.x += v.dx * dt;
                t.y += v.dy * dt;
            } else {
                v.dx = 0.0f;
                v.dy = 0.0f;

                bool horizKey = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
                bool vertKey  = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S];

                switch (g.direction) {
                    case GravityDir::DOWN:
                    case GravityDir::UP: {
                        if (keys[SDL_SCANCODE_A])
                            v.dx = -v.speed;
                        if (keys[SDL_SCANCODE_D])
                            v.dx = v.speed;
                        if (!horizKey) {
                            v.dx -= v.dx * friction * dt;
                            if (std::abs(v.dx) < 0.5f)
                                v.dx = 0.0f;
                        }
                        t.x += v.dx * dt;
                        break;
                    }
                    case GravityDir::LEFT:
                    case GravityDir::RIGHT: {
                        if (keys[SDL_SCANCODE_W])
                            v.dy = -v.speed;
                        if (keys[SDL_SCANCODE_S])
                            v.dy = v.speed;
                        if (!vertKey) {
                            v.dy -= v.dy * friction * dt;
                            if (std::abs(v.dy) < 0.5f)
                                v.dy = 0.0f;
                        }
                        t.y += v.dy * dt;
                        break;
                    }
                }
            }

            // Apply gravity along the gravity axis
            if (!g.isGrounded) {
                g.velocity += GRAVITY_FORCE * dt;
                if (g.velocity > MAX_FALL_SPEED)
                    g.velocity = MAX_FALL_SPEED;
            }

            // Hold space boosts away from wall
            if (g.jumpHeld && !g.isGrounded && g.velocity < 0.0f) {
                g.velocity -= JUMP_FORCE * 0.5f * dt;
            }

            // Apply gravity velocity in the correct direction
            switch (g.direction) {
                case GravityDir::DOWN:
                    t.y += g.velocity * dt;
                    break;
                case GravityDir::UP:
                    t.y -= g.velocity * dt;
                    break;
                case GravityDir::LEFT:
                    t.x -= g.velocity * dt;
                    break;
                case GravityDir::RIGHT:
                    t.x += g.velocity * dt;
                    break;
            }

            g.timer += dt;
        }
    });

    // Enemies bounce left and right within the window and off placed tiles,
    // flipping to face their movement direction.
    auto tileView  = reg.view<TileTag, Transform, Collider>();
    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider, Renderable>(entt::exclude<DeadTag>);
    enemyView.each([&](Transform& t, Velocity& v, const Collider& c, Renderable& r) {
        t.x += v.dx * dt;

        // Window-edge bouncing
        if (t.x < 0.0f) {
            t.x  = 0.0f;
            v.dx = std::abs(v.dx);
        } else if (t.x + c.w > windowW) {
            t.x  = static_cast<float>(windowW - c.w);
            v.dx = -std::abs(v.dx);
        }

        // Tile-wall bouncing — check AABB overlap against every placed tile.
        // We only care about horizontal contact (the enemy walks on a flat Y plane),
        // so we push the enemy out horizontally and flip dx.
        tileView.each([&](const Transform& tt, const Collider& tc) {
            // Vertical overlap — enemy must share some Y range with the tile
            bool vertOverlap = t.y < tt.y + tc.h && t.y + c.h > tt.y;
            if (!vertOverlap) return;

            // Horizontal overlap
            float eRight  = t.x + c.w;
            float eLeft   = t.x;
            float tRight  = tt.x + tc.w;
            float tLeft   = tt.x;
            if (eRight <= tLeft || eLeft >= tRight) return;

            // Push out whichever side has less penetration and flip velocity
            float penLeft  = eRight - tLeft;  // penetration from enemy's right into tile's left
            float penRight = tRight - eLeft;   // penetration from tile's right into enemy's left

            if (penLeft < penRight) {
                // Enemy came from the left — push back left, bounce right
                t.x  = tLeft - c.w;
                v.dx = -std::abs(v.dx);
            } else {
                // Enemy came from the right — push back right, bounce left
                t.x  = tRight;
                v.dx = std::abs(v.dx);
            }
        });

        // Flip sprite to face the direction of travel
        r.flipH = v.dx > 0.0f;
    });
}
