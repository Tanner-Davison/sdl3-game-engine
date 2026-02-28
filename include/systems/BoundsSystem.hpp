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

            // Each wall uses its own correct collider dimension so that direction
            // changes mid-frame (via activate()) never cause stale sidewall math.
            //
            // When gravity is LEFT or RIGHT the collider is rotated in world-space:
            //   X depth  = c.h  (the tall axis presses into the wall)
            //   Y height = c.w  (the narrow axis slides along the wall)
            // When gravity is UP or DOWN the collider is upright:
            //   X width  = c.w
            //   Y height = c.h
            //
            // LEFT wall  — player's left edge (t.x) must stay >= 0
            if (t.x < 0.0f) {
                t.x = 0.0f;
                activate(GravityDir::LEFT);
            }
            // RIGHT wall — player's right edge depends on current gravity direction
            //   LEFT/RIGHT gravity: right edge = t.x + c.h
            //   UP/DOWN gravity:    right edge = t.x + c.w
            {
                bool sw = g.active && (g.direction == GravityDir::LEFT ||
                                       g.direction == GravityDir::RIGHT);
                float rightEdge = t.x + (sw ? c.h : c.w);
                if (rightEdge > windowW) {
                    t.x = static_cast<float>(windowW - (sw ? c.h : c.w));
                    activate(GravityDir::RIGHT);
                }
            }
            // TOP wall — player's top edge (t.y) must stay >= 0
            if (t.y < 0.0f) {
                t.y = 0.0f;
                activate(GravityDir::UP);
            }
            // BOTTOM wall — player's bottom edge depends on current gravity direction
            //   LEFT/RIGHT gravity: bottom = t.y + c.w  (rotated: narrow axis is vertical)
            //   UP/DOWN gravity:    bottom = t.y + c.h
            {
                bool sw = g.active && (g.direction == GravityDir::LEFT ||
                                       g.direction == GravityDir::RIGHT);
                float bottomEdge = t.y + (sw ? c.w : c.h);
                if (bottomEdge > windowH) {
                    t.y = static_cast<float>(windowH - (sw ? c.w : c.h));
                    activate(GravityDir::DOWN);
                }
            }

            // Ground detection — clamp and set isGrounded when touching the gravity wall.
            // Each case uses the correct dimension for that specific wall direction.
            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= windowH) {
                            t.y          = static_cast<float>(windowH - c.h);
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            t.y          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::LEFT:
                        // Left wall: depth axis is c.h, so floor edge is t.x (left edge)
                        if (t.x <= 0.0f) {
                            t.x          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::RIGHT:
                        // Right wall: depth axis is c.h, so floor edge is t.x + c.h
                        if (t.x + c.h >= windowW) {
                            t.x          = static_cast<float>(windowW - c.h);
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                }
            }
        });
}
