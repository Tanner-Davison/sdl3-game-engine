#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <Text.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <string>

inline void HUDSystem(entt::registry& reg,
                      SDL_Surface*    screen,
                      int             windowW,
                      Text*           healthText,
                      Text*           gravityText) {
    auto view = reg.view<PlayerTag, Health, GravityState>();
    view.each([&](const Health& h, const GravityState& g) {
        constexpr int barW       = 200;
        constexpr int barH       = 15;
        const int     barX       = windowW - barW - 20;
        constexpr int barY       = 20;
        SDL_Rect      background = {barX, barY, barW, barH};
        int           fillW      = static_cast<int>(barW * (h.current / h.max));
        SDL_Rect      foreground = {barX, barY, fillW, barH};

        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
        SDL_FillSurfaceRect(screen, &background, SDL_MapRGB(fmt, nullptr, 50, 50, 50));

        float pct     = h.current / h.max;
        Uint8 red     = static_cast<Uint8>(255 * (1.0f - pct));
        Uint8 green   = static_cast<Uint8>(255 * pct);
        SDL_FillSurfaceRect(screen, &foreground, SDL_MapRGB(fmt, nullptr, red, green, 0));

        std::string label = std::to_string(static_cast<int>(h.current)) + " / " +
                            std::to_string(static_cast<int>(h.max));
        healthText->SetPosition(barX, barY - 20);
        healthText->CreateSurface(label);
        healthText->Render(screen);

        // Zero gravity countdown
        if (g.punishmentTimer > 0.0f) {
            int         secs = static_cast<int>(std::ceil(g.punishmentTimer));
            std::string msg  = "Zero Gravity Activated for " + std::to_string(secs) + " s";
            gravityText->SetPosition(windowW / 2 - 160, 20);
            gravityText->CreateSurface(msg);
            gravityText->Render(screen);
        }
    });
}
