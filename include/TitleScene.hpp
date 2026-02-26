#pragma once
#include "Image.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SurfaceUtils.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <memory>

class GameScene;

class TitleScene : public Scene {
  public:
    void Load(Window& window) override {
        background = std::make_unique<Image>("game_assets/base_pack/bg_castle.png",
                                             nullptr, FitMode::PRESCALED);

        SDL_Rect windowRect = {0, 0, window.GetWidth(), window.GetHeight()};

        // Title — centered horizontally, above center vertically
        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, titleY - 80, 72);

        // Button — centered in window, slightly below center
        startBtnRect = CenterRect(windowRect, 150, 55, 20);
        startButton  = std::make_unique<Rectangle>(startBtnRect);
        startButton->SetColor({255, 255, 255, 255});
        startButton->SetHoverColor({180, 180, 180, 255});

        // Button text — centered inside the button
        auto [btnX, btnY] = Text::CenterInRect("Play", 32, startBtnRect);
        startBtnText = std::make_unique<Text>("Play", SDL_Color{0, 0, 0, 255},
                                              btnX, btnY, 32);

        // Hint text — centered horizontally, below the button
        auto [hintX, hintY] = Text::CenterInRect("Press ENTER to Play", 24, windowRect);
        startKeyText = std::make_unique<Text>("Press ENTER to Play",
                                              SDL_Color{200, 200, 200, 255},
                                              hintX, startBtnRect.y + startBtnRect.h + 20, 24);
    }

    void Unload() override {}

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN) {
            startGame = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;
            if (mx >= startBtnRect.x && mx <= startBtnRect.x + startBtnRect.w &&
                my >= startBtnRect.y && my <= startBtnRect.y + startBtnRect.h) {
                startGame = true;
            }
        }

        startButton->HandleEvent(e);
        return true;
    }

    void Update(float dt) override {}

    void Render(Window& window) override {
        window.Render();
        background->Render(window.GetSurface());
        titleText->Render(window.GetSurface());
        startButton->Render(window.GetSurface());
        startBtnText->Render(window.GetSurface());
        startKeyText->Render(window.GetSurface());
        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    bool startGame = false;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      startBtnText;
    std::unique_ptr<Text>      startKeyText;
    std::unique_ptr<Rectangle> startButton;
    SDL_Rect                   startBtnRect{};
};
