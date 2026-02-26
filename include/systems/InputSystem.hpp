#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

inline void InputSystem(entt::registry& reg, SDL_Event& e) {
    auto view = reg.view<PlayerTag, Velocity, Renderable, GravityState>();
    view.each([&e](Velocity& v, Renderable& r, GravityState& g) {
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

        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_LCTRL)
            g.isCrouching = false;

        if (!g.active) {
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
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_SPACE) {
                if (g.isGrounded) {
                    g.velocity   = -JUMP_FORCE;
                    g.isGrounded = false;
                    g.jumpHeld   = true;
                }
            }
            if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_SPACE)
                g.jumpHeld = false;
        }
    });
}
