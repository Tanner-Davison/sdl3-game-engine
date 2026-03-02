#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// LadderSystem
//
// Treats a vertical column of LadderTag tiles as one unit.
//
// How it works:
//   1. Find all ladder tiles that are horizontally aligned with the player.
//   2. From those, find the topmost tile's top-Y (columnTop) and the
//      bottommost tile's bottom-Y (columnBot). That is the whole climbable range.
//   3. While climbing W moves the player up clamped to columnTop,
//      S moves the player down. Player cannot go above columnTop.
//   4. When the player reaches columnTop they enter atTop state:
//      - Gravity is off, player is snapped to sit exactly on top of the column.
//      - CollisionSystem sees NO ladder tile (player is above all of them)
//        so no passthrough issue — they just sit on solid ground if there
//        is a tile there, or hover if not.
//      - Pressing S nudges the player back down into the column and resumes
//        climbing downward.
//      - SPACE or walking off horizontally restores gravity.
//
// MovementSystem must NOT touch t.y when climb.climbing || climb.atTop.
// ─────────────────────────────────────────────────────────────────────────────
inline void LadderSystem(entt::registry& reg, float dt) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    bool wHeld     = keys[SDL_SCANCODE_W];
    bool sHeld     = keys[SDL_SCANCODE_S];
    bool spaceHeld = keys[SDL_SCANCODE_SPACE];

    auto ladderView = reg.view<LadderTag, Transform, Collider>();
    auto playerView = reg.view<PlayerTag, Transform, Collider,
                               GravityState, Velocity, ClimbState>();

    playerView.each([&](Transform& pt, const Collider& pc,
                        GravityState& g, Velocity& v, ClimbState& climb) {

        // ── Build the ladder column the player is aligned with ────────────────
        // A tile is "in column" if it is horizontally aligned with the player
        // (with an inset so player must be reasonably centred).
        // columnTop = top edge of the topmost tile in the column
        // columnBot = bottom edge of the bottommost tile in the column
        // touching  = player's AABB currently overlaps at least one column tile
        constexpr float inset = 8.0f;

        float columnTop  =  1e9f;
        float columnBot  = -1e9f;
        bool  inColumn   = false;  // horizontally aligned with any ladder tile
        bool  touching   = false;  // vertically overlapping at least one tile

        ladderView.each([&](const Transform& lt, const Collider& lc) {
            bool alignX = (pt.x + inset)        < (lt.x + lc.w) &&
                          (pt.x + pc.w - inset)  >  lt.x;
            if (!alignX) return;

            inColumn  = true;
            columnTop = std::min(columnTop, lt.y);
            columnBot = std::max(columnBot, lt.y + lc.h);

            bool overlapY = pt.y         < (lt.y + lc.h) &&
                           (pt.y + pc.h) >  lt.y;
            if (overlapY) touching = true;
        });

        if (!inColumn) {
            columnTop = 0.0f;
            columnBot = 0.0f;
        }

        // The Y position where the player's feet sit exactly on top of the column
        float topRestY = columnTop - pc.h;

        climb.onLadder = touching;

        // ─────────────────────────────────────────────────────────────────────
        // atTop state
        // ─────────────────────────────────────────────────────────────────────
        if (climb.atTop) {
            // Keep player frozen — no gravity, no vertical movement
            v.dy       = 0.0f;
            g.active   = false;
            g.velocity = 0.0f;
            // Enforce position: feet exactly at column top
            if (inColumn) pt.y = topRestY;

            // SPACE → jump off
            if (spaceHeld) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                return;
            }

            // Walked off column horizontally → fall
            if (!inColumn) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = 0.0f;
                g.isGrounded = false;
                return;
            }

            // S → descend back into the column
            if (sHeld) {
                climb.atTop    = false;
                climb.climbing = true;
                g.active       = false;
                g.velocity     = 0.0f;
                // Nudge into the column so touching becomes true next frame
                pt.y = columnTop + 1.0f;
            }
            // W or no input → stay at top
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // climbing state
        // ─────────────────────────────────────────────────────────────────────
        if (climb.climbing) {
            v.dy       = 0.0f;
            g.velocity = 0.0f;

            // SPACE → jump off
            if (spaceHeld) {
                climb.climbing = false;
                g.active       = true;
                g.velocity     = -JUMP_FORCE;
                g.isGrounded   = false;
                return;
            }

            // No longer in column → restore gravity
            if (!inColumn) {
                climb.climbing = false;
                g.active       = true;
                g.velocity     = 0.0f;
                g.isGrounded   = false;
                return;
            }

            if (wHeld) {
                pt.y -= CLIMB_SPEED * dt;
                // Clamp to column top — cannot go above it
                if (pt.y <= topRestY) {
                    pt.y           = topRestY;
                    climb.climbing = false;
                    climb.atTop    = true;
                    g.active       = false;
                    g.velocity     = 0.0f;
                }
            } else if (sHeld) {
                pt.y += CLIMB_SPEED * dt;
            }
            // Neither key → hang
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // idle state — grab ladder on W or S
        // ─────────────────────────────────────────────────────────────────────
        if (touching && (wHeld || sHeld)) {
            climb.climbing = true;
            g.active       = false;
            g.velocity     = 0.0f;
            v.dy           = 0.0f;

            if (wHeld) {
                pt.y -= CLIMB_SPEED * dt;
                if (pt.y <= topRestY) {
                    pt.y           = topRestY;
                    climb.climbing = false;
                    climb.atTop    = true;
                }
            } else {
                pt.y += CLIMB_SPEED * dt;
            }
        }
    });
}
