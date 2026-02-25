#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <print>

void MovementSystem(entt::registry& reg, float dt) {
    auto view = reg.view<Transform, Velocity>();
    view.each([dt](Transform& t, Velocity& v) {
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
    auto view = reg.view<PlayerTag, Velocity, Renderable>();
    view.each([&e](Velocity& v, Renderable& r) {
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_W: v.dy = -v.speed; break;
                case SDLK_S: v.dy =  v.speed; break;
                case SDLK_A: v.dx = -v.speed; r.flipH = true;  break;
                case SDLK_D: v.dx =  v.speed; r.flipH = false; break;
            }
        }
        if (e.type == SDL_EVENT_KEY_UP) {
            switch (e.key.key) {
                case SDLK_W: case SDLK_S: v.dy = 0.0f; break;
                case SDLK_A: case SDLK_D: v.dx = 0.0f; break;
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
