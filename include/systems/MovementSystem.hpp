#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <entt/entt.hpp>

inline void MovementSystem(entt::registry& reg, float dt, int windowW) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag>();
    playerView.each([dt, keys](Transform& t, Velocity& v, GravityState& g) {
        constexpr float friction = 3.0f;

        if (!g.active) {
            bool moving = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                          keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
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

        if (g.isCrouching) {
            v.dx -= v.dx * friction * dt;
            v.dy -= v.dy * friction * dt;
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
                    if (keys[SDL_SCANCODE_A])
                        v.dx = -v.speed;
                    if (keys[SDL_SCANCODE_D])
                        v.dx = v.speed;
                    if (!keys[SDL_SCANCODE_A] && !keys[SDL_SCANCODE_D]) {
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
                    if (!keys[SDL_SCANCODE_W] && !keys[SDL_SCANCODE_S]) {
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

    auto tileView  = reg.view<TileTag, Transform, Collider>();
    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider, Renderable>(
        entt::exclude<DeadTag>);
    enemyView.each([&](Transform& t, Velocity& v, const Collider& c, Renderable& r) {
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

        r.flipH = v.dx > 0.0f;
    });
}
