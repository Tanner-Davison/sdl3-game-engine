#pragma once
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class LevelEditorScene : public Scene {
  public:
    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override {}
    void Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    enum class Tool { Coin, Enemy, Erase, PlayerStart, Tile };

    // --- Constants ---
    static constexpr int GRID       = 40;
    static constexpr int TOOLBAR_H  = 60;
    static constexpr int PALETTE_W  = 120; // right-side palette panel width
    static constexpr int ICON_SIZE  = 40;
    static constexpr int PAL_ICON   = 50;  // palette thumbnail size
    static constexpr int PAL_COLS   = 2;
    static constexpr float ENEMY_SPEED = 120.0f;

    // --- State ---
    Window*     mWindow      = nullptr;
    Tool        mActiveTool  = Tool::Coin;
    bool        mLaunchGame  = false;
    bool        mIsDragging  = false;
    int         mDragIndex   = -1;
    bool        mDragIsCoin  = false;
    bool        mDragIsTile  = false;
    std::string mStatusMsg   = "New level";
    std::string mLevelName   = "level1";
    int         mPaletteScroll = 0;       // scroll offset in palette (items)
    int         mSelectedTile  = 0;       // index into mPaletteItems
    int         mTileW = GRID, mTileH = GRID; // current tile place size

    Level mLevel;

    // --- Palette item ---
    struct PaletteItem {
        std::string path;        // full relative path to image
        std::string label;       // filename without extension
        SDL_Surface* thumb = nullptr; // scaled thumbnail for palette sidebar
        SDL_Surface* full  = nullptr; // full-res surface for rendering placed tiles
    };
    std::vector<PaletteItem> mPaletteItems;

    // --- Assets ---
    std::unique_ptr<Image>       background;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::unique_ptr<SpriteSheet> enemySheet;

    // --- Toolbar button rects ---
    SDL_Rect btnCoin {}, btnEnemy {}, btnErase {}, btnPlayerStart {};
    SDL_Rect btnTile {}, btnSave  {}, btnLoad  {}, btnPlay        {}, btnClear {};

    // --- Text ---
    std::unique_ptr<Text> lblCoin, lblEnemy, lblErase, lblPlayer;
    std::unique_ptr<Text> lblTile, lblSave,  lblLoad,  lblPlay,   lblClear;
    std::unique_ptr<Text> lblStatus, lblTool;

    // --- Helpers ---
    int CanvasW() const { return mWindow ? mWindow->GetWidth() - PALETTE_W : 800; }

    SDL_Point SnapToGrid(int x, int y) const {
        int cx = (x / GRID) * GRID;
        int cy = ((y - TOOLBAR_H) / GRID) * GRID + TOOLBAR_H;
        return {cx, std::max(TOOLBAR_H, cy)};
    }

    bool HitTest(const SDL_Rect& r, int x, int y) const {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    int HitCoin(int x, int y) const {
        for (int i = 0; i < (int)mLevel.coins.size(); i++) {
            SDL_Rect r = {(int)mLevel.coins[i].x, (int)mLevel.coins[i].y, GRID, GRID};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }
    int HitEnemy(int x, int y) const {
        for (int i = 0; i < (int)mLevel.enemies.size(); i++) {
            SDL_Rect r = {(int)mLevel.enemies[i].x, (int)mLevel.enemies[i].y, GRID, GRID};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }
    int HitTile(int x, int y) const {
        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
            const auto& t = mLevel.tiles[i];
            SDL_Rect r = {(int)t.x, (int)t.y, t.w, t.h};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }

    void SetStatus(const std::string& msg) {
        mStatusMsg = msg;
        if (lblStatus) lblStatus->CreateSurface(mStatusMsg);
    }

    void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    }

    void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        SDL_Rect rects[4] = {
            {r.x, r.y, r.w, t}, {r.x, r.y + r.h, r.w, t},
            {r.x, r.y, t, r.h}, {r.x + r.w, r.y, t, r.h}
        };
        for (auto& rr : rects) SDL_FillSurfaceRect(s, &rr, col);
    }

    void LoadPalette();
};
