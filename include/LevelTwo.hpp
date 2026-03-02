
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

class LevelTwo : public Scene {
  public:
    void Load(Window& window) override {
        mWindow  = &window;
        gameOver = false;

        playerSheet =
            std::make_unique<SpriteSheet>("game_assets/base_pack/Player/p1_spritesheet.png",
                                          "game_assets/base_pack/Player/p1_spritesheet.txt");

        // Load all player animations
        walkFrames  = playerSheet->GetAnimation("p1_walk");
        jumpFrames  = walkFrames;
        idleFrames  = {playerSheet->GetFrame("p1_stand")};
        hurtFrames  = {playerSheet->GetFrame("p1_hurt")};
        duckFrames  = {playerSheet->GetFrame("p1_duck")};
        frontFrames = {playerSheet->GetFrame("p1_front")};

        enemySheet = std::make_unique<SpriteSheet>(
            "game_assets/base_pack/Enemies/enemies_spritesheet.png",
            "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
        enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

        background = std::make_unique<Image>(
            "game_assets/base_pack/deepspace_scene.png", nullptr, FitMode::PRESCALED);
        locationText = std::make_unique<Text>("You are in space!!", 20, 20);

        actionText = std::make_unique<Text>(
            "Level 1: Collect ALL the coins!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

        gameOverText = std::make_unique<Text>("Game Over!",
                                              SDL_Color{255, 0, 0, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 - 60,
                                              64);
        retryBtnText = std::make_unique<Text>("Retry",
                                              SDL_Color{0, 0, 0, 255},
                                              window.GetWidth() / 2 - 28,
                                              window.GetHeight() / 2 + 22,
                                              32);
        retryKeyText = std::make_unique<Text>("Press R to Retry",
                                              SDL_Color{200, 200, 200, 255},
                                              window.GetWidth() / 2 - 100,
                                              window.GetHeight() / 2 + 110,
                                              24);

        retryBtnRect = {window.GetWidth() / 2 - 75, window.GetHeight() / 2 + 10, 150, 55};
        retryButton  = std::make_unique<Rectangle>(retryBtnRect);
        retryButton->SetColor({255, 255, 255, 255});
        retryButton->SetHoverColor({180, 180, 180, 255});

        levelCompleteText = std::make_unique<Text>("Level Complete!",
                                                   SDL_Color{255, 215, 0, 255},
                                                   window.GetWidth() / 2 - 160,
                                                   window.GetHeight() / 2 - 40,
                                                   64);
        Spawn();
    }

    void Unload() override {
        reg.clear();
        mWindow = nullptr;
    }

    std::unique_ptr<Scene> NextScene() override;

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT)
            return false;

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
                if (mx >= retryBtnRect.x && mx <= retryBtnRect.x + retryBtnRect.w &&
                    my >= retryBtnRect.y && my <= retryBtnRect.y + retryBtnRect.h) {
                    Respawn();
                }
            }
            retryButton->HandleEvent(e);
        }
        return true;
    }

    void Update(float dt) override {
        if (levelComplete) {
            levelCompleteTimer -= dt;
            return;
        }
        if (!gameOver) {
            MovementSystem(reg, dt, mWindow->GetWidth());
            CenterPullSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
            PlayerStateSystem(reg);
            BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
            AnimationSystem(reg, dt);
            CollisionSystem(reg,
                            dt,
                            gameOver,
                            coinCount,
                            stompCount,
                            mWindow->GetWidth(),
                            mWindow->GetHeight());

            // Check win condition
            if (totalCoins > 0 && coinCount >= totalCoins)
                levelComplete = true;
        }
    }

    void Render(Window& window) override {
        window.Render();
        background->Render(window.GetSurface());

        if (levelComplete) {
            RenderSystem(reg, window.GetSurface());
            HUDSystem(reg,
                      window.GetSurface(),
                      window.GetWidth(),
                      healthText.get(),
                      gravityText.get(),
                      coinText.get(),
                      coinCount,
                      stompText.get(),
                      stompCount);
            if (levelCompleteText)
                levelCompleteText->Render(window.GetSurface());
        } else if (gameOver) {
            gameOverText->Render(window.GetSurface());
            retryButton->Render(window.GetSurface());
            retryBtnText->Render(window.GetSurface());
            retryKeyText->Render(window.GetSurface());
        } else {
            locationText->Render(window.GetSurface());
            actionText->Render(window.GetSurface());
            RenderSystem(reg, window.GetSurface());
            HUDSystem(reg,
                      window.GetSurface(),
                      window.GetWidth(),
                      healthText.get(),
                      gravityText.get(),
                      coinText.get(),
                      coinCount,
                      stompText.get(),
                      stompCount);
        }

        window.Update();
    }

  private:
    entt::registry reg;
    bool           gameOver           = false;
    bool           levelComplete      = false;
    float          levelCompleteTimer = 2.0f; // seconds before transitioning
    int            totalCoins         = 0;
    Window*        mWindow            = nullptr;

    std::unique_ptr<SpriteSheet> playerSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::vector<SDL_Rect>        walkFrames;
    std::vector<SDL_Rect>        jumpFrames;
    std::vector<SDL_Rect>        idleFrames;
    std::vector<SDL_Rect>        hurtFrames;
    std::vector<SDL_Rect>        duckFrames;
    std::vector<SDL_Rect>        frontFrames;
    std::vector<SDL_Rect>        enemyWalkFrames;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      locationText;
    std::unique_ptr<Text>      actionText;
    std::unique_ptr<Text>      gameOverText;
    std::unique_ptr<Text>      retryBtnText;
    std::unique_ptr<Text>      retryKeyText;
    std::unique_ptr<Rectangle> retryButton;
    std::unique_ptr<Text>      healthText;
    std::unique_ptr<Text>      gravityText;
    std::unique_ptr<Text>      coinText;
    std::unique_ptr<Text>      stompText;
    std::unique_ptr<Text>      levelCompleteText;
    int                        coinCount  = 0;
    int                        stompCount = 0;
    SDL_Rect                   retryBtnRect{};

    void Spawn() {
        healthText  = std::make_unique<Text>("100", SDL_Color{255, 255, 255, 255}, 0, 0, 16);
        gravityText = std::make_unique<Text>("", SDL_Color{100, 200, 255, 255}, 0, 0, 20);
        coinText    = std::make_unique<Text>(
            "Gold Collected: 0", SDL_Color{255, 215, 0, 255}, 0, 0, 16);
        stompText = std::make_unique<Text>(
            "Enemies Stomped: 0", SDL_Color{255, 100, 100, 255}, 0, 0, 16);
        coinSheet =
            std::make_unique<SpriteSheet>("game_assets/gold_coins/", "Gold_", 30, 40, 40);
        std::vector<SDL_Rect> coinFrames = coinSheet->GetAnimation("Gold_");

        // Spawn coins randomly along the left, top, or right wall
        for (int i = 0; i < COIN_COUNT; i++) {
            int   wall = rand() % 3; // 0=left, 1=top, 2=right
            float cx   = 0.0f;
            float cy   = 0.0f;
            int   pad  = COIN_SIZE + 10;

            switch (wall) {
                case 0: // left wall
                    cx = 5.0f;
                    cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2));
                    break;
                case 1: // top wall
                    cx = static_cast<float>(pad + rand() % (mWindow->GetWidth() - pad * 2));
                    cy = 5.0f;
                    break;
                case 2: // right wall
                    cx = static_cast<float>(mWindow->GetWidth() - COIN_SIZE - 5);
                    cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2));
                    break;
            }

            auto coin = reg.create();
            reg.emplace<Transform>(coin, cx, cy);
            reg.emplace<Renderable>(coin, coinSheet->GetSurface(), coinFrames, false);
            reg.emplace<AnimationState>(coin, 0, (int)coinFrames.size(), 0.0f, 15.0f, true);
            reg.emplace<Collider>(coin, COIN_SIZE, COIN_SIZE);
            reg.emplace<CoinTag>(coin);
        }

        // Count total coins so we know the win condition
        totalCoins = static_cast<int>(reg.view<CoinTag>().size());

        auto player = reg.create();
        reg.emplace<Transform>(player,
                               (float)(mWindow->GetWidth() / 2 - 33),
                               (float)(mWindow->GetHeight() - PLAYER_SPRITE_HEIGHT));
        reg.emplace<Velocity>(player);
        reg.emplace<AnimationState>(player, 0, (int)walkFrames.size(), 0.0f, 12.0f, true);
        reg.emplace<Renderable>(player, playerSheet->GetSurface(), walkFrames, false);
        reg.emplace<PlayerTag>(player);
        reg.emplace<Health>(player);
        reg.emplace<Collider>(player, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
        reg.emplace<InvincibilityTimer>(player);
        reg.emplace<GravityState>(player);
        reg.emplace<AnimationSet>(player,
                                  AnimationSet{
                                      .idle  = idleFrames,
                                      .walk  = walkFrames,
                                      .jump  = jumpFrames,
                                      .hurt  = hurtFrames,
                                      .duck  = duckFrames,
                                      .front = frontFrames,
                                  });

        for (int i = 0; i < GRAVITYSLUGSCOUNT; ++i) {
            float xPos = static_cast<float>(rand() % (mWindow->GetWidth() - 100));
            float yPos =
                static_cast<float>(rand() % (mWindow->GetHeight() - SLIME_SPRITE_HEIGHT));
            auto enemy = reg.create();
            reg.emplace<Transform>(enemy, xPos, yPos);
            // Random speed between 60 and 180, random direction
            float speed = 60.0f + static_cast<float>(rand() % 120);
            float dx    = (rand() % 2 == 0) ? speed : -speed;
            reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
            reg.emplace<AnimationState>(
                enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
            reg.emplace<Renderable>(enemy, enemySheet->GetSurface(), enemyWalkFrames, false);
            reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
            reg.emplace<EnemyTag>(enemy);
        }
    }

    void Respawn() {
        reg.clear();
        gameOver           = false;
        levelComplete      = false;
        levelCompleteTimer = 2.0f;
        coinCount          = 0;
        stompCount         = 0;
        Spawn();
    }
};
