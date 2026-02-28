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
class LevelEditorScene;

class TitleScene : public Scene {
  public:
    void Load(Window& window) override {
        background = std::make_unique<Image>("game_assets/base_pack/bg_castle.png",
                                             nullptr, FitMode::PRESCALED);

        SDL_Rect windowRect = {0, 0, window.GetWidth(), window.GetHeight()};

        // Title
        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, titleY - 100, 72);

        // Play button — center, slightly above middle
        startBtnRect = CenterRect(windowRect, 150, 55, -10);
        startButton  = std::make_unique<Rectangle>(startBtnRect);
        startButton->SetColor({255, 255, 255, 255});
        startButton->SetHoverColor({180, 180, 180, 255});
        auto [btnX, btnY] = Text::CenterInRect("Play", 32, startBtnRect);
        startBtnText = std::make_unique<Text>("Play", SDL_Color{0, 0, 0, 255}, btnX, btnY, 32);

        // Editor button — below Play
        editorBtnRect = CenterRect(windowRect, 150, 55, 60);
        editorButton  = std::make_unique<Rectangle>(editorBtnRect);
        editorButton->SetColor({80, 120, 200, 255});
        editorButton->SetHoverColor({100, 150, 230, 255});
        auto [eBtnX, eBtnY] = Text::CenterInRect("Level Editor", 24, editorBtnRect);
        editorBtnText = std::make_unique<Text>("Level Editor", SDL_Color{255, 255, 255, 255},
                                               eBtnX, eBtnY, 24);

        // Hints
        auto [hintX, hintY] = Text::CenterInRect("Press ENTER to Play", 24, windowRect);
        startKeyText = std::make_unique<Text>("Press ENTER to Play",
                                              SDL_Color{200, 200, 200, 255},
                                              hintX, editorBtnRect.y + editorBtnRect.h + 20, 24);
    }

    void Unload() override {}

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN)
            startGame = true;

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;
            if (mx >= startBtnRect.x && mx <= startBtnRect.x + startBtnRect.w &&
                my >= startBtnRect.y && my <= startBtnRect.y + startBtnRect.h)
                startGame = true;
            if (mx >= editorBtnRect.x && mx <= editorBtnRect.x + editorBtnRect.w &&
                my >= editorBtnRect.y && my <= editorBtnRect.y + editorBtnRect.h)
                openEditor = true;
        }

        startButton->HandleEvent(e);
        editorButton->HandleEvent(e);
        return true;
    }

    void Update(float dt) override {}

    void Render(Window& window) override {
        window.Render();
        background->Render(window.GetSurface());
        titleText->Render(window.GetSurface());
        startButton->Render(window.GetSurface());
        startBtnText->Render(window.GetSurface());
        editorButton->Render(window.GetSurface());
        editorBtnText->Render(window.GetSurface());
        startKeyText->Render(window.GetSurface());
        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    bool startGame  = false;
    bool openEditor = false;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      startBtnText;
    std::unique_ptr<Text>      editorBtnText;
    std::unique_ptr<Text>      startKeyText;
    std::unique_ptr<Rectangle> startButton;
    std::unique_ptr<Rectangle> editorButton;
    SDL_Rect                   startBtnRect{};
    SDL_Rect                   editorBtnRect{};
};
