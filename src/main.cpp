/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Components.hpp"
#include "Image.hpp"
#include "Rectangle.hpp"
#include "ScaledText.hpp"
#include "SpriteSheet.hpp"
#include "Systems.hpp"
#include "Text.hpp"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdlib>
#include <ctime>
#include <entt/entt.hpp>
#include <print>

entt::entity SpawnPlayer(entt::registry&              reg,
                          int                          windowW,
                          int                          windowH,
                          SDL_Surface*                 sheet,
                          const std::vector<SDL_Rect>& frames) {
    auto player = reg.create();
    reg.emplace<Transform>(player,
                           (float)(windowW / 2 - 33),
                           (float)(windowH / 2 - 46));
    reg.emplace<Velocity>(player);
    reg.emplace<AnimationState>(player, 0, (int)frames.size(), 0.0f, 12.0f, true);
    reg.emplace<Renderable>(player, sheet, frames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
    reg.emplace<InvincibilityTimer>(player);
    return player;
}

void SpawnEnemies(entt::registry&              reg,
                  int                          windowW,
                  int                          windowH,
                  SDL_Surface*                 sheet,
                  const std::vector<SDL_Rect>& frames) {
    for (int i = 0; i < 15; ++i) {
        float xPos  = rand() % (windowW - 100);
        float yPos  = rand() % (windowH - 100);
        auto  enemy = reg.create();
        reg.emplace<Transform>(enemy, xPos, yPos);
        reg.emplace<Velocity>(enemy);
        reg.emplace<AnimationState>(enemy, 0, (int)frames.size(), 0.0f, 7.0f, true);
        reg.emplace<Renderable>(enemy, sheet, frames, false);
        reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
    }
}

void DoRetry(entt::registry& reg, bool& gameOver, int windowW, int windowH,
             SDL_Surface* playerSheet, const std::vector<SDL_Rect>& walkFrames,
             SDL_Surface* enemySheet, const std::vector<SDL_Rect>& enemyFrames) {
    reg.clear();
    SpawnPlayer(reg, windowW, windowH, playerSheet, walkFrames);
    SpawnEnemies(reg, windowW, windowH, enemySheet, enemyFrames);
    gameOver = false;
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    if (!TTF_Init()) {
        std::print("Error initializing SDL_ttf: %s\n", SDL_GetError());
        return 1;
    }
    srand(static_cast<unsigned int>(time(nullptr)));

    Window         GameWindow;
    entt::registry reg;
    bool           gameOver = false;

    Image BackgroundImg{"game_assets/base_pack/bg_castle.png", nullptr, FitMode::COVER};

    Text       LocationText{"You are in space!!", 20, 20};
    ScaledText ScaledExample{"Game on!", 0, 200, GameWindow.GetWidth()};
    Text       ActionText{"Float Around", {100, 100, 100, 0}, 20, 80, 20};
    Text       GameOverText{"Game Over!",
                      {255, 0, 0, 255},
                      GameWindow.GetWidth() / 2 - 100,
                      GameWindow.GetHeight() / 2 - 60,
                      64};
    Text       RetryKeyText{"Press R to Retry",
                      {200, 200, 200, 255},
                      GameWindow.GetWidth() / 2 - 100,
                      GameWindow.GetHeight() / 2 + 110,
                      24};
    Text       RetryBtnText{"Retry",
                      {0, 0, 0, 255},
                      GameWindow.GetWidth() / 2 - 28,
                      GameWindow.GetHeight() / 2 + 22,
                      32};

    // Retry button rectangle — centered below game over text
    SDL_Rect      retryBtnRect{GameWindow.GetWidth() / 2 - 75,
                               GameWindow.GetHeight() / 2 + 10,
                               150, 55};
    Rectangle     RetryButton{retryBtnRect};
    RetryButton.SetColor({255, 255, 255, 255});
    RetryButton.SetHoverColor({180, 180, 180, 255});

    // Asset loading — happens once, survives retries
    SpriteSheet           playerSheet("game_assets/base_pack/Player/p1_spritesheet.png",
                                      "game_assets/base_pack/Player/p1_spritesheet.txt");
    std::vector<SDL_Rect> walkFrames = playerSheet.GetAnimation("p1_walk");

    SpriteSheet           enemySheet("game_assets/base_pack/Enemies/enemies_spritesheet.png",
                                     "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    std::vector<SDL_Rect> enemyWalkFrames = enemySheet.GetAnimation("slimeWalk");

    // Initial spawn
    SpawnPlayer(reg, GameWindow.GetWidth(), GameWindow.GetHeight(),
                playerSheet.GetSurface(), walkFrames);
    SpawnEnemies(reg, GameWindow.GetWidth(), GameWindow.GetHeight(),
                 enemySheet.GetSurface(), enemyWalkFrames);

    UI        UIManager;
    SDL_Event E;

    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 lastTime  = SDL_GetPerformanceCounter();

    while (true) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  deltaTime   = (float)(currentTime - lastTime) / frequency;
        lastTime           = currentTime;

        while (SDL_PollEvent(&E)) {
            UIManager.HandleEvent(E);

            if (!gameOver) {
                InputSystem(reg, E);
            } else {
                // R key retry
                if (E.type == SDL_EVENT_KEY_DOWN && E.key.key == SDLK_R) {
                    DoRetry(reg, gameOver,
                            GameWindow.GetWidth(), GameWindow.GetHeight(),
                            playerSheet.GetSurface(), walkFrames,
                            enemySheet.GetSurface(), enemyWalkFrames);
                }

                // Button click retry — check manually since Rectangle::OnLeftClick
                // needs subclassing for callbacks; this keeps it simple
                if (E.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    E.button.button == SDL_BUTTON_LEFT) {
                    int mx = (int)E.button.x;
                    int my = (int)E.button.y;
                    if (mx >= retryBtnRect.x &&
                        mx <= retryBtnRect.x + retryBtnRect.w &&
                        my >= retryBtnRect.y &&
                        my <= retryBtnRect.y + retryBtnRect.h) {
                        DoRetry(reg, gameOver,
                                GameWindow.GetWidth(), GameWindow.GetHeight(),
                                playerSheet.GetSurface(), walkFrames,
                                enemySheet.GetSurface(), enemyWalkFrames);
                    }
                }

                RetryButton.HandleEvent(E);
            }

            if (E.type == SDL_EVENT_QUIT) {
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        if (!gameOver) {
            MovementSystem(reg, deltaTime);
            AnimationSystem(reg, deltaTime);
            CollisionSystem(reg, deltaTime, gameOver);
        }

        GameWindow.Render();
        BackgroundImg.Render(GameWindow.GetSurface());

        if (gameOver) {
            GameOverText.Render(GameWindow.GetSurface());
            RetryButton.Render(GameWindow.GetSurface());
            RetryBtnText.Render(GameWindow.GetSurface());
            RetryKeyText.Render(GameWindow.GetSurface());
        } else {
            LocationText.Render(GameWindow.GetSurface());
            ScaledExample.Render(GameWindow.GetSurface());
            ActionText.Render(GameWindow.GetSurface());
            RenderSystem(reg, GameWindow.GetSurface());
        }

        GameWindow.Update();
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
