/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Components.hpp"
#include "Image.hpp"
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

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    if (!TTF_Init()) {
        std::print("Error initializing SDL_ttf: %s\n", SDL_GetError());
        return 1;
    }
    srand(static_cast<unsigned int>(time(nullptr)));
    Window         GameWindow;
    entt::registry reg;

    Image BackgroundImg{"game_assets/base_pack/bg_castle.png", nullptr, FitMode::COVER};

    Text       LocationText{"You are in space!!", 20, 20};
    ScaledText ScaledExample{"Game on!", 0, 200, GameWindow.GetWidth()};
    Text       ActionText{"Float Around", {100, 100, 100, 0}, 20, 80, 20};

    // Player
    SpriteSheet           playerSheet("game_assets/base_pack/Player/p1_spritesheet.png",
                                      "game_assets/base_pack/Player/p1_spritesheet.txt");
    std::vector<SDL_Rect> walkFrames = playerSheet.GetAnimation("p1_walk");

    auto player = reg.create();

    reg.emplace<Transform>(player,
                           (float)(GameWindow.GetWidth() / 2 - 33),
                           (float)(GameWindow.GetHeight() / 2 - 46));
    reg.emplace<Velocity>(player);
    reg.emplace<AnimationState>(player, 0, (int)walkFrames.size(), 0.0f, 12.0f, true);
    reg.emplace<Renderable>(player, playerSheet.GetSurface(), walkFrames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(
        player, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT); // Components.hpp
    reg.emplace<InvincibilityTimer>(player);

    // Enemies
    SpriteSheet enemySheet("game_assets/base_pack/Enemies/enemies_spritesheet.png",
                           "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    std::vector<SDL_Rect> enemyWalkFrames = enemySheet.GetAnimation("slimeWalk");

    for (int i = 0; i < 15; ++i) {
        float xPos = rand() % (GameWindow.GetWidth() - 100);
        float yPos = rand() % (GameWindow.GetHeight() - 100);

        auto enemy = reg.create();
        reg.emplace<Transform>(enemy, xPos, yPos);
        reg.emplace<Velocity>(enemy);
        reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
        reg.emplace<Renderable>(enemy, enemySheet.GetSurface(), enemyWalkFrames, false);
        reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
    }

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
            InputSystem(reg, E);
            if (E.type == SDL_EVENT_QUIT) {
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        MovementSystem(reg, deltaTime);
        AnimationSystem(reg, deltaTime);
        CollisionSystem(reg, deltaTime);

        GameWindow.Render();
        BackgroundImg.Render(GameWindow.GetSurface());
        LocationText.Render(GameWindow.GetSurface());
        ScaledExample.Render(GameWindow.GetSurface());
        ActionText.Render(GameWindow.GetSurface());
        RenderSystem(reg, GameWindow.GetSurface());
        GameWindow.Update();
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
