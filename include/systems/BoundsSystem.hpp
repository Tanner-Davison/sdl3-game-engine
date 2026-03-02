#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

inline void BoundsSystem(entt::registry& reg, float dt, int windowW, int windowH) {
    auto view = reg.view<Transform, Collider, GravityState, Velocity, AnimationState, PlayerTag>();
    view.each(
        [dt, windowW, windowH](Transform& t, Collider& c, GravityState& g, Velocity& v, AnimationState& anim) {
            if (g.punishmentTimer > 0.0f) {
                g.punishmentTimer -= dt;
                if (g.punishmentTimer <= 0.0f) {
                    g.punishmentTimer = 0.0f;
                    g.active          = true;
                }
            }

            auto activate = [&](GravityDir dir) {
                if (g.punishmentTimer > 0.0f) return;
                if (g.active && g.isGrounded && g.direction == dir) return;

                // Reset to standing collider and clear crouch state on wall transition.
                // Also reset anim.currentAnim so PlayerStateSystem's wasDucking/nowDucking
                // comparison starts clean — prevents stale duck dimensions from the old
                // wall feeding into the resize calculation on the new wall.
                g.isCrouching    = false;
                c.w              = PLAYER_STAND_WIDTH;
                c.h              = PLAYER_STAND_HEIGHT;
                anim.currentAnim = AnimationID::NONE;

                g.timer      = 0.0f;
                g.active     = true;
                g.isGrounded = false;
                g.velocity   = 0.0f;
                g.direction  = dir;
                v.dx         = 0.0f;
                v.dy         = 0.0f;
            };

            // LEFT wall
            if (t.x < 0.0f) {
                t.x = 0.0f;
                activate(GravityDir::LEFT);
            }
            // RIGHT wall
            {
                bool  sw        = g.active && (g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT);
                float rightEdge = t.x + (sw ? c.h : c.w);
                if (rightEdge > windowW) {
                    t.x = static_cast<float>(windowW - (sw ? c.h : c.w));
                    activate(GravityDir::RIGHT);
                }
            }
            // TOP wall
            if (t.y < 0.0f) {
                t.y = 0.0f;
                activate(GravityDir::UP);
            }
            // BOTTOM wall
            {
                bool  sw         = g.active && (g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT);
                float bottomEdge = t.y + (sw ? c.w : c.h);
                if (bottomEdge > windowH) {
                    t.y = static_cast<float>(windowH - (sw ? c.w : c.h));
                    activate(GravityDir::DOWN);
                }
            }

            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= windowH) {
                            t.y          = static_cast<float>(windowH - c.h);
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        // Hard clamp — never let player sink below floor regardless of source
                        if (t.y + c.h > windowH) t.y = static_cast<float>(windowH - c.h);
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            t.y          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        if (t.y < 0.0f) t.y = 0.0f;
                        break;
                    case GravityDir::LEFT:
                        if (t.x <= 0.0f) {
                            t.x          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        if (t.x < 0.0f) t.x = 0.0f;
                        break;
                    case GravityDir::RIGHT:
                        if (t.x + c.h >= windowW) {
                            t.x          = static_cast<float>(windowW - c.h);
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        if (t.x + c.h > windowW) t.x = static_cast<float>(windowW - c.h);
                        break;
                }
            }
        });
}
