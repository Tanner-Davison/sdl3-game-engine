#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <cmath>
#include <print>

void MovementSystem(entt::registry& reg, float dt) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag>();
    playerView.each([dt, keys](Transform& t, Velocity& v, GravityState& g) {
        if (!g.active) {
            // Free mode — friction on key release
            bool anyKeyHeld = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                              keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
            if (!anyKeyHeld) {
                constexpr float friction = 3.0f;
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f) v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f) v.dy = 0.0f;
            }
            t.x += v.dx * dt;
            t.y += v.dy * dt;
        } else {
            // Gravity mode — movement perpendicular to gravity axis still works
            v.dx = 0.0f;
            v.dy = 0.0f;

            bool horizKey = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
            bool vertKey  = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S];
            constexpr float friction = 3.0f;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    // Horizontal walking still works
                    if (keys[SDL_SCANCODE_A]) v.dx = -v.speed;
                    if (keys[SDL_SCANCODE_D]) v.dx =  v.speed;
                    if (!horizKey) {
                        v.dx -= v.dx * friction * dt;
                        if (std::abs(v.dx) < 0.5f) v.dx = 0.0f;
                    }
                    t.x += v.dx * dt;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    // Vertical walking still works
                    if (keys[SDL_SCANCODE_W]) v.dy = -v.speed;
                    if (keys[SDL_SCANCODE_S]) v.dy =  v.speed;
                    if (!vertKey) {
                        v.dy -= v.dy * friction * dt;
                        if (std::abs(v.dy) < 0.5f) v.dy = 0.0f;
                    }
                    t.y += v.dy * dt;
                    break;
                }
            }

            // Apply gravity along the gravity axis
            if (!g.isGrounded) {
                g.velocity += GRAVITY_FORCE * dt;
                if (g.velocity > MAX_FALL_SPEED) g.velocity = MAX_FALL_SPEED;
            }

            // Hold space boosts away from wall
            if (g.jumpHeld && !g.isGrounded && g.velocity < 0.0f) {
                g.velocity -= JUMP_FORCE * 0.5f * dt;
            }

            // Apply gravity velocity in the correct direction
            switch (g.direction) {
                case GravityDir::DOWN:  t.y += g.velocity * dt; break;
                case GravityDir::UP:    t.y -= g.velocity * dt; break;
                case GravityDir::LEFT:  t.x -= g.velocity * dt; break;
                case GravityDir::RIGHT: t.x += g.velocity * dt; break;
            }

            // Timer
            g.timer += dt;
            if (g.timer >= GRAVITY_DURATION) {
                g.active     = false;
                g.timer      = 0.0f;
                g.velocity   = 0.0f;
                g.isGrounded = false;
            }
        }
    });

    // Enemies move without friction
    auto enemyView = reg.view<Transform, Velocity>(entt::exclude<PlayerTag>);
    enemyView.each([dt](Transform& t, Velocity& v) {
        t.x += v.dx * dt;
        t.y += v.dy * dt;
    });
};

void AnimationSystem(entt::registry& reg, float dt) {
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

void RenderSystem(entt::registry& reg, SDL_Surface* screen) {
    auto view = reg.view<Transform, Renderable, AnimationState>();
    view.each([screen](const Transform& t, const Renderable& r, const AnimationState& anim) {
        if (r.frames.empty())
            return;

        const SDL_Rect& src  = r.frames[anim.currentFrame];
        SDL_Rect        dest = {(int)t.x, (int)t.y, src.w, src.h};

        if (r.flipH) {
            SDL_Surface* flipped = SDL_CreateSurface(src.w, src.h, r.sheet->format);
            SDL_SetSurfaceBlendMode(flipped, SDL_BLENDMODE_BLEND);
            SDL_LockSurface(r.sheet);
            SDL_LockSurface(flipped);
            for (int y = 0; y < src.h; y++) {
                for (int x = 0; x < src.w; x++) {
                    Uint32* srcPx =
                        (Uint32*)((Uint8*)r.sheet->pixels + (src.y + y) * r.sheet->pitch +
                                  (src.x + x) * 4);
                    Uint32* dstPx = (Uint32*)((Uint8*)flipped->pixels + y * flipped->pitch +
                                              (src.w - 1 - x) * 4);
                    *dstPx        = *srcPx;
                }
            }
            SDL_UnlockSurface(r.sheet);
            SDL_UnlockSurface(flipped);
            SDL_Rect flippedSrc = {0, 0, src.w, src.h};
            SDL_BlitSurface(flipped, &flippedSrc, screen, &dest);
        } else {
            SDL_BlitSurface(r.sheet, &src, screen, &dest);
        }
    });
}

void InputSystem(entt::registry& reg, SDL_Event& e) {
    // WASD only works in free mode
    auto view = reg.view<PlayerTag, Velocity, Renderable, GravityState>();
    view.each([&e](Velocity& v, Renderable& r, GravityState& g) {
        // Horizontal movement always works
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_A: v.dx = -v.speed; r.flipH = true;  break;
                case SDLK_D: v.dx =  v.speed; r.flipH = false; break;
            }
        }

        if (!g.active) {
            // Vertical movement only in free mode
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_W: v.dy = -v.speed; break;
                    case SDLK_S: v.dy =  v.speed; break;
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
        }
    });
}

void BoundsSystem(entt::registry& reg, int windowW, int windowH) {
    auto view = reg.view<Transform, Collider, GravityState, Velocity, PlayerTag>();
    view.each([windowW, windowH](Transform& t, const Collider& c,
                                  GravityState& g, Velocity& v) {
        auto activate = [&](GravityDir dir) {
            if (!g.active) {
                g.active    = true;
                g.timer     = 0.0f;
                g.isGrounded = false;
                g.velocity  = 0.0f;
                g.direction = dir;
                v.dx        = 0.0f;
                v.dy        = 0.0f;
            }
        };

        // Left wall — gravity pulls left
        if (t.x < 0.0f) {
            t.x = 0.0f;
            activate(GravityDir::LEFT);
        }
        // Right wall — gravity pulls right
        if (t.x + c.w > windowW) {
            t.x = (float)(windowW - c.w);
            activate(GravityDir::RIGHT);
        }
        // Top wall — gravity pulls up
        if (t.y < 0.0f) {
            t.y = 0.0f;
            activate(GravityDir::UP);
        }
        // Bottom wall — gravity pulls down
        if (t.y + c.h > windowH) {
            t.y = (float)(windowH - c.h);
            activate(GravityDir::DOWN);
        }

        // Check if player has landed on their gravity wall
        if (g.active) {
            switch (g.direction) {
                case GravityDir::DOWN:
                    if (t.y + c.h >= windowH) { g.isGrounded = true; g.velocity = 0.0f; }
                    break;
                case GravityDir::UP:
                    if (t.y <= 0.0f) { g.isGrounded = true; g.velocity = 0.0f; }
                    break;
                case GravityDir::LEFT:
                    if (t.x <= 0.0f) { g.isGrounded = true; g.velocity = 0.0f; }
                    break;
                case GravityDir::RIGHT:
                    if (t.x + c.w >= windowW) { g.isGrounded = true; g.velocity = 0.0f; }
                    break;
            }
        }
    });
}

void CollisionSystem(entt::registry& reg, float dt, bool& gameOver) {
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

    auto playerView = reg.view<PlayerTag, Transform, Collider, Health, InvincibilityTimer>();
    auto enemyView  = reg.view<Transform, Collider>(entt::exclude<PlayerTag>);

    playerView.each([&](const Transform& pt, const Collider& pc,
                        Health& health, InvincibilityTimer& inv) {
        if (inv.isInvincible) return;

        enemyView.each([&](const Transform& et, const Collider& ec) {
            bool overlapX = pt.x < et.x + ec.w && pt.x + pc.w > et.x;
            bool overlapY = pt.y < et.y + ec.h && pt.y + pc.h > et.y;

            if (overlapX && overlapY) {
                health.current -= PLAYER_HIT_DAMAGE;
                inv.isInvincible = true;
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
