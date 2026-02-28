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
                    if (!g.isCrouching) {
                        v.dx    = -v.speed;
                        // Only set flip for horizontal movement on horizontal-gravity walls
                        if (g.direction == GravityDir::DOWN || g.direction == GravityDir::UP)
                            r.flipH = !invertFlip;
                    }
                    break;
                case SDLK_D:
                    if (!g.isCrouching) {
                        v.dx    = v.speed;
                        if (g.direction == GravityDir::DOWN || g.direction == GravityDir::UP)
                            r.flipH = invertFlip;
                    }
                    break;
                case SDLK_W:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            // 90CW rotation: flipH=true makes sprite face up the wall
                            r.flipH = true;
                        } else if (g.direction == GravityDir::RIGHT) {
                            // 90CCW rotation: flipH=false makes sprite face up the wall
                            r.flipH = false;
                        }
                    }
                    break;
                case SDLK_S:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            // 90CW rotation: flipH=false makes sprite face down the wall
                            r.flipH = false;
                        } else if (g.direction == GravityDir::RIGHT) {
                            // 90CCW rotation: flipH=true makes sprite face down the wall
                            r.flipH = true;
                        }
                    }
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
