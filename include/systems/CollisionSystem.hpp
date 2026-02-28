#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>
#include <vector>

inline void CollisionSystem(entt::registry& reg, float dt, bool& gameOver, int& coinCount, int& stompCount, int windowW, int windowH) {
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

    // Live enemies only — exclude player, coins, and already-dead enemies
    auto liveEnemyView = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
    // Dead enemies — used for platform standing
    auto deadEnemyView = reg.view<DeadTag, Transform, Collider>();
    auto coinView      = reg.view<CoinTag, Transform, Collider>();
    auto playerView    =
        reg.view<PlayerTag, GravityState, Transform, Collider, Health, InvincibilityTimer>();

    // Deferred mutations — applied after all iteration
    std::vector<entt::entity> toKill; // enemies to stomp
    std::vector<entt::entity> toDestroy; // coins to collect
    toKill.reserve(4);
    toDestroy.reserve(8);

    playerView.each([&](GravityState&       g,
                        Transform&          pt,
                        const Collider&     pc,
                        Health&             health,
                        InvincibilityTimer& inv) {
        // AABB overlap helper — accounts for side-wall rotation
        auto overlap = [&](const Transform& et, const Collider& ec) -> bool {
            bool  sidewall  = g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT;
            int   pcW       = sidewall ? pc.h : pc.w;
            int   pcH       = sidewall ? pc.w : pc.h;
            float adjustedX = pt.x;
            if (g.direction == GravityDir::RIGHT)
                adjustedX -= static_cast<float>(pc.h - PLAYER_SPRITE_WIDTH);
            bool overlapX = adjustedX < et.x + ec.w && adjustedX + pcW > et.x;
            bool overlapY = pt.y < et.y + ec.h && pt.y + pcH > et.y;
            return overlapX && overlapY;
        };

        // Stomp detection — fires when the player's gravity-facing edge (their "feet")
        // contacts the enemy while moving toward the wall. Works on all four walls.
        auto isStomp = [&](const Transform& et, const Collider& ec) -> bool {
            if (!overlap(et, ec)) return false;  // must be overlapping first
            if (g.velocity <= 0.0f) return false; // must be moving toward wall

            switch (g.direction) {
                case GravityDir::DOWN: {
                    // Feet = bottom edge (pt.y + pc.h), moving downward
                    float feetEdge = pt.y + pc.h;
                    return feetEdge <= et.y + ec.h; // feet entered from above
                }
                case GravityDir::UP: {
                    // Feet = top edge (pt.y), moving upward (velocity inverted)
                    float feetEdge = pt.y;
                    return feetEdge >= et.y;
                }
                case GravityDir::LEFT: {
                    // Feet = left edge (pt.x), moving leftward
                    float feetEdge = pt.x;
                    return feetEdge >= et.x;
                }
                case GravityDir::RIGHT: {
                    // Feet = right edge (pt.x + pc.w), moving rightward
                    float adjustedX = pt.x - static_cast<float>(pc.h - PLAYER_SPRITE_WIDTH);
                    float feetEdge  = adjustedX + pc.h;
                    return feetEdge <= et.x + ec.w;
                }
            }
            return false;
        };

        // Live enemy collision
        liveEnemyView.each([&](entt::entity        enemyEntity,
                               const Transform&    et,
                               const Collider&     ec) {
            if (isStomp(et, ec)) {
                // Stomp — kill the enemy, bounce player up slightly
                toKill.push_back(enemyEntity);
                stompCount++;
                g.velocity   = -JUMP_FORCE * 0.5f; // little bounce
                g.isGrounded = false;
            } else if (!inv.isInvincible && overlap(et, ec)) {
                // Side/bottom hit — take damage
                health.current   -= PLAYER_HIT_DAMAGE;
                inv.isInvincible  = true;
                inv.remaining     = inv.duration;
                g.active          = false;
                g.velocity        = 0.0f;
                g.isGrounded      = false;
                g.punishmentTimer = GRAVITY_DURATION;

                if (health.current <= 0.0f) {
                    health.current = 0.0f;
                    gameOver       = true;
                }
            }
        });

        // Dead enemy platform collision — player can stand on dead slimes on any wall
        // Track whether any dead enemy is currently supporting the player this frame.
        // If none are, and the player isn't on a real wall, clear isGrounded so gravity resumes.
        bool onDeadEnemy = false;
        deadEnemyView.each([&](const Transform& et, const Collider& ec) {
            if (g.velocity < 0.0f) return;

            switch (g.direction) {
                case GravityDir::DOWN: {
                    float playerBottom = pt.y + pc.h;
                    bool  horizOverlap = pt.x < et.x + ec.w && pt.x + pc.w > et.x;
                    if (horizOverlap && playerBottom >= et.y && playerBottom <= et.y + ec.h) {
                        pt.y         = et.y - pc.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onDeadEnemy  = true;
                    }
                    break;
                }
                case GravityDir::UP: {
                    float playerTop   = pt.y;
                    bool  horizOverlap = pt.x < et.x + ec.w && pt.x + pc.w > et.x;
                    if (horizOverlap && playerTop <= et.y + ec.h && playerTop >= et.y) {
                        pt.y         = et.y + ec.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onDeadEnemy  = true;
                    }
                    break;
                }
                case GravityDir::LEFT: {
                    float playerLeft  = pt.x;
                    bool  vertOverlap = pt.y < et.y + ec.h && pt.y + pc.h > et.y;
                    if (vertOverlap && playerLeft <= et.x + ec.w && playerLeft >= et.x) {
                        pt.x         = et.x + ec.w;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onDeadEnemy  = true;
                    }
                    break;
                }
                case GravityDir::RIGHT: {
                    float adjustedX   = pt.x - static_cast<float>(pc.h - PLAYER_SPRITE_WIDTH);
                    float playerRight = adjustedX + pc.h;
                    bool  vertOverlap = pt.y < et.y + ec.h && pt.y + pc.w > et.y;
                    if (vertOverlap && playerRight >= et.x && playerRight <= et.x + ec.w) {
                        pt.x         = et.x - pc.h + static_cast<float>(pc.h - PLAYER_SPRITE_WIDTH);
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onDeadEnemy  = true;
                    }
                    break;
                }
            }
        });

        // If no dead enemy is supporting the player and they aren't on a real wall,
        // unground them so gravity pulls them back down
        if (!onDeadEnemy && g.isGrounded) {
            bool onRealWall = false;
            switch (g.direction) {
                case GravityDir::DOWN:  onRealWall = (pt.y + pc.h >= windowH); break;
                case GravityDir::UP:    onRealWall = (pt.y <= 0.0f);            break;
                case GravityDir::LEFT:  onRealWall = (pt.x <= 0.0f);            break;
                case GravityDir::RIGHT: onRealWall = (pt.x + pc.w >= windowW);  break;
            }
            if (!onRealWall) {
                g.isGrounded = false;
            }
        }

        // Coin collection — only in gravity mode
        if (g.active) {
            coinView.each([&](entt::entity coinEntity, const Transform& ct, const Collider& cc) {
                if (overlap(ct, cc)) {
                    toDestroy.push_back(coinEntity);
                    coinCount++;
                }
            });
        }
    });

    // Apply stomps — swap to dead sprite, zero velocity, add DeadTag
    for (auto e : toKill) {
        // Zero out velocity so the enemy stops moving
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = 0.0f;
            v.dy = 0.0f;
        }
        // Swap renderable to dead frame from the enemies spritesheet
        // slimeDead = 0 112 59 12 in the spritesheet
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            r.frames         = {{0, 112, 59, 12}};
            anim.currentFrame = 0;
            anim.totalFrames  = 1;
            anim.looping      = false;
        }
        // Shrink collider to match the flat dead sprite dimensions
        if (reg.all_of<Collider>(e)) {
            auto& col = reg.get<Collider>(e);
            col.w = 59;
            col.h = 12;
        }
        reg.emplace<DeadTag>(e);
    }

    // Destroy collected coins
    for (auto e : toDestroy)
        reg.destroy(e);
}
