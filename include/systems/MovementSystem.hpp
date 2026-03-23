#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <entt/entt.hpp>

inline void MovementSystem(entt::registry& reg, float dt, int windowW) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag, ClimbState>();
    playerView.each([dt, keys](Transform& t, Velocity& v, GravityState& g, const ClimbState& climb) {
        constexpr float friction = 3.0f;

        if (!g.active) {
            // While climbing or parked at top, LadderSystem owns v.dy entirely.
            // Only apply horizontal movement here — never touch v.dy.
            if (climb.climbing || climb.atTop) {
                // Drive horizontal velocity directly from keyboard state each
                // frame so strafe feels as responsive as normal movement.
                // CLIMB_STRAFE_SPEED is the max speed while on the ladder.
                bool leftHeld  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
                bool rightHeld = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
                if (leftHeld && !rightHeld) {
                    v.dx = -CLIMB_STRAFE_SPEED;
                } else if (rightHeld && !leftHeld) {
                    v.dx =  CLIMB_STRAFE_SPEED;
                } else {
                    // No horizontal input — apply friction to glide to a stop
                    v.dx -= v.dx * friction * dt;
                    if (std::abs(v.dx) < 0.5f) v.dx = 0.0f;
                }
                t.x += v.dx * dt;
                // t.y intentionally NOT touched here — LadderSystem owns it.
                return;
            }
            // Free-float mode (gravity off for other reasons, e.g. wall-run punishment)
            bool moving = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                          keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D] ||
                          keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN] ||
                          keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT];
            if (!moving) {
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f)
                    v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f)
                    v.dy = 0.0f;
            }
            t.x += v.dx * dt;
            t.y += v.dy * dt;
            return;
        }

        // Effective speed: base * sprint multiplier when Shift is held
        const float effSpeed = v.speed * (g.sprinting ? SPRINT_MULTIPLIER : 1.0f);

        if (g.isCrouching) {
            // Crouching: no new input, just decelerate with higher friction
            // for a smooth slide-to-stop instead of an instant halt.
            v.dx -= v.dx * CROUCH_FRICTION * dt;
            v.dy -= v.dy * CROUCH_FRICTION * dt;
            if (std::abs(v.dx) < 0.5f)
                v.dx = 0.0f;
            if (std::abs(v.dy) < 0.5f)
                v.dy = 0.0f;
            t.x += v.dx * dt;
            t.y += v.dy * dt;
        } else {
            v.dx = v.dy = 0.0f;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
                        v.dx = -effSpeed;
                    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
                        v.dx = effSpeed;
                    if (!keys[SDL_SCANCODE_A] && !keys[SDL_SCANCODE_D]
                     && !keys[SDL_SCANCODE_LEFT] && !keys[SDL_SCANCODE_RIGHT]) {
                        v.dx -= v.dx * friction * dt;
                        if (std::abs(v.dx) < 0.5f)
                            v.dx = 0.0f;
                    }
                    t.x += v.dx * dt;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
                        v.dy = -effSpeed;
                    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
                        v.dy = effSpeed;
                    if (!keys[SDL_SCANCODE_W] && !keys[SDL_SCANCODE_S]
                     && !keys[SDL_SCANCODE_UP] && !keys[SDL_SCANCODE_DOWN]) {
                        v.dy -= v.dy * friction * dt;
                        if (std::abs(v.dy) < 0.5f)
                            v.dy = 0.0f;
                    }
                    t.y += v.dy * dt;
                    break;
                }
            }
        }

        if (!g.isGrounded) {
            g.velocity = std::min(g.velocity + GRAVITY_FORCE * dt, MAX_FALL_SPEED);
        } else if (g.direction == GravityDir::DOWN) {
            // Ground-stick: always apply a small constant downward nudge while
            // grounded, regardless of whether the player is moving or standing
            // still. This guarantees oTop > 0 in CollisionSystem's floor snap
            // every frame, so isGrounded is reliably re-set to true even when
            // floating-point precision would otherwise produce oTop == 0 exactly
            // and cause a spurious one-frame airborne state (jump anim flash).
            // CollisionSystem resets g.velocity to 0 on any floor snap, so this
            // tiny nudge never accumulates into real downward movement.
            g.velocity = std::min(g.velocity + SLOPE_STICK_VELOCITY, MAX_FALL_SPEED);
        }

        if (g.jumpHeld && !g.isGrounded && g.velocity < 0.0f) {
            g.velocity -= JUMP_FORCE * 0.5f * dt;
        }

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
    });

    // Enemies only turn around on solid tiles — exclude props (visual-only, no collision)
    // and action tiles (slash-triggered; they may have TileTag while still present).
    auto tileView  = reg.view<TileTag, Transform, Collider>(entt::exclude<PropTag, ActionTag>);
    // Find player position for enemy AI targeting
    float playerX = 0.0f, playerW = 0.0f;
    {
        auto pv = reg.view<PlayerTag, Transform, Collider>();
        pv.each([&](const Transform& pt, const Collider& pc) {
            playerX = pt.x;
            playerW = (float)pc.w;
        });
    }

    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider, Renderable>(
        entt::exclude<DeadTag>);
    enemyView.each([&](entt::entity ent, Transform& t, Velocity& v, const Collider& c, Renderable& r) {
        // Enemy AI: if within aggro range, walk toward the player.
        // Skip chasing while stunned (HitFlash active = just got hit).
        bool stunned = false;
        if (auto* hf = reg.try_get<HitFlash>(ent))
            stunned = hf->timer > 0.0f;

        if (!stunned) {
            constexpr float AGGRO_RANGE = 300.0f;
            float enemyCX  = t.x + c.w * 0.5f;
            float playerCX = playerX + playerW * 0.5f;
            float dist     = std::abs(enemyCX - playerCX);
            if (dist < AGGRO_RANGE && dist > 4.0f) {
                float dir = (playerCX > enemyCX) ? 1.0f : -1.0f;
                v.dx = dir * v.speed;
            }
        } else {
            // Stunned: decelerate to a stop so knockback isn't fought
            v.dx *= 0.85f;
            if (std::abs(v.dx) < 1.0f) v.dx = 0.0f;
        }

        t.x += v.dx * dt;

        if (t.x < 0.0f) {
            t.x  = 0.0f;
            v.dx = std::abs(v.dx);
        } else if (t.x + c.w > windowW) {
            t.x  = static_cast<float>(windowW - c.w);
            v.dx = -std::abs(v.dx);
        }

        tileView.each([&](const Transform& tt, const Collider& tc) {
            if (t.y >= tt.y + tc.h || t.y + c.h <= tt.y)
                return;

            float oLeft  = (t.x + c.w) - tt.x;
            float oRight = (tt.x + tc.w) - t.x;
            if (oLeft <= 0.0f || oRight <= 0.0f)
                return;

            if (oLeft < oRight) {
                t.x  = tt.x - c.w;
                v.dx = -std::abs(v.dx);
            } else {
                t.x  = tt.x + tc.w;
                v.dx = std::abs(v.dx);
            }
        });

        // Custom enemy sprites face right by default (FaceRightTag);
        // legacy slime sprites face left by default.
        if (reg.all_of<FaceRightTag>(ent))
            r.flipH = v.dx < 0.0f;   // flip when moving left
        else
            r.flipH = v.dx > 0.0f;   // flip when moving right (legacy slime)
    });
}
