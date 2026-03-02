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
#include <entt/entt.hpp>
#include <memory>
#include <vector>

class LevelTwo : public Scene {
  public:
    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    entt::registry reg;
    bool           gameOver           = false;
    bool           levelComplete      = false;
    float          levelCompleteTimer = 2.0f;
    int            totalCoins         = 0;
    int            coinCount          = 0;
    int            stompCount         = 0;
    Window*        mWindow            = nullptr;
    SDL_Rect       retryBtnRect{};

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

    void Spawn();
    void Respawn();
};
