#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

inline constexpr int PLAYER_STAND_HEIGHT = 60;
inline constexpr int PLAYER_STAND_WIDTH  = 32;
inline constexpr int PLAYER_DUCK_HEIGHT  = 32;
inline constexpr int PLAYER_DUCK_WIDTH   = 44;
inline constexpr int PLAYER_DUCK_ROFF_X  = 5;
inline constexpr int PLAYER_STAND_ROFF_X = -24;

inline void PlayerStateSystem(entt::registry& reg) {
    auto view = reg.view<PlayerTag, Velocity, GravityState, Transform, Collider,
                         Renderable, AnimationState, AnimationSet, InvincibilityTimer>();

    view.each([&reg](entt::entity entity,
                     const Velocity& v, const GravityState& g,
                     Transform& t, Collider& col,
                     Renderable& r, AnimationState& anim,
                     const AnimationSet& set, const InvincibilityTimer& inv) {

        const std::vector<SDL_Rect>* frames = nullptr;
        float    fps     = 12.0f;
        bool     looping = true;
        AnimationID id   = AnimationID::NONE;

        bool moving = std::abs(v.dx) > 1.0f || std::abs(v.dy) > 1.0f;

        if (inv.isInvincible) {
            frames = &set.hurt; fps = 8.0f; looping = false; id = AnimationID::HURT;
        } else if (g.active && !g.isGrounded) {
            frames = &set.jump; fps = 10.0f;                 id = AnimationID::JUMP;
        } else if (g.isCrouching) {
            frames = &set.duck; fps = 8.0f;                  id = AnimationID::DUCK;
        } else if (moving) {
            frames = &set.walk;                               id = AnimationID::WALK;
        } else {
            frames = &set.idle; fps = 8.0f;                  id = AnimationID::IDLE;
        }

        if (!frames || anim.currentAnim == id) return;

        SDL_Surface* sheet = nullptr;
        switch (id) {
            case AnimationID::IDLE:  sheet = set.idleSheet;  break;
            case AnimationID::WALK:  sheet = set.walkSheet;  break;
            case AnimationID::JUMP:  sheet = set.jumpSheet;  break;
            case AnimationID::HURT:  sheet = set.hurtSheet;  break;
            case AnimationID::DUCK:  sheet = set.duckSheet;  break;
            case AnimationID::FRONT: sheet = set.frontSheet; break;
            default: break;
        }
        if (sheet && sheet != r.sheet) {
            r.sheet = sheet;
            if (reg.all_of<FlipCache>(entity)) {
                auto& fc = reg.get<FlipCache>(entity);
                for (auto* s : fc.frames) if (s) SDL_DestroySurface(s);
                fc.frames.clear();
            }
        }

        bool wasDucking = anim.currentAnim == AnimationID::DUCK;
        bool nowDucking = id == AnimationID::DUCK;
        if (wasDucking != nowDucking) {
            int newH = nowDucking ? PLAYER_DUCK_HEIGHT : PLAYER_STAND_HEIGHT;
            int newW = nowDucking ? PLAYER_DUCK_WIDTH  : PLAYER_STAND_WIDTH;

            if (auto* roff = reg.try_get<RenderOffset>(entity))
                roff->x = nowDucking ? PLAYER_DUCK_ROFF_X : PLAYER_STAND_ROFF_X;

            if (g.direction == GravityDir::DOWN)
                t.y = (t.y + col.h) - newH;

            col.h = newH;
            col.w = newW;
        }

        r.frames          = *frames;
        anim.currentFrame = 0;
        anim.timer        = 0.0f;
        anim.fps          = fps;
        anim.looping      = looping;
        anim.totalFrames  = (int)frames->size();
        anim.currentAnim  = id;
    });
}
