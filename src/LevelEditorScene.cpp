#include "LevelEditorScene.hpp"
#include "GameScene.hpp"

namespace fs = std::filesystem;

// ─── LoadPalette ──────────────────────────────────────────────────────────────
void LevelEditorScene::LoadPalette() {
    mPaletteItems.clear();

    std::vector<std::string> dirs = {
        "game_assets/tiles",
        "game_assets/props/Props",
        "game_assets/props/Signboards",
    };

    for (const auto& dir : dirs) {
        if (!fs::exists(dir)) continue;
        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator(dir))
            if (entry.path().extension() == ".png")
                paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());

        for (const auto& p : paths) {
            SDL_Surface* raw = IMG_Load(p.string().c_str());
            if (!raw) continue;
            SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (!converted) continue;
            SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_BLEND);

            SDL_Surface* thumb = SDL_CreateSurface(PAL_ICON, PAL_ICON, SDL_PIXELFORMAT_ARGB8888);
            if (thumb) {
                SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_NONE);
                SDL_Rect src = {0, 0, converted->w, converted->h};
                SDL_Rect dst = {0, 0, PAL_ICON, PAL_ICON};
                SDL_BlitSurfaceScaled(converted, &src, thumb, &dst, SDL_SCALEMODE_LINEAR);
                SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
            }
            mPaletteItems.push_back({p.string(), p.stem().string(), thumb, converted});
        }
    }
}

// ─── LoadBgPalette ────────────────────────────────────────────────────────────
void LevelEditorScene::LoadBgPalette() {
    mBgItems.clear();

    const std::string dir = "game_assets/backgrounds";
    if (!fs::exists(dir)) return;

    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.path().extension() == ".png")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());

    for (const auto& p : paths) {
        SDL_Surface* raw = IMG_Load(p.string().c_str());
        if (!raw) continue;
        SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!converted) continue;

        // Wider thumbnail for backgrounds (1 column), fill full palette width
        SDL_Surface* thumb = SDL_CreateSurface(PALETTE_W - 8, (PALETTE_W - 8) / 2,
                                               SDL_PIXELFORMAT_ARGB8888);
        if (thumb) {
            SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_NONE);
            SDL_Rect src = {0, 0, converted->w, converted->h};
            SDL_Rect dst = {0, 0, thumb->w, thumb->h};
            SDL_BlitSurfaceScaled(converted, &src, thumb, &dst, SDL_SCALEMODE_LINEAR);
            SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
        }
        SDL_DestroySurface(converted);

        mBgItems.push_back({p.string(), p.stem().string(), thumb});

        // Pre-select whichever bg matches the current level background
        if (p.string() == mLevel.background)
            mSelectedBg = (int)mBgItems.size() - 1;
    }
}

// ─── ApplyBackground ─────────────────────────────────────────────────────────
void LevelEditorScene::ApplyBackground(int idx) {
    if (idx < 0 || idx >= (int)mBgItems.size()) return;
    mSelectedBg        = idx;
    mLevel.background  = mBgItems[idx].path;

    // Reload the canvas preview image
    background = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
    SetStatus("Background: " + mBgItems[idx].label);
}

// ─── Load ─────────────────────────────────────────────────────────────────────
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;

    background = std::make_unique<Image>(
        "game_assets/backgrounds/deepspace_scene.png", nullptr, FitMode::PRESCALED);
    coinSheet  = std::make_unique<SpriteSheet>(
        "game_assets/gold_coins/", "Gold_", 30, ICON_SIZE, ICON_SIZE);
    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    if (mLevel.coins.empty() && mLevel.enemies.empty() && mLevel.tiles.empty()) {
        std::string autoPath = "levels/" + mLevelName + ".json";
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
            // Reload background from saved level
            if (!mLevel.background.empty())
                background = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
        }
    }

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - 16);
        mLevel.player.y = static_cast<float>(window.GetHeight() - 60);
    }

    LoadPalette();
    LoadBgPalette();

    // ── Toolbar layout ────────────────────────────────────────────────────────
    int bw = 80, bh = 44, pad = 6, sx = pad, y0 = 8;
    auto nb = [&]() -> SDL_Rect { SDL_Rect r={sx,y0,bw,bh}; sx+=bw+pad; return r; };
    btnCoin        = nb();
    btnEnemy       = nb();
    btnTile        = nb();
    btnErase       = nb();
    btnPlayerStart = nb();
    sx += pad;
    btnSave  = nb();
    btnLoad  = nb();
    sx += pad;
    btnClear = nb();
    sx += pad;
    btnPlay  = nb();

    auto mkLbl = [](const std::string& s, SDL_Rect r) {
        auto [x,y] = Text::CenterInRect(s, 13, r);
        return std::make_unique<Text>(s, SDL_Color{0,0,0,255}, x, y, 13);
    };
    lblCoin    = mkLbl("Coin",   btnCoin);
    lblEnemy   = mkLbl("Enemy",  btnEnemy);
    lblTile    = mkLbl("Tile",   btnTile);
    lblErase   = mkLbl("Erase",  btnErase);
    lblPlayer  = mkLbl("Player", btnPlayerStart);
    lblSave    = mkLbl("Save",   btnSave);
    lblLoad    = mkLbl("Load",   btnLoad);
    lblClear   = mkLbl("Clear",  btnClear);
    lblPlay    = mkLbl("Play",   btnPlay);

    lblStatus = std::make_unique<Text>(mStatusMsg, SDL_Color{220,220,220,255}, pad, TOOLBAR_H+4, 12);
    lblTool   = std::make_unique<Text>("Tool: Coin", SDL_Color{255,215,0,255},
                                       window.GetWidth()-PALETTE_W-140, 18, 13);
}

// ─── Unload ───────────────────────────────────────────────────────────────────
void LevelEditorScene::Unload() {
    for (auto& item : mPaletteItems) {
        if (item.thumb) SDL_DestroySurface(item.thumb);
        if (item.full)  SDL_DestroySurface(item.full);
    }
    mPaletteItems.clear();
    for (auto& item : mBgItems) {
        if (item.thumb) SDL_DestroySurface(item.thumb);
    }
    mBgItems.clear();
    mWindow = nullptr;
}

// ─── HandleEvent ──────────────────────────────────────────────────────────────
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // ── File drop ─────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_BEGIN) {
        mDropActive = true;
        SetStatus("Drop a .png to import...");
        return true;
    }
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mDropActive = false;
        return true;
    }
    if (e.type == SDL_EVENT_DROP_FILE) {
        mDropActive = false;
        std::string path = e.drop.data ? std::string(e.drop.data) : "";
        if (!path.empty()) ImportDroppedTile(path);
        return true;
    }

    // ── Import text input mode ────────────────────────────────────────────────
    if (mImportInputActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            mImportInputText += e.text.text;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) {
                mImportInputActive = false;
                mImportInputText.clear();
                SetStatus("Import cancelled");
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
            if (e.key.key == SDLK_BACKSPACE && !mImportInputText.empty()) {
                mImportInputText.pop_back();
                return true;
            }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                std::string path = mImportInputText;
                mImportInputActive = false;
                mImportInputText.clear();
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                if (!path.empty()) ImportDroppedTile(path);
                return true;
            }
        }
        return true;
    }

    // ── Mouse wheel ───────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy; SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        if (mx >= CanvasW()) {
            // Scroll the active palette
            if (mActiveTab == PaletteTab::Tiles) {
                mPaletteScroll = std::max(0, mPaletteScroll - (int)e.wheel.y);
                int totalRows  = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
                mPaletteScroll = std::min(mPaletteScroll, std::max(0, totalRows - 1));
            } else {
                mBgPaletteScroll = std::max(0, mBgPaletteScroll - (int)e.wheel.y);
                mBgPaletteScroll = std::min(mBgPaletteScroll,
                                            std::max(0, (int)mBgItems.size() - 1));
            }
        } else {
            if (mActiveTool == Tool::Tile) {
                mTileW = std::max(GRID, mTileW + (int)e.wheel.y * GRID);
                mTileH = mTileW;
                SetStatus("Tile size: " + std::to_string(mTileW));
            }
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_1: mActiveTool = Tool::Coin;        lblTool->CreateSurface("Tool: Coin");   break;
            case SDLK_2: mActiveTool = Tool::Enemy;       lblTool->CreateSurface("Tool: Enemy");  break;
            case SDLK_3: mActiveTool = Tool::Tile;        lblTool->CreateSurface("Tool: Tile");
                         mActiveTab  = PaletteTab::Tiles; break;
            case SDLK_4: mActiveTool = Tool::Erase;       lblTool->CreateSurface("Tool: Erase");  break;
            case SDLK_5: mActiveTool = Tool::PlayerStart; lblTool->CreateSurface("Tool: Player"); break;
            case SDLK_6: mActiveTab  = PaletteTab::Backgrounds;
                         lblTool->CreateSurface("BG picker"); break;
            case SDLK_I:
                mImportInputActive = true;
                mImportInputText.clear();
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                if (mActiveTab == PaletteTab::Backgrounds)
                    SetStatus("Import background path (Enter=import, Esc=cancel):");
                else
                    SetStatus("Import tile path (Enter=import, Esc=cancel):");
                break;
            case SDLK_S:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    fs::create_directories("levels");
                    std::string path = "levels/" + mLevelName + ".json";
                    mLevel.name = mLevelName;
                    SaveLevel(mLevel, path);
                    SetStatus("Saved: " + path);
                }
                break;
            case SDLK_Z:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    if (!mLevel.tiles.empty())        { mLevel.tiles.pop_back();   SetStatus("Undo tile"); }
                    else if (!mLevel.coins.empty())   { mLevel.coins.pop_back();   SetStatus("Undo coin"); }
                    else if (!mLevel.enemies.empty()) { mLevel.enemies.pop_back(); SetStatus("Undo enemy"); }
                }
                break;
        }
    }

    // ── Mouse button down ─────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x;
        int my = (int)e.button.y;

        // ── Palette tab bar click ─────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H && my < TOOLBAR_H + TAB_H) {
            int halfW = PALETTE_W / 2;
            if (mx < CanvasW() + halfW)
                mActiveTab = PaletteTab::Tiles;
            else
                mActiveTab = PaletteTab::Backgrounds;
            return true;
        }

        // ── Toolbar buttons ────────────────────────────────────────────────────
        auto tb = [&](SDL_Rect r, Tool t, const std::string& lbl) {
            if (HitTest(r, mx, my)) { mActiveTool = t; lblTool->CreateSurface("Tool: "+lbl); return true; }
            return false;
        };
        if (tb(btnCoin,        Tool::Coin,        "Coin"))   return true;
        if (tb(btnEnemy,       Tool::Enemy,       "Enemy"))  return true;
        if (tb(btnTile,        Tool::Tile,        "Tile"))   return true;
        if (tb(btnErase,       Tool::Erase,       "Erase"))  return true;
        if (tb(btnPlayerStart, Tool::PlayerStart, "Player")) return true;

        if (HitTest(btnSave, mx, my)) {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name = mLevelName;
            SaveLevel(mLevel, path); SetStatus("Saved: " + path); return true;
        }
        if (HitTest(btnLoad, mx, my)) {
            std::string path = "levels/" + mLevelName + ".json";
            if (LoadLevel(path, mLevel)) {
                SetStatus("Loaded: " + path);
                if (!mLevel.background.empty())
                    background = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
                LoadBgPalette(); // re-sync selected bg highlight
            } else {
                SetStatus("No file: " + path);
            }
            return true;
        }
        if (HitTest(btnClear, mx, my)) {
            mLevel.coins.clear(); mLevel.enemies.clear(); mLevel.tiles.clear();
            SetStatus("Cleared"); return true;
        }
        if (HitTest(btnPlay, mx, my)) {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name = mLevelName;
            SaveLevel(mLevel, path); mLaunchGame = true; return true;
        }

        // ── Palette panel clicks ───────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mActiveTab == PaletteTab::Tiles) {
                // Same 2-column grid as before
                constexpr int PAD   = 4;
                constexpr int LBL_H = 14;
                const int cellW = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
                const int cellH = cellW + LBL_H;
                const int itemH = cellH + PAD;
                int relX = mx - CanvasW() - PAD;
                int relY = my - TOOLBAR_H - TAB_H - PAD;
                int col  = relX / (cellW + PAD);
                int row  = relY / itemH;
                if (col >= 0 && col < PAL_COLS) {
                    int idx = (mPaletteScroll + row) * PAL_COLS + col;
                    if (idx >= 0 && idx < (int)mPaletteItems.size()) {
                        mSelectedTile = idx;
                        mActiveTool   = Tool::Tile;
                        lblTool->CreateSurface("Tool: Tile");
                        SetStatus("Selected: " + mPaletteItems[idx].label);
                    }
                }
            } else {
                // Single-column background list
                constexpr int PAD   = 4;
                constexpr int LBL_H = 14;
                const int thumbH = (PALETTE_W - 8) / 2;
                const int itemH  = thumbH + LBL_H + PAD;
                int relY = my - TOOLBAR_H - TAB_H - PAD;
                int row  = relY / itemH;
                int idx  = mBgPaletteScroll + row;
                if (idx >= 0 && idx < (int)mBgItems.size())
                    ApplyBackground(idx);
            }
            return true;
        }

        // ── Canvas clicks ──────────────────────────────────────────────────────
        if (my < TOOLBAR_H || mx >= CanvasW()) return true;
        auto [sx, sy] = SnapToGrid(mx, my);

        switch (mActiveTool) {
            case Tool::Coin:
                mLevel.coins.push_back({(float)sx, (float)sy});
                SetStatus("Coin at " + std::to_string(sx) + "," + std::to_string(sy));
                break;
            case Tool::Enemy:
                mLevel.enemies.push_back({(float)sx, (float)sy, ENEMY_SPEED});
                SetStatus("Enemy at " + std::to_string(sx) + "," + std::to_string(sy));
                break;
            case Tool::Tile:
                if (!mPaletteItems.empty()) {
                    mLevel.tiles.push_back({(float)sx, (float)sy, mTileW, mTileH,
                                            mPaletteItems[mSelectedTile].path});
                    SetStatus("Tile: " + mPaletteItems[mSelectedTile].label);
                }
                break;
            case Tool::Erase: {
                int ti = HitTile(mx, my);
                if (ti >= 0) { mLevel.tiles.erase(mLevel.tiles.begin()+ti); SetStatus("Erased tile"); break; }
                int ci = HitCoin(mx, my);
                if (ci >= 0) { mLevel.coins.erase(mLevel.coins.begin()+ci); SetStatus("Erased coin"); break; }
                int ei = HitEnemy(mx, my);
                if (ei >= 0) { mLevel.enemies.erase(mLevel.enemies.begin()+ei); SetStatus("Erased enemy"); break; }
                break;
            }
            case Tool::PlayerStart:
                mLevel.player = {(float)sx, (float)sy};
                SetStatus("Player start set");
                break;
        }

        // Start drag
        if (mActiveTool != Tool::Erase) {
            int ti = HitTile(mx, my);
            if (ti >= 0) { mIsDragging=true; mDragIndex=ti; mDragIsTile=true;  mDragIsCoin=false; return true; }
            int ci = HitCoin(mx, my);
            if (ci >= 0) { mIsDragging=true; mDragIndex=ci; mDragIsCoin=true;  mDragIsTile=false; return true; }
            int ei = HitEnemy(mx, my);
            if (ei >= 0) { mIsDragging=true; mDragIndex=ei; mDragIsCoin=false; mDragIsTile=false; return true; }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        mIsDragging = false;

    if (e.type == SDL_EVENT_MOUSE_MOTION && mIsDragging && mDragIndex >= 0) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;
        if (my >= TOOLBAR_H && mx < CanvasW()) {
            auto [sx, sy] = SnapToGrid(mx, my);
            if (mDragIsTile && mDragIndex < (int)mLevel.tiles.size()) {
                mLevel.tiles[mDragIndex].x = (float)sx;
                mLevel.tiles[mDragIndex].y = (float)sy;
            } else if (mDragIsCoin && mDragIndex < (int)mLevel.coins.size()) {
                mLevel.coins[mDragIndex].x = (float)sx;
                mLevel.coins[mDragIndex].y = (float)sy;
            } else if (!mDragIsCoin && !mDragIsTile && mDragIndex < (int)mLevel.enemies.size()) {
                mLevel.enemies[mDragIndex].x = (float)sx;
                mLevel.enemies[mDragIndex].y = (float)sy;
            }
        }
    }

    return true;
}

// ─── Render ───────────────────────────────────────────────────────────────────
void LevelEditorScene::Render(Window& window) {
    window.Render();
    SDL_Surface* screen = window.GetSurface();
    int cw = CanvasW();

    background->Render(screen);

    // Grid
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
    Uint32 gridCol = SDL_MapRGBA(fmt, nullptr, 255,255,255,20);
    for (int x = 0; x < cw; x += GRID) {
        SDL_Rect l = {x, TOOLBAR_H, 1, window.GetHeight()-TOOLBAR_H};
        SDL_FillSurfaceRect(screen, &l, gridCol);
    }
    for (int y = TOOLBAR_H; y < window.GetHeight(); y += GRID) {
        SDL_Rect l = {0, y, cw, 1};
        SDL_FillSurfaceRect(screen, &l, gridCol);
    }

    // Tiles
    for (const auto& t : mLevel.tiles) {
        SDL_Surface* tileSurf = nullptr;
        for (const auto& item : mPaletteItems)
            if (item.path == t.imagePath) { tileSurf = item.full ? item.full : item.thumb; break; }
        SDL_Rect dst = {(int)t.x, (int)t.y, t.w, t.h};
        if (tileSurf) {
            SDL_BlitSurfaceScaled(tileSurf, nullptr, screen, &dst, SDL_SCALEMODE_LINEAR);
        } else {
            SDL_Surface* loaded = IMG_Load(t.imagePath.c_str());
            if (loaded) { SDL_BlitSurfaceScaled(loaded, nullptr, screen, &dst, SDL_SCALEMODE_LINEAR); SDL_DestroySurface(loaded); }
            else         DrawRect(screen, dst, {80,80,120,200});
        }
        DrawOutline(screen, dst, {100,180,255,255});
    }

    // Coins
    auto coinFrames = coinSheet->GetAnimation("Gold_");
    if (!coinFrames.empty())
        for (const auto& c : mLevel.coins) {
            SDL_Rect src = coinFrames[0], dst = {(int)c.x,(int)c.y,ICON_SIZE,ICON_SIZE};
            SDL_BlitSurfaceScaled(coinSheet->GetSurface(), &src, screen, &dst, SDL_SCALEMODE_LINEAR);
            DrawOutline(screen, dst, {255,215,0,255});
        }

    // Enemies
    auto slimeFrames = enemySheet->GetAnimation("slimeWalk");
    if (!slimeFrames.empty())
        for (const auto& en : mLevel.enemies) {
            SDL_Rect src = slimeFrames[0], dst = {(int)en.x,(int)en.y,ICON_SIZE,ICON_SIZE};
            SDL_BlitSurfaceScaled(enemySheet->GetSurface(), &src, screen, &dst, SDL_SCALEMODE_LINEAR);
            DrawOutline(screen, dst, {255,80,80,255});
        }

    // Player marker
    DrawRect(screen, {(int)mLevel.player.x,(int)mLevel.player.y,32,20}, {0,200,80,180});
    DrawOutline(screen, {(int)mLevel.player.x,(int)mLevel.player.y,32,20}, {0,255,100,255}, 2);

    // Tile ghost
    if (mActiveTool == Tool::Tile && !mPaletteItems.empty()) {
        float fmx, fmy; SDL_GetMouseState(&fmx, &fmy); int mx=(int)fmx, my=(int)fmy;
        if (my >= TOOLBAR_H && mx < cw) {
            auto [sx,sy] = SnapToGrid(mx, my);
            DrawRect(screen, {sx, sy, mTileW, mTileH}, {100,180,255,60});
            DrawOutline(screen, {sx, sy, mTileW, mTileH}, {100,180,255,200});
        }
    }

    // ── Toolbar ───────────────────────────────────────────────────────────────
    DrawRect(screen, {0,0,window.GetWidth(),TOOLBAR_H}, {25,25,35,245});
    auto drawBtn = [&](SDL_Rect r, SDL_Color bg, SDL_Color border, Text* lbl, bool active) {
        if (active) bg = {70,140,255,255};
        DrawRect(screen, r, bg); DrawOutline(screen, r, border);
        if (lbl) lbl->Render(screen);
    };
    drawBtn(btnCoin,        {55,55,65,255},{180,180,180,255},lblCoin.get(),    mActiveTool==Tool::Coin);
    drawBtn(btnEnemy,       {55,55,65,255},{180,180,180,255},lblEnemy.get(),   mActiveTool==Tool::Enemy);
    drawBtn(btnTile,        {55,55,65,255},{180,180,180,255},lblTile.get(),    mActiveTool==Tool::Tile);
    drawBtn(btnErase,       {55,55,65,255},{180,180,180,255},lblErase.get(),   mActiveTool==Tool::Erase);
    drawBtn(btnPlayerStart, {55,55,65,255},{180,180,180,255},lblPlayer.get(),  mActiveTool==Tool::PlayerStart);
    drawBtn(btnSave,        {40,110,40,255},{120,230,120,255},lblSave.get(),   false);
    drawBtn(btnLoad,        {40,70,120,255},{120,160,230,255},lblLoad.get(),   false);
    drawBtn(btnClear,       {110,40,40,255},{230,100,100,255},lblClear.get(),  false);
    drawBtn(btnPlay,        {40,140,40,255},{80,230,80,255},  lblPlay.get(),   false);

    DrawRect(screen, {0,TOOLBAR_H,cw,22}, {18,18,26,220});
    if (lblStatus) lblStatus->Render(screen);
    if (lblTool)   lblTool->Render(screen);

    // ── Palette panel ─────────────────────────────────────────────────────────
    DrawRect(screen, {cw,0,PALETTE_W,window.GetHeight()}, {20,20,30,255});
    DrawOutline(screen, {cw,0,PALETTE_W,window.GetHeight()}, {60,60,80,255});

    // Tab bar  [  Tiles  |  Backgrounds  ]
    {
        int halfW = PALETTE_W / 2;
        bool tilesActive = (mActiveTab == PaletteTab::Tiles);

        SDL_Rect tabTiles = {cw,             TOOLBAR_H, halfW,    TAB_H};
        SDL_Rect tabBg    = {cw + halfW,     TOOLBAR_H, halfW,    TAB_H};

        SDL_Color activeC   = {50, 100, 200, 255};
        SDL_Color inactiveC = {30,  30,  45, 255};
        SDL_Color borderC   = {80, 120, 200, 255};

        DrawRect(screen, tabTiles, tilesActive  ? activeC : inactiveC);
        DrawRect(screen, tabBg,    !tilesActive ? activeC : inactiveC);
        DrawOutline(screen, tabTiles, borderC);
        DrawOutline(screen, tabBg,    borderC);

        auto [tx,ty] = Text::CenterInRect("Tiles",       11, tabTiles);
        auto [bx,by] = Text::CenterInRect("Backgrounds", 11, tabBg);
        Text tTiles("Tiles",       SDL_Color{tilesActive  ? (Uint8)255:(Uint8)160, 255, 255, 255}, tx, ty, 11);
        Text tBg   ("Backgrounds", SDL_Color{!tilesActive ? (Uint8)255:(Uint8)160, 255, 255, 255}, bx, by, 11);
        tTiles.Render(screen);
        tBg.Render(screen);
    }

    int paletteContentY = TOOLBAR_H + TAB_H;

    if (mActiveTab == PaletteTab::Tiles) {
        // ── Tiles palette (same as before) ────────────────────────────────────
        // Header strip
        DrawRect(screen, {cw, paletteContentY, PALETTE_W, 44}, {30,30,45,255});
        Text palHdr ("Tiles & Props",  SDL_Color{200,200,220,255}, cw+4, paletteContentY+4,  11);
        Text palHdr2("Scroll: wheel",  SDL_Color{120,120,140,255}, cw+4, paletteContentY+18, 10);
        Text palSize("Size: "+std::to_string(mTileW), SDL_Color{180,200,255,255}, cw+4, paletteContentY+32, 10);
        palHdr.Render(screen); palHdr2.Render(screen); palSize.Render(screen);
        paletteContentY += 44;

        static constexpr int PAL_PADDING = 4;
        static constexpr int PAL_LBL_H   = 14;
        const int cellW   = (PALETTE_W - PAL_PADDING * (PAL_COLS + 1)) / PAL_COLS;
        const int cellH   = cellW + PAL_LBL_H;
        const int itemH   = cellH + PAL_PADDING;
        const int visRows = (window.GetHeight() - paletteContentY) / itemH;
        const int startI  = mPaletteScroll * PAL_COLS;
        const int endI    = std::min(startI + (visRows + 1) * PAL_COLS, (int)mPaletteItems.size());

        for (int i = startI; i < endI; i++) {
            int col = (i - startI) % PAL_COLS;
            int row = (i - startI) / PAL_COLS;
            int ix  = cw + PAL_PADDING + col * (cellW + PAL_PADDING);
            int iy  = paletteContentY + PAL_PADDING + row * itemH;
            bool sel = (i == mSelectedTile && mActiveTool == Tool::Tile);

            SDL_Rect cell = {ix, iy, cellW, cellH};
            DrawRect(screen, cell, sel ? SDL_Color{50,100,200,220} : SDL_Color{35,35,55,220});
            DrawOutline(screen, cell, sel ? SDL_Color{100,180,255,255} : SDL_Color{55,55,80,255});

            SDL_Surface* ts = mPaletteItems[i].thumb ? mPaletteItems[i].thumb : mPaletteItems[i].full;
            if (ts) { SDL_Rect imgDst={ix+1,iy+1,cellW-2,cellW-2}; SDL_BlitSurfaceScaled(ts,nullptr,screen,&imgDst,SDL_SCALEMODE_LINEAR); }
            else    DrawRect(screen,{ix+1,iy+1,cellW-2,cellW-2},{60,40,80,255});

            std::string lbl = mPaletteItems[i].label;
            if ((int)lbl.size() > 9) lbl = lbl.substr(0,8) + "~";
            Uint8 lr=sel?255:170, lg=sel?255:170, lb=sel?255:190;
            Text lblT(lbl,SDL_Color{lr,lg,lb,255},ix+2,iy+cellW+2,9);
            lblT.Render(screen);
        }

        // Scroll indicator
        int totalRows = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
        if (totalRows > visRows) {
            float pct = (float)mPaletteScroll / std::max(1, totalRows - visRows);
            int   sh  = std::max(20, (int)((window.GetHeight()-paletteContentY)*visRows/(float)totalRows));
            int   sy2 = paletteContentY + (int)((window.GetHeight()-paletteContentY-sh)*pct);
            DrawRect(screen, {cw+PALETTE_W-4, sy2, 3, sh}, {100,150,255,180});
        }

    } else {
        // ── Backgrounds palette ───────────────────────────────────────────────
        DrawRect(screen, {cw, paletteContentY, PALETTE_W, 24}, {30,30,45,255});
        Text bgHdr("Backgrounds  (I=import)", SDL_Color{200,200,220,255}, cw+4, paletteContentY+6, 10);
        bgHdr.Render(screen);
        paletteContentY += 24;

        constexpr int PAD   = 4;
        constexpr int LBL_H = 16;
        const int thumbW = PALETTE_W - PAD * 2;
        const int thumbH = thumbW / 2;             // 16:8 aspect
        const int itemH  = thumbH + LBL_H + PAD;

        int visCount = (window.GetHeight() - paletteContentY) / itemH;
        int startI   = mBgPaletteScroll;
        int endI     = std::min(startI + visCount + 1, (int)mBgItems.size());

        for (int i = startI; i < endI; i++) {
            int iy  = paletteContentY + PAD + (i - startI) * itemH;
            bool sel = (i == mSelectedBg);

            // Outer cell
            SDL_Rect cell = {cw + PAD, iy, thumbW, thumbH + LBL_H};
            DrawRect(screen, cell, sel ? SDL_Color{50,100,200,220} : SDL_Color{35,35,55,220});
            DrawOutline(screen, cell, sel ? SDL_Color{100,220,255,255} : SDL_Color{55,55,80,255}, sel ? 2 : 1);

            // Thumbnail
            SDL_Rect imgDst = {cw+PAD+1, iy+1, thumbW-2, thumbH-2};
            if (mBgItems[i].thumb)
                SDL_BlitSurfaceScaled(mBgItems[i].thumb, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
            else
                DrawRect(screen, imgDst, {40,40,70,255});

            // Label
            std::string lbl = mBgItems[i].label;
            if ((int)lbl.size() > 14) lbl = lbl.substr(0,13) + "~";
            Text lblT(lbl, SDL_Color{(Uint8)(sel?255:170),(Uint8)(sel?255:170),(Uint8)(sel?255:190),255},
                      cw+PAD+2, iy+thumbH+2, 10);
            lblT.Render(screen);
        }

        // Scroll indicator
        if ((int)mBgItems.size() > visCount) {
            float pct = (float)mBgPaletteScroll / std::max(1,(int)mBgItems.size()-visCount);
            int   sh  = std::max(20,(int)((window.GetHeight()-paletteContentY)*visCount/(float)mBgItems.size()));
            int   sy2 = paletteContentY + (int)((window.GetHeight()-paletteContentY-sh)*pct);
            DrawRect(screen, {cw+PALETTE_W-4, sy2, 3, sh}, {100,150,255,180});
        }
    }

    // ── Bottom status / hint bar ───────────────────────────────────────────────
    std::string counts = std::to_string(mLevel.coins.size())+"c  "+
                         std::to_string(mLevel.enemies.size())+"e  "+
                         std::to_string(mLevel.tiles.size())+"t";
    Text cntT(counts, SDL_Color{160,160,160,255}, 6, window.GetHeight()-22, 12);
    cntT.Render(screen);

    Text hintT("1:Coin 2:Enemy 3:Tile 4:Erase 5:Player 6:BG  I:Import  Ctrl+S:Save  Ctrl+Z:Undo",
               SDL_Color{100,100,100,255}, 150, window.GetHeight()-22, 11);
    hintT.Render(screen);

    // ── Import input bar ──────────────────────────────────────────────────────
    if (mImportInputActive) {
        int panelH = 44;
        int panelY = window.GetHeight() - 24 - panelH;
        DrawRect(screen, {0, panelY, cw, panelH}, {10, 20, 50, 240});
        DrawOutline(screen, {0, panelY, cw, panelH}, {80, 180, 255, 255}, 2);

        std::string dest = (mActiveTab == PaletteTab::Backgrounds)
                         ? "game_assets/backgrounds/"
                         : "game_assets/tiles/";
        Text importLbl("Import " + dest + "  (Enter=confirm, Esc=cancel)",
                       SDL_Color{140,200,255,255}, 8, panelY+4, 11);
        importLbl.Render(screen);

        int fieldX=8, fieldY=panelY+18, fieldW=cw-16, fieldH=20;
        DrawRect(screen,  {fieldX,fieldY,fieldW,fieldH}, {20,35,80,255});
        DrawOutline(screen,{fieldX,fieldY,fieldW,fieldH}, {80,180,255,200});
        Text inputText(mImportInputText+"|", SDL_Color{255,255,255,255}, fieldX+4, fieldY+2, 12);
        inputText.Render(screen);
    }

    // ── File-drop overlay (native drop, non-WSL2) ─────────────────────────────
    if (mDropActive) {
        DrawRect(screen, {0, TOOLBAR_H, cw, window.GetHeight()-TOOLBAR_H}, {20,80,160,80});
        constexpr int BORDER = 6;
        SDL_Color bc = {80,180,255,220};
        DrawRect(screen,{0,TOOLBAR_H,cw,BORDER},bc);
        DrawRect(screen,{0,window.GetHeight()-BORDER,cw,BORDER},bc);
        DrawRect(screen,{0,TOOLBAR_H,BORDER,window.GetHeight()-TOOLBAR_H},bc);
        DrawRect(screen,{cw-BORDER,TOOLBAR_H,BORDER,window.GetHeight()-TOOLBAR_H},bc);

        int cx2=cw/2, cy2=window.GetHeight()/2;
        DrawRect(screen,{cx2-220,cy2-44,440,88},{10,30,70,220});
        DrawOutline(screen,{cx2-220,cy2-44,440,88},{80,180,255,255},2);
        std::string hint = (mActiveTab==PaletteTab::Backgrounds)
                         ? "Drop .png to import as background"
                         : "Drop .png to import as tile";
        Text dropLine1(hint, SDL_Color{255,255,255,255}, cx2-168, cy2-32, 28);
        Text dropLine2("Saved to game_assets/"+(mActiveTab==PaletteTab::Backgrounds?std::string("backgrounds/"):std::string("tiles/")),
                       SDL_Color{140,200,255,255}, cx2-150, cy2+4, 18);
        dropLine1.Render(screen);
        dropLine2.Render(screen);
    }

    window.Update();
}

// ─── NextScene ────────────────────────────────────────────────────────────────
std::unique_ptr<Scene> LevelEditorScene::NextScene() {
    if (mLaunchGame) {
        mLaunchGame = false;
        return std::make_unique<GameScene>("levels/" + mLevelName + ".json");
    }
    return nullptr;
}

// ─── ImportDroppedTile ────────────────────────────────────────────────────────
bool LevelEditorScene::ImportDroppedTile(const std::string& srcPath) {
    fs::path src(srcPath);
    std::string ext = src.extension().string();
    // normalise to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") {
        SetStatus("Import failed: only .png supported (got " + ext + ")");
        return false;
    }

    // Route to correct directory based on active tab
    bool isBg = (mActiveTab == PaletteTab::Backgrounds);
    std::string destDirStr = isBg ? "game_assets/backgrounds" : "game_assets/tiles";
    fs::path destDir(destDirStr);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) { SetStatus("Import failed: can't create " + destDirStr); return false; }

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, ec);
        if (ec) { SetStatus("Import failed: " + ec.message()); return false; }
    }

    if (isBg) {
        // ── Add to background palette and apply immediately ───────────────────
        SDL_Surface* raw = IMG_Load(dest.string().c_str());
        if (!raw) { SetStatus("Import failed: can't load " + dest.string()); return false; }
        SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!converted) { SetStatus("Import failed: conversion error"); return false; }

        const int thumbW = PALETTE_W - 8;
        const int thumbH = thumbW / 2;
        SDL_Surface* thumb = SDL_CreateSurface(thumbW, thumbH, SDL_PIXELFORMAT_ARGB8888);
        if (thumb) {
            SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_NONE);
            SDL_Rect s2 = {0,0,converted->w,converted->h};
            SDL_Rect d2 = {0,0,thumbW,thumbH};
            SDL_BlitSurfaceScaled(converted, &s2, thumb, &d2, SDL_SCALEMODE_LINEAR);
            SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
        }
        SDL_DestroySurface(converted);

        mBgItems.push_back({dest.string(), dest.stem().string(), thumb});
        // Scroll to bottom and apply the new background
        mBgPaletteScroll = std::max(0, (int)mBgItems.size() - 1);
        ApplyBackground((int)mBgItems.size() - 1);
        SetStatus("Imported & applied: " + dest.filename().string());
    } else {
        // ── Add to tile palette ───────────────────────────────────────────────
        SDL_Surface* raw = IMG_Load(dest.string().c_str());
        if (!raw) { SetStatus("Import failed: can't load " + dest.string()); return false; }
        SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!converted) { SetStatus("Import failed: conversion error"); return false; }
        SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_BLEND);

        SDL_Surface* thumb = SDL_CreateSurface(PAL_ICON, PAL_ICON, SDL_PIXELFORMAT_ARGB8888);
        if (thumb) {
            SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_NONE);
            SDL_Rect ts={0,0,converted->w,converted->h}, td={0,0,PAL_ICON,PAL_ICON};
            SDL_BlitSurfaceScaled(converted, &ts, thumb, &td, SDL_SCALEMODE_LINEAR);
            SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
        }

        mPaletteItems.push_back({dest.string(), dest.stem().string(), thumb, converted});
        mSelectedTile = (int)mPaletteItems.size() - 1;
        mActiveTool   = Tool::Tile;
        if (lblTool) lblTool->CreateSurface("Tool: Tile");
        mActiveTab    = PaletteTab::Tiles;

        int totalRows  = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
        mPaletteScroll = std::max(0, totalRows - 1);
        SetStatus("Imported: " + dest.filename().string() + " -> auto-selected");
    }
    return true;
}
