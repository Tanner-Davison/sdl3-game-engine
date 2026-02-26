#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

inline void BoundsSystem(entt::registry& reg, float dt, int windowW, int windowH) {
    auto view = reg.view<Transform, Collider, GravityState, Velocity, PlayerTag>();
    view.each(
        [dt, windowW, windowH](Transform& t, const Collider& c, GravityState& g, Velocity& v) {
            // Tick punishment — block wall activation until it expires
            if (g.punishmentTimer > 0.0f) {
                g.punishmentTimer -= dt;
                if (g.punishmentTimer <= 0.0f) {
                    g.punishmentTimer = 0.0f;
                    g.active          = true; // re-enable gravity after punishment
                }
            }

            auto activate = [&](GravityDir dir) {
                if (g.punishmentTimer > 0.0f) return; // still being punished
                if (g.active && g.isGrounded && g.direction == dir)
                    return;
                g.timer      = 0.0f;
                g.active     = true;
                g.isGrounded = false;
                g.velocity   = 0.0f;
                g.direction  = dir;
                v.dx         = 0.0f;
                v.dy         = 0.0f;
            };

            if (t.x < 0.0f) {
                t.x = 0.0f;
                activate(GravityDir::LEFT);
            }
            if (t.x + c.w > windowW) {
                t.x = static_cast<float>(windowW - c.w);
                activate(GravityDir::RIGHT);
            }
            if (t.y < 0.0f) {
                t.y = 0.0f;
                activate(GravityDir::UP);
            }
            if (t.y + c.h > windowH) {
                t.y = static_cast<float>(windowH - c.h);
                activate(GravityDir::DOWN);
            }

            // Check if player has landed on their gravity wall
            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= windowH) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::LEFT:
                        if (t.x <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::RIGHT:
                        if (t.x + c.w >= windowW) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                }
            }
        });
}
