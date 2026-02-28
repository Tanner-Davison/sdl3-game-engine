#pragma once
#include <Components.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <vector>

inline void CollisionSystem(entt::registry& reg, float dt, bool& gameOver, int& coinCount, int& stompCount, int windowW, int windowH) {
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

    auto liveEnemyView = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
    auto deadEnemyView = reg.view<DeadTag, Transform, Collider>();
    auto coinView      = reg.view<CoinTag, Transform, Collider>();
    auto playerView    = reg.view<PlayerTag, GravityState, Transform, Collider, Health, InvincibilityTimer>();

    std::vector<entt::entity> toKill;
    std::vector<entt::entity> toDestroy;
    toKill.reserve(4);
    toDestroy.reserve(8);

    playerView.each([&](GravityState& g, Transform& pt, const Collider& pc, Health& health, InvincibilityTimer& inv) {
        bool sidewall = g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT;
        float pw = sidewall ? (float)pc.h : (float)pc.w;
        float ph = sidewall ? (float)pc.w : (float)pc.h;

        auto aabb = [&](const Transform& et, const Collider& ec) -> bool {
            return pt.x < et.x + ec.w && pt.x + pw > et.x &&
                   pt.y < et.y + ec.h && pt.y + ph > et.y;
        };

        auto isStomp = [&](const Transform& et, const Collider& ec) -> bool {
            if (!aabb(et, ec) || g.velocity <= 0.0f) return false;
            switch (g.direction) {
                case GravityDir::DOWN:  return (pt.y + pc.h) <= et.y + ec.h;
                case GravityDir::UP:    return pt.y >= et.y;
                case GravityDir::LEFT:  return pt.x >= et.x;
                case GravityDir::RIGHT: return (pt.x + pc.h) <= et.x + ec.w;
            }
            return false;
        };

        liveEnemyView.each([&](entt::entity enemy, const Transform& et, const Collider& ec) {
            if (isStomp(et, ec)) {
                toKill.push_back(enemy);
                stompCount++;
                g.velocity   = -JUMP_FORCE * 0.5f;
                g.isGrounded = false;
            } else if (!inv.isInvincible && aabb(et, ec)) {
                health.current -= PLAYER_HIT_DAMAGE;
                if (health.current <= 0.0f) {
                    health.current = 0.0f;
                    gameOver = true;
                }
                inv.isInvincible  = true;
                inv.remaining     = inv.duration;
                g.active          = false;
                g.velocity        = 0.0f;
                g.isGrounded      = false;
                g.punishmentTimer = GRAVITY_DURATION;
            }
        });

        bool onDeadEnemy = false;
        deadEnemyView.each([&](const Transform& et, const Collider& ec) {
            if (g.velocity < 0.0f) return;
            switch (g.direction) {
                case GravityDir::DOWN: {
                    float bottom = pt.y + pc.h;
                    if (pt.x < et.x + ec.w && pt.x + pc.w > et.x &&
                        bottom >= et.y && bottom <= et.y + ec.h) {
                        pt.y = et.y - pc.h;
                        g.velocity = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::UP: {
                    if (pt.x < et.x + ec.w && pt.x + pc.w > et.x &&
                        pt.y <= et.y + ec.h && pt.y >= et.y) {
                        pt.y = et.y + ec.h;
                        g.velocity = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::LEFT: {
                    if (pt.y < et.y + ec.h && pt.y + pc.h > et.y &&
                        pt.x <= et.x + ec.w && pt.x >= et.x) {
                        pt.x = et.x + ec.w;
                        g.velocity = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::RIGHT: {
                    float right = pt.x + pc.h;
                    if (pt.y < et.y + ec.h && pt.y + pc.w > et.y &&
                        right >= et.x && right <= et.x + ec.w) {
                        pt.x = et.x - pc.h;
                        g.velocity = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
            }
        });

        if (!onDeadEnemy && g.isGrounded) {
            bool onWindow = false;
            switch (g.direction) {
                case GravityDir::DOWN:  onWindow = pt.y + pc.h >= windowH; break;
                case GravityDir::UP:    onWindow = pt.y <= 0.0f;            break;
                case GravityDir::LEFT:  onWindow = pt.x <= 0.0f;            break;
                case GravityDir::RIGHT: onWindow = pt.x + pc.h >= windowW;  break;
            }
            if (!onWindow) g.isGrounded = false;
        }

        auto tileView = reg.view<TileTag, Transform, Collider>();

        // Pass 1 — gravity axis. Snap feet to floors, head to ceilings.
        // Must run before pass 2 so horizontal contacts resolve at the correct height.
        tileView.each([&](const Transform& tt, const Collider& tc) {
            if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

            float oTop    = (pt.y + ph) - tt.y;
            float oBottom = (tt.y + tc.h) - pt.y;
            float oLeft   = (pt.x + pw) - tt.x;
            float oRight  = (tt.x + tc.w) - pt.x;

            switch (g.direction) {
                case GravityDir::DOWN:
                    if (oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y = tt.y - ph;
                        g.velocity = 0.0f;
                    } else if (oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y = tt.y + tc.h;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::UP:
                    if (oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y = tt.y + tc.h;
                        g.velocity = 0.0f;
                    } else if (oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        pt.y = tt.y - ph;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::LEFT:
                    if (oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x = tt.x + tc.w;
                        g.velocity = 0.0f;
                    } else if (oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        pt.x = tt.x - pw;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::RIGHT:
                    if (oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x = tt.x - pw;
                        g.velocity = 0.0f;
                    } else if (oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        pt.x = tt.x + tc.w;
                        g.velocity = 0.0f;
                    }
                    break;
            }
        });

        // Pass 2 — lateral axis. Player is at the right floor height now,
        // so any remaining overlap is a wall and gets pushed out horizontally.
        tileView.each([&](const Transform& tt, const Collider& tc) {
            if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    float oLeft  = (pt.x + pw) - tt.x;
                    float oRight = (tt.x + tc.w) - pt.x;
                    pt.x = oLeft < oRight ? tt.x - pw : tt.x + tc.w;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    float oTop    = (pt.y + ph) - tt.y;
                    float oBottom = (tt.y + tc.h) - pt.y;
                    pt.y = oTop < oBottom ? tt.y - ph : tt.y + tc.h;
                    break;
                }
            }
        });

        if (g.active) {
            coinView.each([&](entt::entity coin, const Transform& ct, const Collider& cc) {
                if (aabb(ct, cc)) {
                    toDestroy.push_back(coin);
                    coinCount++;
                }
            });
        }
    });

    for (auto e : toKill) {
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = v.dy = 0.0f;
        }
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            r.frames          = {{0, 112, 59, 12}};
            anim.currentFrame = 0;
            anim.totalFrames  = 1;
            anim.looping      = false;
        }
        if (reg.all_of<Collider>(e)) {
            auto& col = reg.get<Collider>(e);
            col.w = 59;
            col.h = 12;
        }
        reg.emplace<DeadTag>(e);
    }

    for (auto e : toDestroy)
        reg.destroy(e);
}
