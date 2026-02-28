#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

/**
 * @brief Determines and transitions the player's active animation based on current state.
 *
 * Evaluates InvincibilityTimer, GravityState, and Velocity to select the
 * appropriate animation from the entity's AnimationSet, then updates Renderable
 * and AnimationState only when the animation actually changes.
 *
 * @par Animation Priority (highest to lowest)
 * -# Hurt  — playing when invincible after taking damage (non-looping)
 * -# Jump  — while airborne in gravity mode
 * -# Walk  — when |dx| or |dy| exceeds 1.0 px/s
 * -# Duck  — when crouching
 * -# Idle  — fallback when stationary
 */
// Standing collider: w=32, h=60, roff.x=-24  (centered over upright body)
// Crouching collider: w=44, h=32, roff.x=5   (over the crouched ball, bottom-left of frame)
inline constexpr int PLAYER_STAND_HEIGHT  = 60;
inline constexpr int PLAYER_STAND_WIDTH   = 32;
inline constexpr int PLAYER_DUCK_HEIGHT   = 32;
inline constexpr int PLAYER_DUCK_WIDTH    = 44;
inline constexpr int PLAYER_DUCK_ROFF_X   = 5;   // duck roff.x
inline constexpr int PLAYER_STAND_ROFF_X  = -24; // stand roff.x

inline void PlayerStateSystem(entt::registry& reg) {
    auto view = reg.view<PlayerTag,
                         Velocity,
                         GravityState,
                         Transform,
                         Collider,
                         Renderable,
                         AnimationState,
                         AnimationSet,
                         InvincibilityTimer>();

    view.each([&reg](entt::entity           entity,
                 const Velocity&           v,
                 const GravityState&       g,
                 Transform&                t,
                 Collider&                 col,
                 Renderable&               r,
                 AnimationState&           anim,
                 const AnimationSet&       set,
                 const InvincibilityTimer& inv) {
        const std::vector<SDL_Rect>* newFrames  = nullptr;
        float                        newFps     = 12.0f;
        bool                         newLooping = true;
        AnimationID                  newID      = AnimationID::NONE;

        bool isMoving = std::abs(v.dx) > 1.0f || std::abs(v.dy) > 1.0f;

        if (inv.isInvincible) {
            newFrames  = &set.hurt;
            newFps     = 8.0f;
            newLooping = false;
            newID      = AnimationID::HURT;
        } else if (g.active && !g.isGrounded) {
            newFrames  = &set.jump;
            newFps     = 10.0f;
            newLooping = true;
            newID      = AnimationID::JUMP;
        } else if (g.isCrouching) {
            newFrames  = &set.duck;
            newFps     = 8.0f;
            newLooping = true;
            newID      = AnimationID::DUCK;
        } else if (isMoving) {
            newFrames  = &set.walk;
            newFps     = 12.0f;
            newLooping = true;
            newID      = AnimationID::WALK;
        } else {
            newFrames  = &set.idle;
            newFps     = 8.0f;
            newLooping = true;
            newID      = AnimationID::IDLE;
        }

        if (newFrames && anim.currentAnim != newID) {
            // Swap the sheet if the new animation lives on a different surface
            SDL_Surface* newSheet = nullptr;
            switch (newID) {
                case AnimationID::IDLE:  newSheet = set.idleSheet;  break;
                case AnimationID::WALK:  newSheet = set.walkSheet;  break;
                case AnimationID::JUMP:  newSheet = set.jumpSheet;  break;
                case AnimationID::HURT:  newSheet = set.hurtSheet;  break;
                case AnimationID::DUCK:  newSheet = set.duckSheet;  break;
                case AnimationID::FRONT: newSheet = set.frontSheet; break;
                default: break;
            }
            if (newSheet && newSheet != r.sheet) {
                r.sheet = newSheet;
                // Invalidate flip cache — it was built for the old sheet
                if (reg.all_of<FlipCache>(entity)) {
                    auto& fc = reg.get<FlipCache>(entity);
                    for (auto* s : fc.frames) if (s) SDL_DestroySurface(s);
                    fc.frames.clear();
                }
            }

            bool wasD = anim.currentAnim == AnimationID::DUCK;
            bool nowD = newID            == AnimationID::DUCK;
            if (wasD != nowD) {
                int newH = nowD ? PLAYER_DUCK_HEIGHT : PLAYER_STAND_HEIGHT;
                int newW = nowD ? PLAYER_DUCK_WIDTH  : PLAYER_STAND_WIDTH;

                // Update RenderOffset so the hitbox tracks the visible body.
                // Standing: roff.x=-24 (centers 80px frame over 32px collider)
                // Crouching: roff.x=+5  (body ball sits in bottom-left of frame)
                // roff.y stays at -10 for both (feet-padding from frame bottom).
                auto* roff = reg.try_get<RenderOffset>(entity);
                if (roff) roff->x = nowD ? PLAYER_DUCK_ROFF_X : PLAYER_STAND_ROFF_X;

                // Anchor the wall-flush edge so the player doesn't float or clip.
                switch (g.direction) {
                    case GravityDir::DOWN:
                        // floor = bottom edge: keep t.y + col.h fixed
                        t.y   = (t.y + col.h) - newH;
                        col.h = newH;
                        col.w = newW;
                        break;
                    case GravityDir::UP:
                        // floor = top edge: t.y fixed
                        col.h = newH;
                        col.w = newW;
                        break;
                    case GravityDir::LEFT:
                        // floor = left edge: t.x fixed
                        // sidewall: col.h is the depth axis, col.w is the slide axis
                        col.h = newH;
                        col.w = newW;
                        break;
                    case GravityDir::RIGHT:
                        // floor = right edge: t.x + col.h fixed
                        col.h = newH;
                        col.w = newW;
                        break;
                }
            }

            r.frames          = *newFrames;
            anim.currentFrame = 0;
            anim.timer        = 0.0f;
            anim.fps          = newFps;
            anim.looping      = newLooping;
            anim.totalFrames  = static_cast<int>(newFrames->size());
            anim.currentAnim  = newID;
        }
    });
}
