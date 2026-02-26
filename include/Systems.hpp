#include "Text.hpp"
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <SurfaceUtils.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <print>

inline void MovementSystem(entt::registry& reg, float dt, int windowW) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag>();
    playerView.each([dt, keys](Transform& t, Velocity& v, GravityState& g) {
        if (!g.active) {
            // FREE MODE — friction on key release
            bool anyKeyHeld = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                              keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
            if (!anyKeyHeld) {
                constexpr float friction = 3.0f;
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f)
                    v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f)
                    v.dy = 0.0f;
            }
            t.x += v.dx * dt;
            t.y += v.dy * dt;
        } else {
            // GRAVITY MODE — movement perpendicular to gravity axis still works
            v.dx = 0.0f;
            v.dy = 0.0f;

            bool            horizKey = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
            bool            vertKey  = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S];
            constexpr float friction = 3.0f;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    // Horizontal walking still works
                    if (keys[SDL_SCANCODE_A])
                        v.dx = -v.speed;
                    if (keys[SDL_SCANCODE_D])
                        v.dx = v.speed;
                    if (!horizKey) {
                        v.dx -= v.dx * friction * dt;
                        if (std::abs(v.dx) < 0.5f)
                            v.dx = 0.0f;
                    }
                    t.x += v.dx * dt;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    // Vertical walking still works
                    if (keys[SDL_SCANCODE_W])
                        v.dy = -v.speed;
                    if (keys[SDL_SCANCODE_S])
                        v.dy = v.speed;
                    if (!vertKey) {
                        v.dy -= v.dy * friction * dt;
                        if (std::abs(v.dy) < 0.5f)
                            v.dy = 0.0f;
                    }
                    t.y += v.dy * dt;
                    break;
                }
            }

            // Apply gravity along the gravity axis
            if (!g.isGrounded) {
                g.velocity += GRAVITY_FORCE * dt;
                if (g.velocity > MAX_FALL_SPEED)
                    g.velocity = MAX_FALL_SPEED;
            }

            // Hold space boosts away from wall
            if (g.jumpHeld && !g.isGrounded && g.velocity < 0.0f) {
                g.velocity -= JUMP_FORCE * 0.5f * dt;
            }

            // Apply gravity velocity in the correct direction
            switch (g.direction) {
                case GravityDir::DOWN:
                    t.y += g.velocity * dt;
                    break;
                case GravityDir::UP:
                    t.y -= g.velocity * dt;
                    break;
                case GravityDir::LEFT:
                    t.x -= g.velocity * dt;
                    break;
                case GravityDir::RIGHT:
                    t.x += g.velocity * dt;
                    break;
            }

            // Accumulate time on current wall (available for future use)
            g.timer += dt;
        }
    });

    // Enemies bounce left and right within the window
    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider>();
    enemyView.each([dt, windowW](Transform& t, Velocity& v, const Collider& c) {
        t.x += v.dx * dt;
        if (t.x < 0.0f) {
            t.x  = 0.0f;
            v.dx = std::abs(v.dx);
        } else if (t.x + c.w > windowW) {
            t.x  = static_cast<float>(windowW - c.w);
            v.dx = -std::abs(v.dx);
        }
    });
};

/**
 * @brief Advances frame counters for all animated entities each tick.
 *
 * Accumulates @p dt into each entity's @ref AnimationState::timer and steps
 * @ref AnimationState::currentFrame forward whenever the accumulated time
 * exceeds one frame interval (1.0 / fps). Multiple frames may advance in a
 * single call if dt is large (e.g. after a hitch), keeping animations
 * time-accurate rather than frame-rate dependent.
 *
 * Non-looping animations are frozen on their final frame until an external
 * system (e.g. @ref PlayerStateSystem) transitions them to a new state.
 *
 * @param reg  The EnTT registry. Queries all entities with @ref AnimationState.
 * @param dt   Delta time in seconds since the last frame.
 */
inline void AnimationSystem(entt::registry& reg, float dt) {
    auto view = reg.view<AnimationState>();
    view.each([dt](AnimationState& anim) {
        if (!anim.looping && anim.currentFrame == anim.totalFrames - 1)
            return;
        anim.timer += dt;
        float interval = 1.0f / anim.fps;
        while (anim.timer >= interval) {
            anim.currentFrame = (anim.currentFrame + 1) % anim.totalFrames;
            anim.timer -= interval;
        }
    });
}

inline void RenderSystem(entt::registry& reg, SDL_Surface* screen) {
    auto view = reg.view<Transform, Renderable, AnimationState>();
    view.each([&reg, screen](entt::entity          entity,
                             const Transform&      t,
                             const Renderable&     r,
                             const AnimationState& anim) {
        if (r.frames.empty())
            return;

        const SDL_Rect& src = r.frames[anim.currentFrame];

        // Extract the current frame into a temporary surface
        SDL_Surface* frame = SDL_CreateSurface(src.w, src.h, r.sheet->format);
        SDL_SetSurfaceBlendMode(frame, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(r.sheet, &src, frame, nullptr);

        // Flip horizontally if needed
        if (r.flipH) {
            SDL_Surface* flipped = SDL_CreateSurface(src.w, src.h, r.sheet->format);
            SDL_SetSurfaceBlendMode(flipped, SDL_BLENDMODE_BLEND);
            SDL_LockSurface(frame);
            SDL_LockSurface(flipped);
            for (int y = 0; y < src.h; y++) {
                for (int x = 0; x < src.w; x++) {
                    Uint32* srcPx = reinterpret_cast<Uint32*>(
                        static_cast<Uint8*>(frame->pixels) + y * frame->pitch + x * 4);
                    Uint32* dstPx =
                        reinterpret_cast<Uint32*>(static_cast<Uint8*>(flipped->pixels) +
                                                  y * flipped->pitch + (src.w - 1 - x) * 4);
                    *dstPx = *srcPx;
                }
            }
            SDL_UnlockSurface(frame);
            SDL_UnlockSurface(flipped);
            SDL_DestroySurface(frame);
            frame = flipped;
        }

        // Rotate based on gravity direction if this entity has GravityState
        auto* g = reg.try_get<GravityState>(entity);
        if (g && g->active) {
            SDL_Surface* rotated = nullptr;
            switch (g->direction) {
                case GravityDir::DOWN:
                    rotated = nullptr;
                    break;
                case GravityDir::UP:
                    rotated = RotateSurface180(frame);
                    break;
                case GravityDir::RIGHT:
                    rotated = RotateSurface90CCW(frame);
                    break;
                case GravityDir::LEFT:
                    rotated = RotateSurface90CW(frame);
                    break;
            }
            if (rotated) {
                SDL_DestroySurface(frame);
                frame = rotated;
            }
        }

        // When rotated, sprite dimensions swap so we need to adjust position
        // to keep the player visually flush against the wall
        int renderX = static_cast<int>(t.x);
        int renderY = static_cast<int>(t.y);
        if (g && g->active) {
            switch (g->direction) {
                case GravityDir::RIGHT:
                    // Sprite is now wider than collider, shift left by the difference
                    renderX = static_cast<int>(t.x) - (frame->w - PLAYER_SPRITE_WIDTH);
                    break;
                case GravityDir::UP:
                    // Sprite is now taller, shift up by the difference
                    renderY = static_cast<int>(t.y) - (frame->h - PLAYER_SPRITE_HEIGHT);
                    break;
                default:
                    break;
            }
        }
        // Flash red when hurt — toggle on/off at 10Hz using remaining invincibility time
        auto* inv = reg.try_get<InvincibilityTimer>(entity);
        if (inv && inv->isInvincible) {
            constexpr float flashRate = 10.0f;
            bool            tintOn = static_cast<int>(inv->remaining * flashRate) % 2 == 0;
            if (tintOn)
                SDL_SetSurfaceColorMod(frame, 255, 0, 0);
        }

        SDL_Rect dest = {renderX, renderY, frame->w, frame->h};
        SDL_BlitSurface(frame, nullptr, screen, &dest);
        SDL_SetSurfaceColorMod(frame, 255, 255, 255); // reset mod before destroy
        SDL_DestroySurface(frame);
    });
}

inline void InputSystem(entt::registry& reg, SDL_Event& e) {
    // WASD only works in free mode
    auto view = reg.view<PlayerTag, Velocity, Renderable, GravityState>();
    view.each([&e](Velocity& v, Renderable& r, GravityState& g) {
        // Horizontal movement always works
        // On the top wall the sprite is rotated 180 so left/right facing is inverted
        bool invertFlip = g.active && g.direction == GravityDir::UP;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_A:
                    v.dx    = -v.speed;
                    r.flipH = !invertFlip;
                    break;
                case SDLK_D:
                    v.dx    = v.speed;
                    r.flipH = invertFlip;
                    break;
                case SDLK_LCTRL:
                    g.isCrouching = true;
                    break;
            }
        }

        if (!g.active) {
            // Vertical movement only in free mode
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_W:
                        v.dy = -v.speed;
                        break;
                    case SDLK_S:
                        v.dy = v.speed;
                        break;
                }
            }
        } else {
            // Space jump only in gravity mode when grounded
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_SPACE) {
                if (g.isGrounded) {
                    // Jump always pushes away from the gravity wall
                    g.velocity   = -JUMP_FORCE;
                    g.isGrounded = false;
                    g.jumpHeld   = true;
                }
            }
            if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_SPACE) {
                g.jumpHeld = false;
            }
            if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_LCTRL) {
                g.isCrouching = false;
            }
        }
    });
}

inline void CenterPullSystem(entt::registry& reg, float dt, int windowW, int windowH) {
    auto view = reg.view<Transform, Velocity, GravityState, PlayerTag>();
    view.each([dt, windowW, windowH](Transform& t, Velocity& v, GravityState& g) {
        if (g.active)
            return; // only runs in free mode

        float centerX = windowW / 2.0f;
        float centerY = windowH / 2.0f;

        float dx   = centerX - t.x;
        float dy   = centerY - t.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        // Only pull if far from center
        if (dist > 5.0f) {
            constexpr float pullSpeed = 200.0f;
            float           norm      = pullSpeed / dist;
            t.x += dx * norm * dt;
            t.y += dy * norm * dt;
        }
    });
}

inline void BoundsSystem(entt::registry& reg, int windowW, int windowH) {
    auto view = reg.view<Transform, Collider, GravityState, Velocity, PlayerTag>();
    view.each(
        [windowW, windowH](Transform& t, const Collider& c, GravityState& g, Velocity& v) {
            auto activate = [&](GravityDir dir) {
                // If already grounded on this same wall, don't do anything
                if (g.active && g.isGrounded && g.direction == dir)
                    return;
                // Reset timer whenever we switch to a new wall
                g.timer      = 0.0f;
                g.active     = true;
                g.isGrounded = false;
                g.velocity   = 0.0f;
                g.direction  = dir;
                v.dx         = 0.0f;
                v.dy         = 0.0f;
            };

            // Left wall — gravity pulls left
            if (t.x < 0.0f) {
                t.x = 0.0f;
                activate(GravityDir::LEFT);
            }
            // Right wall — gravity pulls right
            if (t.x + c.w > windowW) {
                t.x = static_cast<float>(windowW - c.w);
                activate(GravityDir::RIGHT);
            }
            // Top wall — gravity pulls up
            if (t.y < 0.0f) {
                t.y = 0.0f;
                activate(GravityDir::UP);
            }
            // Bottom wall — gravity pulls down
            if (t.y + c.h > windowH) {
                t.y = static_cast<float>(windowH - c.h);
                activate(GravityDir::DOWN);
            }

            // Check if player has landed on their gravity wall
            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= windowH) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::LEFT:
                        if (t.x <= 0.0f) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                    case GravityDir::RIGHT:
                        if (t.x + c.w >= windowW) {
                            g.isGrounded = true;
                            g.velocity   = 0.0f;
                        }
                        break;
                }
            }
        });
}

/**
 * @brief Determines and transitions the player's active animation based on current game
 * state.
 *
 * Runs every frame after movement and physics updates. Evaluates the player's
 * @ref InvincibilityTimer, @ref GravityState, and @ref Velocity to select the
 * appropriate animation from the entity's @ref AnimationSet, then updates
 * @ref Renderable and @ref AnimationState only when the animation actually changes.
 *
 * @par Animation Priority (highest to lowest)
 * -# **Hurt** — plays when the player is invincible after taking damage (non-looping)
 * -# **Jump** — plays while airborne in gravity mode
 * -# **Walk** — plays when |dx| or |dy| exceeds 1.0 px/s
 * -# **Idle** — fallback when stationary
 *
 * @note Animation state is only reset when @ref AnimationID changes, preventing
 *       per-frame resets that would stall the animation at frame 0.
 *
 * @param reg  The EnTT registry. Queries entities with
 *             PlayerTag, Velocity, GravityState, Renderable,
 *             AnimationState, AnimationSet, and InvincibilityTimer.
 */
inline void PlayerStateSystem(entt::registry& reg) {
    auto view = reg.view<PlayerTag,
                         Velocity,
                         GravityState,
                         Renderable,
                         AnimationState,
                         AnimationSet,
                         InvincibilityTimer>();

    view.each([](const Velocity&           v,
                 const GravityState&       g,
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
        } else if (isMoving) {
            newFrames  = &set.walk;
            newFps     = 12.0f;
            newLooping = true;
            newID      = AnimationID::WALK;
        } else if (g.isCrouching) {
            newFrames  = &set.duck;
            newFps     = 8.0f;
            newLooping = true;
            newID      = AnimationID::DUCK;
        } else {
            newFrames  = &set.idle;
            newFps     = 8.0f;
            newLooping = true;
            newID      = AnimationID::IDLE;
        }

        if (newFrames && anim.currentAnim != newID) {
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

inline void CollisionSystem(entt::registry& reg, float dt, bool& gameOver) {
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
            bool overlapX = pt.x < et.x + ec.w && pt.x + pc.w > et.x;
            bool overlapY = pt.y < et.y + ec.h && pt.y + pc.h > et.y;

            if (overlapX && overlapY) {
                health.current -= PLAYER_HIT_DAMAGE;
                inv.isInvincible = true;
                g.isGrounded     = true;
                g.active         = true;
                inv.remaining    = inv.duration;
                std::print("Player hit! Health: {}\n", health.current);

                if (health.current <= 0.0f) {
                    health.current = 0.0f;
                    gameOver       = true;
                }
            }
        });
    });
}
inline void HUDSystem(entt::registry& reg,
                      SDL_Surface*    screen,
                      int             windowW,
                      Text*           healthText) {
    auto view = reg.view<PlayerTag, Health>();
    view.each([&](const Health& h) {
        constexpr int barW       = 200;
        constexpr int barH       = 15;
        const int     barX       = windowW - barW - 20; // is the -20 essentially padding?
        constexpr int barY       = 20;
        SDL_Rect      background = {barX, barY, barW, barH};
        int      fillW = static_cast<int>(barW * (h.current / h.max)); // what is this for?
        SDL_Rect foreground = {
            barX, barY, fillW, barH}; // this is the loader that sits over the background?

        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(
            screen->format); // what does this do and why is it needed?
        // Dark Gray background
        SDL_FillSurfaceRect(screen, &background, SDL_MapRGB(fmt, nullptr, 50, 50, 50));
        // Green/Yellow/red fill based on health percentage
        float pct = h.current / h.max;
        Uint8 r   = static_cast<Uint8>(255 * (1.0f - pct)); // more red as health drops
        Uint8 g   = static_cast<Uint8>(255 * pct);          // more green as health increases
        SDL_FillSurfaceRect(screen, &foreground, SDL_MapRGB(fmt, nullptr, r, g, 0));
        std::string label = std::to_string(static_cast<int>(h.current)) + " / " +
                            std::to_string(static_cast<int>(h.max));
        healthText->SetPosition(barX, barY - 20);
        healthText->CreateSurface(label);
        healthText->Render(screen);
    });
}
