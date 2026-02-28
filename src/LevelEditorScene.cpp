#include "LevelEditorScene.hpp"
#include "GameScene.hpp"

namespace fs = std::filesystem;

// ─── LoadPalette ─────────────────────────────────────────────────────────────
void LevelEditorScene::LoadPalette() {
    mPaletteItems.clear();

    // Collect PNGs from tiles/ and props/Props/
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

            // Convert to the screen's pixel format so blits never silently fail
            // due to format mismatch. SDL_PIXELFORMAT_ARGB8888 is universally
            // compatible with SDL_BlitSurfaceScaled on all platforms.
            SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (!converted) continue;
            SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_BLEND);

            // Pre-scaled thumbnail (PAL_ICON x PAL_ICON) for the sidebar grid
            SDL_Surface* thumb = SDL_CreateSurface(PAL_ICON, PAL_ICON, SDL_PIXELFORMAT_ARGB8888);
            if (thumb) {
                SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_NONE);
                SDL_Rect src = {0, 0, converted->w, converted->h};
                SDL_Rect dst = {0, 0, PAL_ICON, PAL_ICON};
                SDL_BlitSurfaceScaled(converted, &src, thumb, &dst, SDL_SCALEMODE_LINEAR);
                SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
            }

            PaletteItem item;
            item.path  = p.string();
            item.label = p.stem().string();
            item.thumb = thumb;
            item.full  = converted; // keep full-res for rendering placed tiles
            mPaletteItems.push_back(std::move(item));
        }
    }
}

// ─── Load ────────────────────────────────────────────────────────────────────
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;

    background = std::make_unique<Image>(
        "game_assets/base_pack/deepspace_scene.png", nullptr, FitMode::PRESCALED);
    coinSheet  = std::make_unique<SpriteSheet>(
        "game_assets/gold_coins/", "Gold_", 30, ICON_SIZE, ICON_SIZE);
    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    // Auto-load the last saved level so edits are never lost between sessions.
    // Only do this if mLevel is still in its default state (i.e. a fresh editor open,
    // not a reload after the scene was already populated in memory).
    if (mLevel.coins.empty() && mLevel.enemies.empty() && mLevel.tiles.empty()) {
        std::string autoPath = "levels/" + mLevelName + ".json";
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
        }
    }

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - 16);
        mLevel.player.y = static_cast<float>(window.GetHeight() - 60);
    }

    LoadPalette();

    // ── Toolbar layout ──────────────────────────────────────────────────────
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
    mWindow = nullptr;
}

// ─── HandleEvent ─────────────────────────────────────────────────────────────
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        // Scroll palette or adjust tile size
        if (mActiveTool == Tool::Tile) {
            int mx, my;
            float fmx, fmy; SDL_GetMouseState(&fmx, &fmy); mx=(int)fmx; my=(int)fmy;
            if (mx >= CanvasW()) {
                mPaletteScroll = std::max(0, mPaletteScroll - (int)e.wheel.y);
                // clamp: scroll is in rows of PAL_COLS
                int totalRows  = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
                int maxScroll  = std::max(0, totalRows - 1);
                mPaletteScroll = std::min(mPaletteScroll, maxScroll);
            } else {
                // Scroll over canvas adjusts tile size
                mTileW = std::max(GRID, mTileW + (int)e.wheel.y * GRID);
                mTileH = mTileW;
                SetStatus("Tile size: " + std::to_string(mTileW));
            }
        }
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_1: mActiveTool = Tool::Coin;        lblTool->CreateSurface("Tool: Coin");        break;
            case SDLK_2: mActiveTool = Tool::Enemy;       lblTool->CreateSurface("Tool: Enemy");       break;
            case SDLK_3: mActiveTool = Tool::Tile;        lblTool->CreateSurface("Tool: Tile");        break;
            case SDLK_4: mActiveTool = Tool::Erase;       lblTool->CreateSurface("Tool: Erase");       break;
            case SDLK_5: mActiveTool = Tool::PlayerStart; lblTool->CreateSurface("Tool: Player");      break;
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

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x;
        int my = (int)e.button.y;

        // Toolbar
        auto tb = [&](SDL_Rect r, Tool t, const std::string& lbl) {
            if (HitTest(r, mx, my)) { mActiveTool = t; lblTool->CreateSurface("Tool: "+lbl); return true; }
            return false;
        };
        if (tb(btnCoin,        Tool::Coin,        "Coin"))        return true;
        if (tb(btnEnemy,       Tool::Enemy,        "Enemy"))       return true;
        if (tb(btnTile,        Tool::Tile,         "Tile"))        return true;
        if (tb(btnErase,       Tool::Erase,        "Erase"))       return true;
        if (tb(btnPlayerStart, Tool::PlayerStart,  "Player"))      return true;

        if (HitTest(btnSave, mx, my)) {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name = mLevelName;
            SaveLevel(mLevel, path); SetStatus("Saved: " + path); return true;
        }
        if (HitTest(btnLoad, mx, my)) {
            std::string path = "levels/" + mLevelName + ".json";
            LoadLevel(path, mLevel) ? SetStatus("Loaded: "+path) : SetStatus("No file: "+path);
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

        // Palette click — select tile (mirrors 2-column grid layout in Render)
        if (mActiveTool == Tool::Tile && mx >= CanvasW() && my >= TOOLBAR_H) {
            constexpr int PAD   = 4;
            constexpr int LBL_H = 14;
            const int cellW = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
            const int cellH = cellW + LBL_H;
            const int itemH = cellH + PAD;
            int relX = mx - CanvasW() - PAD;
            int relY = my - TOOLBAR_H - PAD;
            int col  = relX / (cellW + PAD);
            int row  = relY / itemH;
            if (col >= 0 && col < PAL_COLS) {
                int idx = (mPaletteScroll + row) * PAL_COLS + col;
                if (idx >= 0 && idx < (int)mPaletteItems.size()) {
                    mSelectedTile = idx;
                    SetStatus("Selected: " + mPaletteItems[idx].label);
                }
            }
            return true;
        }

        // Canvas click
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
            if (ti >= 0) { mIsDragging=true; mDragIndex=ti; mDragIsTile=true; mDragIsCoin=false; return true; }
            int ci = HitCoin(mx, my);
            if (ci >= 0) { mIsDragging=true; mDragIndex=ci; mDragIsCoin=true;  mDragIsTile=false; return true; }
            int ei = HitEnemy(mx, my);
            if (ei >= 0) { mIsDragging=true; mDragIndex=ei; mDragIsCoin=false; mDragIsTile=false; return true; }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        mIsDragging = false;

    if (e.type == SDL_EVENT_MOUSE_MOTION && mIsDragging && mDragIndex >= 0) {
        int mx = (int)e.motion.x;
        int my = (int)e.motion.y;
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

    // Tiles — render using full-res surface for crisp visuals
    for (const auto& t : mLevel.tiles) {
        SDL_Surface* tileSurf = nullptr;
        for (const auto& item : mPaletteItems)
            if (item.path == t.imagePath) { tileSurf = item.full ? item.full : item.thumb; break; }
        SDL_Rect dst = {(int)t.x, (int)t.y, t.w, t.h};
        if (tileSurf) {
            SDL_BlitSurfaceScaled(tileSurf, nullptr, screen, &dst, SDL_SCALEMODE_LINEAR);
        } else {
            // Fallback: try loading directly if not in palette (e.g. after load from file)
            SDL_Surface* loaded = IMG_Load(t.imagePath.c_str());
            if (loaded) {
                SDL_BlitSurfaceScaled(loaded, nullptr, screen, &dst, SDL_SCALEMODE_LINEAR);
                SDL_DestroySurface(loaded);
            } else {
                DrawRect(screen, dst, {80,80,120,200});
            }
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
    SDL_Rect pr = {(int)mLevel.player.x,(int)mLevel.player.y,32,20};
    DrawRect(screen, pr, {0,200,80,180});
    DrawOutline(screen, pr, {0,255,100,255}, 2);

    // Tile ghost under cursor
    if (mActiveTool == Tool::Tile && !mPaletteItems.empty()) {
        float fmx2, fmy2; SDL_GetMouseState(&fmx2, &fmy2); int mx=(int)fmx2, my=(int)fmy2;
        if (my >= TOOLBAR_H && mx < cw) {
            auto [sx,sy] = SnapToGrid(mx, my);
            SDL_Rect ghost = {sx, sy, mTileW, mTileH};
            DrawRect(screen, ghost, {100,180,255,60});
            DrawOutline(screen, ghost, {100,180,255,200});
        }
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────
    DrawRect(screen, {0,0,window.GetWidth(),TOOLBAR_H}, {25,25,35,245});

    auto drawBtn = [&](SDL_Rect r, SDL_Color bg, SDL_Color border, Text* lbl, bool active) {
        if (active) bg = {70,140,255,255};
        DrawRect(screen, r, bg);
        DrawOutline(screen, r, border);
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

    // Status bar
    DrawRect(screen, {0,TOOLBAR_H,cw,22}, {18,18,26,220});
    if (lblStatus) lblStatus->Render(screen);
    if (lblTool)   lblTool->Render(screen);

    // ── Palette panel ─────────────────────────────────────────────────────────
    DrawRect(screen, {cw,0,PALETTE_W,window.GetHeight()}, {20,20,30,255});
    DrawOutline(screen, {cw,0,PALETTE_W,window.GetHeight()}, {60,60,80,255});

    // Palette header
    DrawRect(screen, {cw,0,PALETTE_W,TOOLBAR_H}, {30,30,45,255});
    Text palHdr("Tiles & Props", SDL_Color{200,200,220,255}, cw+4, 8,  11);
    Text palHdr2("Scroll: wheel",SDL_Color{120,120,140,255}, cw+4, 24, 10);
    Text palSize("Size: "+std::to_string(mTileW), SDL_Color{180,200,255,255}, cw+4, 40, 10);
    palHdr.Render(screen); palHdr2.Render(screen); palSize.Render(screen);

    // Palette items — 2-column grid, thumbnail fills cell, label below
    static constexpr int PAL_PADDING  = 4;   // gap between cells
    static constexpr int PAL_LBL_H    = 14;  // pixel rows reserved for the label
    const int cellW   = (PALETTE_W - PAL_PADDING * (PAL_COLS + 1)) / PAL_COLS;
    const int cellH   = cellW + PAL_LBL_H;   // square image + label strip
    const int itemH   = cellH + PAL_PADDING;
    const int visRows = (window.GetHeight() - TOOLBAR_H) / itemH;
    // scroll is in rows of PAL_COLS items
    const int startRow = mPaletteScroll;
    const int startI   = startRow * PAL_COLS;
    const int endI     = std::min(startI + (visRows + 1) * PAL_COLS,
                                  (int)mPaletteItems.size());

    for (int i = startI; i < endI; i++) {
        int col  = (i - startI) % PAL_COLS;
        int row  = (i - startI) / PAL_COLS;
        int ix   = cw + PAL_PADDING + col * (cellW + PAL_PADDING);
        int iy   = TOOLBAR_H + PAL_PADDING + row * itemH;

        bool sel = (i == mSelectedTile && mActiveTool == Tool::Tile);

        // Cell background
        SDL_Rect cell = {ix, iy, cellW, cellH};
        DrawRect(screen,   cell, sel ? SDL_Color{50,100,200,220} : SDL_Color{35,35,55,220});
        DrawOutline(screen, cell, sel ? SDL_Color{100,180,255,255} : SDL_Color{55,55,80,255});

        // Thumbnail — use the pre-scaled thumb for speed; fall back to full
        SDL_Surface* thumbSrc = mPaletteItems[i].thumb
                              ? mPaletteItems[i].thumb
                              : mPaletteItems[i].full;
        if (thumbSrc) {
            SDL_Rect imgDst = {ix + 1, iy + 1, cellW - 2, cellW - 2};
            SDL_BlitSurfaceScaled(thumbSrc, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
        } else {
            DrawRect(screen, {ix+1, iy+1, cellW-2, cellW-2}, {60,40,80,255});
        }

        // Label strip below the image
        std::string lbl = mPaletteItems[i].label;
        if ((int)lbl.size() > 9) lbl = lbl.substr(0, 8) + "~";
        Uint8 lr = sel ? 255 : 170, lg = sel ? 255 : 170, lb = sel ? 255 : 190;
        Text lblT(lbl, SDL_Color{lr, lg, lb, 255},
                  ix + 2, iy + cellW + 2, 9);
        lblT.Render(screen);
    }

    // Scroll indicator
    int totalRows = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
    if (totalRows > visRows) {
        float pct = (float)mPaletteScroll / std::max(1, totalRows - visRows);
        int   sh  = std::max(20, (int)((window.GetHeight() - TOOLBAR_H) * visRows / (float)totalRows));
        int   sy  = TOOLBAR_H + (int)((window.GetHeight() - TOOLBAR_H - sh) * pct);
        DrawRect(screen, {cw + PALETTE_W - 4, sy, 3, sh}, {100, 150, 255, 180});
    }

    // Entity counts
    std::string counts = std::to_string(mLevel.coins.size())+"c  "+
                         std::to_string(mLevel.enemies.size())+"e  "+
                         std::to_string(mLevel.tiles.size())+"t";
    Text cntT(counts, SDL_Color{160,160,160,255}, 6, window.GetHeight()-22, 12);
    cntT.Render(screen);

    Text hintT("1:Coin 2:Enemy 3:Tile 4:Erase 5:Player  Ctrl+S:Save  Ctrl+Z:Undo  Wheel:TileSize",
               SDL_Color{100,100,100,255}, 150, window.GetHeight()-22, 11);
    hintT.Render(screen);

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
