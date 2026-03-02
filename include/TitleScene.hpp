#pragma once
#include "Image.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SurfaceUtils.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class GameScene;
class LevelEditorScene;
namespace fs = std::filesystem;

class TitleScene : public Scene {
  public:
    void Load(Window& window) override {
        mWindowW = window.GetWidth();
        mWindowH = window.GetHeight();

        background = std::make_unique<Image>("game_assets/backgrounds/bg_castle.png",
                                             nullptr, FitMode::PRESCALED);

        SDL_Rect windowRect = {0, 0, mWindowW, mWindowH};

        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, titleY - 120, 72);

        // Two top buttons side by side: Play (hardcoded) | Level Editor
        int   btnW = 180, btnH = 55;
        int   gap  = 20;
        int   cy   = mWindowH / 2 - 80;
        int   cx   = mWindowW / 2;

        // Play button (left)
        playBtnRect = {cx - btnW - gap / 2, cy, btnW, btnH};
        playButton  = std::make_unique<Rectangle>(playBtnRect);
        playButton->SetColor({255, 255, 255, 255});
        playButton->SetHoverColor({180, 180, 180, 255});
        auto [pbx, pby] = Text::CenterInRect("Play", 32, playBtnRect);
        playBtnText = std::make_unique<Text>("Play", SDL_Color{0, 0, 0, 255}, pbx, pby, 32);

        // Level Editor button (right)
        editorBtnRect = {cx + gap / 2, cy, btnW, btnH};
        editorButton  = std::make_unique<Rectangle>(editorBtnRect);
        editorButton->SetColor({80, 120, 200, 255});
        editorButton->SetHoverColor({100, 150, 230, 255});
        auto [eBtnX, eBtnY] = Text::CenterInRect("Level Editor", 24, editorBtnRect);
        editorBtnText = std::make_unique<Text>("Level Editor", SDL_Color{255, 255, 255, 255},
                                               eBtnX, eBtnY, 24);

        // Hint below the two buttons
        hintText = std::make_unique<Text>("Press ENTER to play hardcoded level",
                                          SDL_Color{160, 160, 160, 255},
                                          cx - 190, cy + btnH + 10, 16);

        // Scan levels/ directory and build a play button for each .json file
        scanLevels(mWindowW, mWindowH);

        if (mLevelButtons.empty()) {
            noLevelsText = std::make_unique<Text>(
                "No saved levels yet — make one in the Level Editor!",
                SDL_Color{140, 140, 140, 255},
                cx - 230, editorBtnRect.y + editorBtnRect.h + 60, 16);
        }
    }

    void Unload() override {}

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        // ENTER = hardcoded play (empty path)
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN) {
            mChosenLevel = "";
            startGame    = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;

            // Hardcoded Play button
            if (mx >= playBtnRect.x && mx <= playBtnRect.x + playBtnRect.w &&
                my >= playBtnRect.y && my <= playBtnRect.y + playBtnRect.h) {
                mChosenLevel = "";
                startGame    = true;
            }
            // Level Editor button
            if (mx >= editorBtnRect.x && mx <= editorBtnRect.x + editorBtnRect.w &&
                my >= editorBtnRect.y && my <= editorBtnRect.y + editorBtnRect.h) {
                openEditor = true;
            }
            // Saved level buttons
            for (auto& lb : mLevelButtons) {
                if (mx >= lb.rect.x && mx <= lb.rect.x + lb.rect.w &&
                    my >= lb.rect.y && my <= lb.rect.y + lb.rect.h) {
                    mChosenLevel = lb.path;
                    startGame    = true;
                }
            }
        }

        playButton->HandleEvent(e);
        editorButton->HandleEvent(e);
        return true;
    }

    void Update(float dt) override {}

    void Render(Window& window) override {
        window.Render();
        SDL_Surface* s = window.GetSurface();
        background->Render(s);
        titleText->Render(s);
        // Top row buttons
        playButton->Render(s);
        playBtnText->Render(s);
        editorButton->Render(s);
        editorBtnText->Render(s);
        if (hintText)    hintText->Render(s);
        // Saved levels section
        if (levelsHeader) levelsHeader->Render(s);
        if (mLevelButtons.empty() && noLevelsText) noLevelsText->Render(s);
        for (auto& lb : mLevelButtons) {
            lb.btn->Render(s);
            lb.label->Render(s);
        }
        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    struct LevelButton {
        SDL_Rect                   rect;
        std::string                path;
        std::unique_ptr<Rectangle> btn;
        std::unique_ptr<Text>      label;
    };

    void scanLevels(int winW, int winH) {
        mLevelButtons.clear();
        if (!fs::exists("levels")) return;

        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator("levels"))
            if (entry.path().extension() == ".json")
                found.push_back(entry.path());
        std::sort(found.begin(), found.end());

        // Layout: centered column below the editor button, one row per level
        int btnW    = 260;
        int btnH    = 48;
        int gap     = 12;
        int startY  = editorBtnRect.y + editorBtnRect.h + 30;
        int centerX = winW / 2;

        // Header
        if (!found.empty()) {
            levelsHeader = std::make_unique<Text>(
                "-- Play a Saved Level --",
                SDL_Color{255, 215, 0, 255},
                centerX - 130, startY, 20);
            startY += 34;
        }

        for (const auto& p : found) {
            SDL_Rect r = {centerX - btnW / 2, startY, btnW, btnH};
            auto btn = std::make_unique<Rectangle>(r);
            btn->SetColor({40, 160, 80, 255});
            btn->SetHoverColor({60, 200, 100, 255});

            std::string name = p.stem().string();
            auto [lx, ly] = Text::CenterInRect(name, 22, r);
            auto lbl = std::make_unique<Text>(name, SDL_Color{255, 255, 255, 255}, lx, ly, 22);

            mLevelButtons.push_back({r, p.string(), std::move(btn), std::move(lbl)});
            startY += btnH + gap;
        }
    }

    bool        startGame  = false;
    bool        openEditor = false;
    std::string mChosenLevel;
    int         mWindowW = 0;
    int         mWindowH = 0;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      playBtnText;
    std::unique_ptr<Text>      editorBtnText;
    std::unique_ptr<Text>      hintText;
    std::unique_ptr<Text>      noLevelsText;
    std::unique_ptr<Text>      levelsHeader;
    std::unique_ptr<Rectangle> playButton;
    std::unique_ptr<Rectangle> editorButton;
    SDL_Rect                   playBtnRect{};
    SDL_Rect                   editorBtnRect{};

    std::vector<LevelButton> mLevelButtons;
};
