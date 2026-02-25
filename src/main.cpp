/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Button.hpp"
#include "Image.hpp"
#include "ScaledText.hpp"
#include "Sprite.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
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

    Window GameWindow;

    Image BackgroundImg{
        "game_assets/base_pack/bg_castle.png", nullptr, FitMode::COVER};

    Text LocationText{"You are in space!!", 20, 20};

    ScaledText ScaledExample{
        "How big is this going to be needed? what if i kept on adding text ",
        0,
        200,
        GameWindow.GetWidth()};

    Text ActionText{"Float Around", {100, 100, 100, 0}, 20, 80, 20};

    // Player sprite
    SpriteSheet playerSheet("game_assets/base_pack/Player/p1_spritesheet.png",
                            "game_assets/base_pack/Player/p1_spritesheet.txt");

    std::vector<SDL_Rect> walkFrames = playerSheet.GetAnimation("p1_walk");

    Sprite PlayerSprite(playerSheet.GetSurface(),
                        walkFrames,
                        nullptr,
                        GameWindow.GetWidth() / 2 - 33,
                        GameWindow.GetHeight() / 2 - 46);

    PlayerSprite.SetAnimationSpeed(12.0f);
    PlayerSprite.SetLooping(true);

    SpriteSheet enemySheet(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    std::vector<SDL_Rect> enemyWalkFrames =
        enemySheet.GetAnimation("slimeWalk");

    std::vector<std::unique_ptr<Sprite>> Enemies;

    for (int i = 0; i < 15; ++i) {
        float xPos = rand() % (GameWindow.GetWidth() - 100);
        float yPos = rand() % (GameWindow.GetHeight() - 100);

        Enemies.push_back(std::make_unique<Sprite>(
            enemySheet.GetSurface(), enemyWalkFrames, nullptr, xPos, yPos));

        Enemies.back()->SetAnimationSpeed(7.0f);
        Enemies.back()->SetMoveSpeed(20.0f);
        Enemies.back()->SetLooping(true);
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
            PlayerSprite.HandleEvent(E);
            for (auto& sprite : Enemies) {
                sprite->HandleEvent(E);
            }
            if (E.type == SDL_EVENT_QUIT) {
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        PlayerSprite.Update(deltaTime);
        for (auto& enemy : Enemies) {
            enemy->Update(deltaTime);
        }

        GameWindow.Render();
        BackgroundImg.Render(GameWindow.GetSurface());
        LocationText.Render(GameWindow.GetSurface());
        ScaledExample.Render(GameWindow.GetSurface());
        ActionText.Render(GameWindow.GetSurface());

        for (auto& enemy : Enemies) {
            enemy->Render(GameWindow.GetSurface());
        }
        PlayerSprite.Render(GameWindow.GetSurface());

        GameWindow.Update();
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
