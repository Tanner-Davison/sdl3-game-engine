#pragma once
#include <Components.hpp>
#include <cmath>
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
        // AABB overlap helper — accounts for side-wall collider rotation.
        // For LEFT/RIGHT gravity the collider's h becomes the X extent and w the Y extent.
        auto overlap = [&](const Transform& et, const Collider& ec) -> bool {
            bool sidewall = g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT;
            int  pcW      = sidewall ? pc.h : pc.w;
            int  pcH      = sidewall ? pc.w : pc.h;
            bool overlapX = pt.x < et.x + ec.w && pt.x + pcW > et.x;
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
                    // Feet = right edge (pt.x + pc.h), moving rightward
                    float feetEdge = pt.x + pc.h;
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
                    float playerRight = pt.x + pc.h;
                    bool  vertOverlap = pt.y < et.y + ec.h && pt.y + pc.w > et.y;
                    if (vertOverlap && playerRight >= et.x && playerRight <= et.x + ec.w) {
                        pt.x         = et.x - pc.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onDeadEnemy  = true;
                    }
                    break;
                }
            }
        });

        // If no dead enemy is supporting the player and they aren't on a real wall,
        // tentatively clear isGrounded. The tile loop below will re-set it if the
        // player is resting on a tile. This avoids the one-frame flicker that made
        // jump detection unreliable on tile surfaces.
        if (!onDeadEnemy && g.isGrounded) {
            bool onRealWall = false;
            switch (g.direction) {
                case GravityDir::DOWN:  onRealWall = (pt.y + pc.h >= windowH); break;
                case GravityDir::UP:    onRealWall = (pt.y <= 0.0f);            break;
                case GravityDir::LEFT:  onRealWall = (pt.x <= 0.0f);            break;
                case GravityDir::RIGHT: onRealWall = (pt.x + pc.h >= windowW);  break;
            }
            if (!onRealWall)
                g.isGrounded = false; // tiles will restore this below if still supported
        }

        // Tile collision — two-pass approach to prevent corner-climbing.
        //
        // Pass 1: gravity axis only (Y for DOWN/UP, X for LEFT/RIGHT).
        //   Resolves landing on floors and hitting ceilings first, so pt.y is
        //   correct before we ever check horizontal contacts.
        // Pass 2: lateral axis only (X for DOWN/UP, Y for LEFT/RIGHT).
        //   Now that the player sits at the right height, any horizontal overlap
        //   with a wall tile is purely a wall contact — never a floor corner.
        //
        // This eliminates the "walk up onto a tile" bug where a single-pass
        // resolver would pick resolveY on a wall tile's top corner before the
        // floor tile had a chance to pin the player's Y position.

        bool sidewall = (g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT);
        float pcW = sidewall ? static_cast<float>(pc.h) : static_cast<float>(pc.w);
        float pcH = sidewall ? static_cast<float>(pc.w) : static_cast<float>(pc.h);

        auto tileView = reg.view<TileTag, Transform, Collider>();

        // ── Pass 1: gravity axis ────────────────────────────────────────────
        tileView.each([&](const Transform& tt, const Collider& tc) {
            // Broad phase
            if (pt.x + pcW <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + pcH <= tt.y || pt.y >= tt.y + tc.h) return;

            float overlapTop    = (pt.y + pcH) - tt.y;
            float overlapBottom = (tt.y + tc.h) - pt.y;
            float overlapLeft   = (pt.x + pcW) - tt.x;
            float overlapRight  = (tt.x + tc.w) - pt.x;

            switch (g.direction) {
                case GravityDir::DOWN: {
                    // Only resolve downward landing (player falling onto tile top)
                    if (overlapTop < overlapBottom && overlapTop <= overlapLeft && overlapTop <= overlapRight) {
                        bool wasFalling = g.velocity >= 0.0f;
                        pt.y = tt.y - pcH;
                        g.velocity = 0.0f;
                        if (wasFalling) g.isGrounded = true;
                    }
                    // Ceiling hit (jumping up into tile bottom)
                    else if (overlapBottom < overlapTop && overlapBottom <= overlapLeft && overlapBottom <= overlapRight) {
                        bool wasFalling = g.velocity >= 0.0f;
                        pt.y = tt.y + tc.h;
                        g.velocity = 0.0f;
                        // Don't set grounded when hitting ceiling from below
                    }
                    break;
                }
                case GravityDir::UP: {
                    if (overlapBottom < overlapTop && overlapBottom <= overlapLeft && overlapBottom <= overlapRight) {
                        bool wasFalling = g.velocity >= 0.0f;
                        pt.y = tt.y + tc.h;
                        g.velocity = 0.0f;
                        if (wasFalling) g.isGrounded = true;
                    } else if (overlapTop < overlapBottom && overlapTop <= overlapLeft && overlapTop <= overlapRight) {
                        pt.y = tt.y - pcH;
                        g.velocity = 0.0f;
                    }
                    break;
                }
                case GravityDir::LEFT: {
                    if (overlapRight < overlapLeft && overlapRight <= overlapTop && overlapRight <= overlapBottom) {
                        bool wasFalling = g.velocity >= 0.0f;
                        pt.x = tt.x + tc.w;
                        g.velocity = 0.0f;
                        if (wasFalling) g.isGrounded = true;
                    } else if (overlapLeft < overlapRight && overlapLeft <= overlapTop && overlapLeft <= overlapBottom) {
                        pt.x = tt.x - pcW;
                        g.velocity = 0.0f;
                    }
                    break;
                }
                case GravityDir::RIGHT: {
                    if (overlapLeft < overlapRight && overlapLeft <= overlapTop && overlapLeft <= overlapBottom) {
                        bool wasFalling = g.velocity >= 0.0f;
                        pt.x = tt.x - pcW;
                        g.velocity = 0.0f;
                        if (wasFalling) g.isGrounded = true;
                    } else if (overlapRight < overlapLeft && overlapRight <= overlapTop && overlapRight <= overlapBottom) {
                        pt.x = tt.x + tc.w;
                        g.velocity = 0.0f;
                    }
                    break;
                }
            }
        });

        // ── Pass 2: lateral axis ────────────────────────────────────────────
        // Player Y is now correct from pass 1. Any remaining overlap with a tile
        // is purely horizontal (wall contact). Push out laterally.
        tileView.each([&](const Transform& tt, const Collider& tc) {
            // Broad phase
            if (pt.x + pcW <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + pcH <= tt.y || pt.y >= tt.y + tc.h) return;

            float overlapLeft  = (pt.x + pcW) - tt.x;
            float overlapRight = (tt.x + tc.w) - pt.x;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP:
                    // Horizontal wall push-out
                    if (overlapLeft < overlapRight) {
                        pt.x = tt.x - pcW; // push left
                    } else {
                        pt.x = tt.x + tc.w; // push right
                    }
                    break;
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    // Vertical wall push-out
                    float oTop    = (pt.y + pcH) - tt.y;
                    float oBottom = (tt.y + tc.h) - pt.y;
                    if (oTop < oBottom) {
                        pt.y = tt.y - pcH;
                    } else {
                        pt.y = tt.y + tc.h;
                    }
                    break;
                }
            }
        });

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
