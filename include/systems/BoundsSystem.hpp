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

            // For LEFT/RIGHT gravity the collider is rotated in world-space:
            // the dimension pressing into the wall is c.h (the tall axis), not c.w.
            // Use the correct depth for each wall so snapping and grounding are exact.
            bool sidewall = g.active &&
                            (g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT);
            int wallDepth = sidewall ? c.h : c.w; // how far the player extends toward L/R wall
            int wallHeight = sidewall ? c.w : c.h; // the perpendicular (slide) axis

            if (t.x < 0.0f) {
                t.x = 0.0f;
                activate(GravityDir::LEFT);
            }
            if (t.x + (sidewall ? c.h : c.w) > windowW) {
                t.x = static_cast<float>(windowW - (sidewall ? c.h : c.w));
                activate(GravityDir::RIGHT);
            }
            if (t.y < 0.0f) {
                t.y = 0.0f;
                activate(GravityDir::UP);
            }
            if (t.y + (sidewall ? c.w : c.h) > windowH) {
                t.y = static_cast<float>(windowH - (sidewall ? c.w : c.h));
                activate(GravityDir::DOWN);
            }

            // Check if player has landed on their gravity wall
            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= windowH) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                            t.y          = static_cast<float>(windowH - c.h);
                        }
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                            t.y          = 0.0f;
                        }
                        break;
                    case GravityDir::LEFT:
                        if (t.x <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                            t.x          = 0.0f;
                        }
                        break;
                    case GravityDir::RIGHT:
                        // The leading edge toward the right wall is t.x + c.h (rotated depth)
                        if (t.x + c.h >= windowW) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                            t.x          = static_cast<float>(windowW - c.h);
                        }
                        break;
                }
            }
        });
}
