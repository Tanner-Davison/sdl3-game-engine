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
                      Text*           gravityText,
                      Text*           coinText,
                      int             coinCount,
                      Text*           stompText,
                      int             stompCount) {
    // Track previous values so we only rebuild text surfaces when something changes
    static int   prevHealth   = -1;
    static int   prevCoin     = -1;
    static int   prevStomp    = -1;
    static int   prevGravSecs = -1;

    auto view = reg.view<PlayerTag, Health, GravityState>();
    view.each([&](const Health& h, const GravityState& g) {
        constexpr int barW = 200;
        constexpr int barH = 15;
        const int     barX = windowW - barW - 20;
        constexpr int barY = 20;

        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);

        // Health bar background + fill
        SDL_Rect background = {barX, barY, barW, barH};
        int      fillW      = static_cast<int>(barW * (h.current / h.max));
        SDL_Rect foreground = {barX, barY, fillW, barH};
        SDL_FillSurfaceRect(screen, &background, SDL_MapRGB(fmt, nullptr, 50, 50, 50));
        float pct   = h.current / h.max;
        Uint8 red   = static_cast<Uint8>(255 * (1.0f - pct));
        Uint8 green = static_cast<Uint8>(255 * pct);
        SDL_FillSurfaceRect(screen, &foreground, SDL_MapRGB(fmt, nullptr, red, green, 0));

        // Health label — only rebuild surface when health value changes
        int curHealth = static_cast<int>(h.current);
        if (curHealth != prevHealth) {
            std::string label = std::to_string(curHealth) + " / " +
                                std::to_string(static_cast<int>(h.max));
            healthText->SetPosition(barX, barY - 20);
            healthText->CreateSurface(label);
            prevHealth = curHealth;
        }
        healthText->Render(screen);

        // Coin count — only rebuild when count changes
        if (coinText) {
            if (coinCount != prevCoin) {
                coinText->SetPosition(barX, barY + barH + 10);
                coinText->CreateSurface("Gold Collected: " + std::to_string(coinCount));
                prevCoin = coinCount;
            }
            coinText->Render(screen);
        }

        // Stomp count — only rebuild when count changes
        if (stompText) {
            if (stompCount != prevStomp) {
                stompText->SetPosition(barX, barY + barH + 30);
                stompText->CreateSurface("Enemies Stomped: " + std::to_string(stompCount));
                prevStomp = stompCount;
            }
            stompText->Render(screen);
        }

        // Zero gravity countdown — rebuild once per second (secs value changes)
        if (g.punishmentTimer > 0.0f && gravityText) {
            int secs = static_cast<int>(std::ceil(g.punishmentTimer));
            if (secs != prevGravSecs) {
                gravityText->SetPosition(windowW / 2 - 160, 20);
                gravityText->CreateSurface("Zero Gravity Activated for " +
                                           std::to_string(secs) + " s");
                prevGravSecs = secs;
            }
            gravityText->Render(screen);
        } else {
            prevGravSecs = -1; // reset so it rebuilds next time it activates
        }
    });
}
