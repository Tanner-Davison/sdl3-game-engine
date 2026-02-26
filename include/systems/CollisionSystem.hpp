#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

inline void CollisionSystem(entt::registry& reg, float dt, bool& gameOver) {
    // Tick down all invincibility timers
    auto timerView = reg.view<InvincibilityTimer>();
    timerView.each([dt](InvincibilityTimer& inv) {
        if (inv.isInvincible) {
            inv.remaining -= dt;
            if (inv.remaining <= 0.0f) {
                inv.remaining    = 0.0f;
                inv.isInvincible = false;
            }
        }
    });

    auto playerView =
        reg.view<PlayerTag, GravityState, Transform, Collider, Health, InvincibilityTimer>();
    auto enemyView = reg.view<Transform, Collider>(entt::exclude<PlayerTag>);

    playerView.each([&](GravityState&       g,
                        const Transform&    pt,
                        const Collider&     pc,
                        Health&             health,
                        InvincibilityTimer& inv) {
        if (inv.isInvincible)
            return;

        enemyView.each([&](const Transform& et, const Collider& ec) {
            // Side-wall rotation swaps effective collider dimensions
            bool  sidewall  = g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT;
            int   pcW       = sidewall ? pc.h : pc.w;
            int   pcH       = sidewall ? pc.w : pc.h;
            float adjustedX = pt.x;
            if (g.direction == GravityDir::RIGHT)
                adjustedX -= static_cast<float>(pc.h - PLAYER_SPRITE_WIDTH);

            bool overlapX = adjustedX < et.x + ec.w && adjustedX + pcW > et.x;
            bool overlapY = pt.y < et.y + ec.h && pt.y + pcH > et.y;

            if (overlapX && overlapY) {
                health.current      -= PLAYER_HIT_DAMAGE;
                inv.isInvincible     = true;
                inv.remaining        = inv.duration;
                // Release into free-float punishment for GRAVITY_DURATION seconds
                g.active            = false;
                g.velocity          = 0.0f;
                g.isGrounded        = false;
                g.punishmentTimer   = GRAVITY_DURATION;

                if (health.current <= 0.0f) {
                    health.current = 0.0f;
                    gameOver       = true;
                }
            }
        });
    });
}
