#pragma once
#include "Components.hpp"
#include "Image.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Systems.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <entt/entt.hpp>
#include <memory>
#include <print>

class GameScene : public Scene {
  public:
    void Load(Window& window) override {
        mWindow  = &window;
        gameOver = false;

        playerSheet = std::make_unique<SpriteSheet>(
            "game_assets/base_pack/Player/p1_spritesheet.png",
            "game_assets/base_pack/Player/p1_spritesheet.txt");
        walkFrames = playerSheet->GetAnimation("p1_walk");

        enemySheet = std::make_unique<SpriteSheet>(
            "game_assets/base_pack/Enemies/enemies_spritesheet.png",
            "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
        enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

        background   = std::make_unique<Image>("game_assets/base_pack/bg_castle.png",
                                               nullptr, FitMode::COVER);
        locationText = std::make_unique<Text>("You are in space!!", 20, 20);
        actionText   = std::make_unique<Text>("Float Around",
                                              SDL_Color{100, 100, 100, 0}, 20, 80, 20);

        gameOverText = std::make_unique<Text>("Game Over!", SDL_Color{255, 0, 0, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 - 60, 64);
        retryBtnText = std::make_unique<Text>("Retry", SDL_Color{0, 0, 0, 255},
                                              window.GetWidth() / 2 - 28,
                                              window.GetHeight() / 2 + 22, 32);
        retryKeyText = std::make_unique<Text>("Press R to Retry",
                                              SDL_Color{200, 200, 200, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 + 110, 24);

        retryBtnRect = {window.GetWidth() / 2 - 75, window.GetHeight() / 2 + 10, 150, 55};
        retryButton  = std::make_unique<Rectangle>(retryBtnRect);
        retryButton->SetColor({255, 255, 255, 255});
        retryButton->SetHoverColor({180, 180, 180, 255});

        Spawn();
    }

    void Unload() override {
        reg.clear();
        mWindow = nullptr;
    }

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        if (!gameOver) {
            InputSystem(reg, e);
        } else {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) {
                Respawn();
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x;
                int my = (int)e.button.y;
                if (mx >= retryBtnRect.x &&
                    mx <= retryBtnRect.x + retryBtnRect.w &&
                    my >= retryBtnRect.y &&
                    my <= retryBtnRect.y + retryBtnRect.h) {
                    Respawn();
                }
            }
            retryButton->HandleEvent(e);
        }
        return true;
    }

    void Update(float dt) override {
        if (!gameOver) {
            MovementSystem(reg, dt);
            AnimationSystem(reg, dt);
            CollisionSystem(reg, dt, gameOver);
        }
    }

    void Render(Window& window) override {
        window.Render();
        background->Render(window.GetSurface());

        if (gameOver) {
            gameOverText->Render(window.GetSurface());
            retryButton->Render(window.GetSurface());
            retryBtnText->Render(window.GetSurface());
            retryKeyText->Render(window.GetSurface());
        } else {
            locationText->Render(window.GetSurface());
            actionText->Render(window.GetSurface());
            RenderSystem(reg, window.GetSurface());
        }

        window.Update();
    }

  private:
    entt::registry reg;
    bool           gameOver = false;
    Window*        mWindow  = nullptr;

    std::unique_ptr<SpriteSheet> playerSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    std::vector<SDL_Rect>        walkFrames;
    std::vector<SDL_Rect>        enemyWalkFrames;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      locationText;
    std::unique_ptr<Text>      actionText;
    std::unique_ptr<Text>      gameOverText;
    std::unique_ptr<Text>      retryBtnText;
    std::unique_ptr<Text>      retryKeyText;
    std::unique_ptr<Rectangle> retryButton;
    SDL_Rect                   retryBtnRect{};

    void Spawn() {
        auto player = reg.create();
        reg.emplace<Transform>(player,
                               (float)(mWindow->GetWidth() / 2 - 33),
                               (float)(mWindow->GetHeight() / 2 - 46));
        reg.emplace<Velocity>(player);
        reg.emplace<AnimationState>(player, 0, (int)walkFrames.size(), 0.0f, 12.0f, true);
        reg.emplace<Renderable>(player, playerSheet->GetSurface(), walkFrames, false);
        reg.emplace<PlayerTag>(player);
        reg.emplace<Health>(player);
        reg.emplace<Collider>(player, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
        reg.emplace<InvincibilityTimer>(player);

        for (int i = 0; i < 15; ++i) {
            float xPos  = rand() % (mWindow->GetWidth() - 100);
            float yPos  = rand() % (mWindow->GetHeight() - 100);
            auto  enemy = reg.create();
            reg.emplace<Transform>(enemy, xPos, yPos);
            reg.emplace<Velocity>(enemy);
            reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
            reg.emplace<Renderable>(enemy, enemySheet->GetSurface(), enemyWalkFrames, false);
            reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
        }
    }

    void Respawn() {
        reg.clear();
        gameOver = false;
        Spawn();
    }
};
