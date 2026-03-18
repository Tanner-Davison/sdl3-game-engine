#include "LevelEditorScene.hpp"
#include "AnimatedTile.hpp"
#include "EditorCanvasRenderer.hpp"
#include "EditorUIRenderer.hpp"
#include "GameScene.hpp"
#include "SurfaceUtils.hpp"
#include "TitleScene.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <climits>
#include <print>

namespace fs = std::filesystem;

// Shims kept for callers inside Load() / ImportPath() that use MakeThumb/LoadPNG
// directly (folder icon, bg import). These simply forward to the cache statics.
static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h) {
    return EditorSurfaceCache::MakeThumb(src, w, h);
}
static SDL_Surface* LoadPNG(const fs::path& p) {
    return EditorSurfaceCache::LoadPNG(p);
}

// --- OpenAnimPicker / CloseAnimPicker --------------------------------------
void LevelEditorScene::OpenAnimPicker(int tileIdx) {
    mActionAnimPickerTile = tileIdx;
    mAnimPickerEntries.clear();

    // "None" entry first — always available to clear any existing assignment
    mAnimPickerEntries.push_back({"", "None (no death anim)", nullptr});

    // Scan animated_tiles/ for all .json manifests
    auto manifests = ScanAnimatedTiles();
    for (const auto& p : manifests) {
        AnimatedTileDef def;
        if (!LoadAnimatedTileDef(p.string(), def))
            continue;
        SDL_Surface* thumb =
            mSurfaceCache.GetDestroyAnimThumb(p.string()); // builds & caches 48x48
        mAnimPickerEntries.push_back({p.string(), def.name, thumb});
    }

    // Layout: popup centred horizontally over the tile, above or below depending on space.
    // We don't know window size here, so defer rect computation to Render.
    // Just mark as open — Render computes mActionAnimPickerRect each frame.
}

void LevelEditorScene::CloseAnimPicker() {
    mActionAnimPickerTile = -1;
    mAnimPickerEntries.clear();
}

// --- GetPowerUpRegistry ----------------------------------------------------
// The single authoritative list of power-up types for both the editor UI and
// the game runtime. To add a new power-up:
//   1. Add an entry here (id, label, duration)
//   2. Add a PowerUpType enum value in Components.hpp
//   3. Handle the id string in GameScene::Spawn() -> PowerUpTag
//   4. Handle the PowerUpType in MovementSystem / GameScene::Update
const std::vector<LevelEditorScene::PowerUpEntry>& LevelEditorScene::GetPowerUpRegistry() {
    static const std::vector<PowerUpEntry> kRegistry = {
        {"antigravity", "Anti-Gravity (15s)", 15.0f},
        // Add future power-ups here:
        // {"speedboost",  "Speed Boost (10s)",  10.0f},
        // {"invincible",  "Invincibility (8s)",  8.0f},
    };
    return kRegistry;
}

// --- LoadTileView / LoadBgPalette / ApplyBackground -----------------------
// Implementations moved to EditorPalette.cpp. LevelEditorScene delegates
// via inline wrappers in the header. The old ApplyBackground is replaced below.
void LevelEditorScene::ApplyBackground(int idx) {
    mPalette.ApplyBackground(idx, mLevel, [this](const std::string& bgPath) {
        background = std::make_unique<Image>(bgPath, FitModeFromString(mLevel.bgFitMode));
        auto&       items = mPalette.BgItems();
        int         i     = mPalette.SelectedBg();
        std::string label = (i >= 0 && i < (int)items.size()) ? items[i].label : bgPath;
        SetStatus("Background: " + label + "  [" + mLevel.bgFitMode + "]");
    });
}

// ─── Load ─────────────────────────────────────────────────────────────────────
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;
    // Disable SDL motion event coalescing so every mouse move is delivered
    // immediately — without this macOS batches motion events causing pan lag.
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    SDL_SetHint("SDL_MOUSE_TOUCH_EVENTS", "0");

    background = std::make_unique<Image>("game_assets/backgrounds/deepspace_scene.png",
                                         FitModeFromString(mLevel.bgFitMode));
    coinSheet  = std::make_unique<SpriteSheet>(
        "game_assets/gold_coins/", "Gold_", 30, ICON_SIZE, ICON_SIZE);
    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    // Determine which level file to load on startup.
    // Three cases:
    //   mForceNew == true          → skip all loading, start blank
    //   mOpenPath non-empty        → load that specific file
    //   mOpenPath empty, no force  → auto-resume levels/level1.json if it exists
    if (!mForceNew && mLevel.coins.empty() && mLevel.enemies.empty() &&
        mLevel.tiles.empty()) {
        std::string autoPath;
        if (!mOpenPath.empty()) {
            autoPath = mOpenPath;
            fs::path p(mOpenPath);
            mLevelName = p.stem().string();
        } else {
            autoPath = "levels/" + mLevelName + ".json";
        }
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
            if (!mLevel.background.empty())
                background = std::make_unique<Image>(mLevel.background,
                                                     FitModeFromString(mLevel.bgFitMode));
        } else if (!mOpenPath.empty()) {
            // Path given but file doesn't exist yet — new level with that name
            SetStatus("New level: " + mLevelName);
        }
    } else if (mForceNew) {
        // Apply the name chosen in the title-screen modal, if one was given
        if (!mPresetName.empty())
            mLevelName = mPresetName;
        SetStatus("New level: " + mLevelName);
    }

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - PLAYER_STAND_WIDTH / 2);
        mLevel.player.y =
            static_cast<float>(window.GetHeight() - PLAYER_STAND_HEIGHT - GRID * 2);
    }

    // Load the generic folder icon once — shared by all folder palette cells.
    // We keep it alive for the lifetime of the editor scene.
    if (!mFolderIcon) {
        SDL_Surface* raw = IMG_Load("game_assets/generic_folder.png");
        if (raw) {
            SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (conv) {
                mFolderIcon = MakeThumb(conv, PAL_ICON, PAL_ICON);
                SDL_DestroySurface(conv);
            }
        }
    }

    // Initialize the palette subsystem with the shared surface cache and
    // folder icon, then load tile and background palettes.
    mPalette.Init(mSurfaceCache, mFolderIcon);
    LoadTileView(TILE_ROOT);
    LoadBgPalette();

    // ── Toolbar layout ────────────────────────────────────────────────────────
    mToolbar.RebuildLayout();
    mToolbar.CreateLabels();

    // Set gravity label to match current level mode
    {
        std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)     ? "Wall Run"
                           : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                            : "Platform";
        mToolbar.SetGravityLabel(gLbl);
    }

    lblStatus = std::make_unique<Text>(
        mStatusMsg, SDL_Color{180, 180, 200, 255}, BTN_GAP, TOOLBAR_H + 4, 12);
    lblTool = std::make_unique<Text>(
        "Pan", SDL_Color{255, 215, 0, 255}, window.GetWidth() - PALETTE_W - 120, 22, 13);

    // Initialize the default tool
    SwitchTool(ToolId::MoveCam);
}

// --- Unload ----------------------------------------------------------------
void LevelEditorScene::Unload() {
    // Deactivate the current tool before teardown
    if (mTool) {
        auto ctx = MakeToolCtx();
        mTool->OnDeactivate(ctx);
        mTool.reset();
    }

    // Palette owns all tile/bg item surfaces -- free them in one call.
    mPalette.Clear();

    if (mFolderIcon) {
        SDL_DestroySurface(mFolderIcon);
        mFolderIcon = nullptr;
    }

    // Free all cached surfaces (rotation, badge, destroy-anim, tile, extra)
    mSurfaceCache.Clear();

    mWindow = nullptr;
}

// --- HandleEvent -----------------------------------------------------------
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT)
        return false;

    // ── File / folder drop ────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_BEGIN) {
        mDropActive = true;
        SetStatus("Drop a .png or folder...");
        return true;
    }
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mDropActive = false;
        return true;
    }
    if (e.type == SDL_EVENT_DROP_FILE) {
        mDropActive      = false;
        std::string path = e.drop.data ? std::string(e.drop.data) : "";
        if (!path.empty()) {
            // If Action tool is active and the dropped file is an animated tile JSON,
            // assign it as the destroy animation for whatever action tile the cursor
            // is over. Don't import it into the palette.
            if (mActiveToolId == ToolId::Action && IsAnimatedTile(path)) {
                float fmx, fmy;
                SDL_GetMouseState(&fmx, &fmy);
                int mx = (int)fmx, my = (int)fmy;
                int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
                if (ti >= 0 && mLevel.tiles[ti].action) {
                    mLevel.tiles[ti].actionDestroyAnim = path;
                    // Preload the thumbnail now so it's ready to render immediately
                    GetDestroyAnimThumb(path);
                    fs::path p(path);
                    SetStatus("Tile " + std::to_string(ti) + ": death anim \"" +
                              p.stem().string() + "\" assigned");
                } else {
                    SetStatus("Drop a .json onto an Action tile to assign its death anim");
                }
                mActionAnimDropHover = -1;
                return true;
            }
            ImportPath(path);
        }
        return true;
    }

    // Track which action tile the cursor is hovering during an active file drag
    // (SDL sends DROP_BEGIN but not continuous position events, so we do this
    // in motion events below; this just resets when the drop window closes)
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mActionAnimDropHover = -1;
    }

    // ── Moving-platform popup: text input for speed field ─────────────────────
    if (mMovPlatSpeedInput) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            // Only allow digits
            for (char ch : std::string(e.text.text))
                if (ch >= '0' && ch <= '9')
                    mMovPlatSpeedStr += ch;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !mMovPlatSpeedStr.empty()) {
                mMovPlatSpeedStr.pop_back();
                return true;
            }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER ||
                e.key.key == SDLK_ESCAPE || e.key.key == SDLK_TAB) {
                // Commit value
                if (!mMovPlatSpeedStr.empty()) {
                    int v            = std::clamp(std::stoi(mMovPlatSpeedStr), 10, 2000);
                    mMovPlatSpeed    = (float)v;
                    mMovPlatSpeedStr = std::to_string(v);
                    // Apply to current session tiles
                    for (int idx : mMovPlatIndices)
                        mLevel.tiles[idx].moveSpeed = mMovPlatSpeed;
                    // Also apply to ALL moving tiles in the current group
                    for (auto& t : mLevel.tiles) {
                        if (!t.moving)
                            continue;
                        bool inGroup =
                            (mMovPlatCurGroupId != 0 &&
                             t.moveGroupId == mMovPlatCurGroupId) ||
                            std::any_of(mMovPlatIndices.begin(),
                                        mMovPlatIndices.end(),
                                        [&](int i) { return &t == &mLevel.tiles[i]; });
                        if (inGroup)
                            t.moveSpeed = mMovPlatSpeed;
                    }
                }
                mMovPlatSpeedInput = false;
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
        }
        return true; // swallow all other input while field is focused
    }

    // ── Import text input ─────────────────────────────────────────────────────
    if (mImportInputActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            mImportInputText += e.text.text;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_ESCAPE:
                    mImportInputActive = false;
                    mImportInputText.clear();
                    SetStatus("Import cancelled");
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    return true;
                case SDLK_BACKSPACE:
                    if (!mImportInputText.empty())
                        mImportInputText.pop_back();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: {
                    std::string path   = mImportInputText;
                    mImportInputActive = false;
                    mImportInputText.clear();
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    if (!path.empty())
                        ImportPath(path);
                    return true;
                }
                default:
                    break;
            }
        }
        return true;
    }

    // ── Delete confirmation popup ──────────────────────────────────────────
    if (mDelConfirmActive) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mDelConfirmActive = false;
            SetStatus("Delete cancelled");
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (HitTest(mDelConfirmYes, mx, my)) {
                std::error_code ec;
                if (mDelConfirmIsDir)
                    fs::remove_all(mDelConfirmPath, ec);
                else
                    fs::remove(mDelConfirmPath, ec);
                mDelConfirmActive = false;
                // Refresh the right palette depending on what was deleted
                bool wasBg = (mDelConfirmPath.rfind(BG_ROOT, 0) == 0);
                if (wasBg)
                    LoadBgPalette();
                else
                    LoadTileView(mPalette.CurrentDir());
                SetStatus((mDelConfirmIsDir ? "Deleted folder: " : "Deleted: ") +
                          mDelConfirmName);
                return true;
            }
            if (HitTest(mDelConfirmNo, mx, my)) {
                mDelConfirmActive = false;
                SetStatus("Delete cancelled");
                return true;
            }
        }
        return true; // swallow all other input while popup is open
    }

    // ── Pan: middle-mouse drag OR Ctrl + left-mouse drag ────────────────────
    auto startPan = [&](int mx, int my) { mCamera.StartPan(mx, my); };

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mx < CanvasW() && my >= TOOLBAR_H) {
            startPan(mx, my);
            return true;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (SDL_GetModState() & SDL_KMOD_CTRL) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx < CanvasW() && my >= TOOLBAR_H) {
                startPan(mx, my);
                return true;
            }
        }
    }
    // Left-drag with the MoveCam tool pans the camera.
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
        !(SDL_GetModState() & SDL_KMOD_CTRL) && mActiveToolId == ToolId::MoveCam) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mx < CanvasW() && my >= TOOLBAR_H) {
            startPan(mx, my);
            return true;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_LEFT)) {
        if (mCamera.IsPanning()) {
            mCamera.StopPan();
            return true;
        }
    }
    // Pan motion is handled in Update() by polling SDL_GetMouseState every frame
    // for smoothness — no event handler needed here.

    // ── Mouse wheel ───────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;

        // When MovingPlat tool is active, Ctrl+scroll adjusts range instead of zooming.
        // Adjusts the hovered tile's group, OR the current session group if no hover.
        // Start position stays fixed; only the end (range) moves.
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && mActiveToolId == ToolId::MovingPlat &&
            mx < CanvasW()) {
            mMovPlatRange = std::max(GRID * 1.0f, mMovPlatRange + e.wheel.y * GRID);
            // Update current session tiles
            for (int idx : mMovPlatIndices)
                mLevel.tiles[idx].moveRange = mMovPlatRange;
            // Also update the hovered tile's group (handles already-placed platforms)
            int hovTi = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            if (hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                int grp = mLevel.tiles[hovTi].moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moveRange = mMovPlatRange;
                }
            }
            SetStatus("MovePlat range=" + std::to_string((int)mMovPlatRange));
            return true;
        }

        // Ctrl+scroll = zoom in/out, anchored to mouse position
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && mx < CanvasW()) {
            if (mCamera.ApplyZoom(e.wheel.y, mx, my)) {
                SetStatus("Zoom: " + std::to_string(mCamera.ZoomPercent()) +
                          "%  (Ctrl+scroll)");
            }
            return true;
        }

        if (mx >= CanvasW()) {
            if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles) {
                int scroll = std::max(0, mPalette.TileScroll() - (int)e.wheel.y);
                int rows   = ((int)mPalette.Items().size() + PAL_COLS - 1) / PAL_COLS;
                scroll     = std::min(scroll, std::max(0, rows - 1));
                mPalette.SetTileScroll(scroll);
            } else {
                int scroll = std::max(0, mPalette.BgScroll() - (int)e.wheel.y);
                scroll = std::min(scroll, std::max(0, (int)mPalette.BgItems().size() - 1));
                mPalette.SetBgScroll(scroll);
            }
        } else if (mActiveToolId == ToolId::Tile && mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, e.wheel.y, mx, my, SDL_GetModState());
        } else if (mActiveToolId == ToolId::Action) {
            // Accumulate fractional SDL3 scroll ticks, step by 1 hit per full tick
            float fmya;
            SDL_GetMouseState(nullptr, &fmya);
            int mya       = (int)fmya;
            int hovAction = (mya >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, mya) : -1;
            if (hovAction >= 0 && mLevel.tiles[hovAction].action) {
                mScrollAccum += e.wheel.y;
                int steps = (int)mScrollAccum;
                if (steps != 0) {
                    mScrollAccum -= steps;
                    int& hits = mLevel.tiles[hovAction].actionHits;
                    hits      = std::clamp(hits + steps, 1, 99);
                    SetStatus("Action tile hits: " + std::to_string(hits));
                }
            } else {
                mScrollAccum = 0.0f; // not hovering a tile, discard accumulation
            }
        } else if (mActiveToolId == ToolId::Slope && mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, e.wheel.y, mx, my, SDL_GetModState());
        } else if (mActiveToolId == ToolId::MovingPlat) {
            float fmx2, fmy2;
            SDL_GetMouseState(&fmx2, &fmy2);
            int  mxw = (int)fmx2, myw = (int)fmy2;
            int  hovTi = (myw >= TOOLBAR_H && mxw < CanvasW()) ? HitTile(mxw, myw) : -1;
            bool ctrl  = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
            bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (ctrl && hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                // Ctrl+scroll: adjust starting phase for the tile AND its whole group
                auto& ht       = mLevel.tiles[hovTi];
                float newPhase = ht.movePhase + e.wheel.y * 0.05f;
                // Wrap instead of clamp so you can scroll continuously
                if (newPhase < 0.0f)
                    newPhase += 1.0f;
                if (newPhase > 1.0f)
                    newPhase -= 1.0f;
                int grp = ht.moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &ht))
                        t.movePhase = newPhase;
                }
                SetStatus((grp != 0 ? "Group " + std::to_string(grp)
                                    : "Tile " + std::to_string(hovTi)) +
                          "  phase=" + std::to_string(newPhase).substr(0, 4) +
                          "  dir=" + (ht.moveLoopDir > 0 ? "+1(right)" : "-1(left)"));
            } else if (shift && hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                // Shift+scroll: flip start direction for the tile AND its whole group
                int newDir = (e.wheel.y > 0) ? 1 : -1;
                int grp    = mLevel.tiles[hovTi].moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moveLoopDir = newDir;
                }
                SetStatus(
                    (grp != 0 ? "Group " + std::to_string(grp)
                              : "Tile " + std::to_string(hovTi)) +
                    "  dir=" + (newDir > 0 ? "+1 (starts right)" : "-1 (starts left)"));
            } else {
                // Plain scroll: adjust range for current group
                mMovPlatRange = std::max(48.0f, mMovPlatRange + (int)e.wheel.y * GRID);
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveRange = mMovPlatRange;
                SetStatus("MovePlat range=" + std::to_string((int)mMovPlatRange) +
                          "  spd=" + std::to_string((int)mMovPlatSpeed) +
                          (mMovPlatLoop ? "  LOOP" : "") +
                          (mMovPlatTrigger ? "  TRIGGER" : ""));
            }
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_Q:
                SwitchTool(ToolId::Select);
                lblTool->CreateSurface("Select");
                break;
            case SDLK_1:
                SwitchTool(ToolId::Coin);
                lblTool->CreateSurface("Coin");
                break;
            case SDLK_2:
                SwitchTool(ToolId::Enemy);
                lblTool->CreateSurface("Enemy");
                break;
            case SDLK_3:
                SwitchTool(ToolId::Tile);
                lblTool->CreateSurface("Tile");
                mPalette.SetActiveTab(EditorPalette::Tab::Tiles);
                break;
            case SDLK_4:
                SwitchTool(ToolId::Erase);
                lblTool->CreateSurface("Erase");
                break;
            case SDLK_5:
                SwitchTool(ToolId::PlayerStart);
                lblTool->CreateSurface("Player");
                break;
            case SDLK_6:
                mPalette.SetActiveTab(EditorPalette::Tab::Backgrounds);
                lblTool->CreateSurface("Backgrounds");
                break;
            case SDLK_7:
            case SDLK_R:
                SwitchTool(ToolId::Resize);
                lblTool->CreateSurface("Resize");
                break;
            case SDLK_8:
            case SDLK_P:
                SwitchTool(ToolId::Prop);
                lblTool->CreateSurface("Prop");
                break;
            case SDLK_9:
            case SDLK_L:
                SwitchTool(ToolId::Ladder);
                lblTool->CreateSurface("Ladder");
                break;
            case SDLK_0:
                SwitchTool(ToolId::Action);
                lblTool->CreateSurface("Action");
                CloseAnimPicker();
                break;
            case SDLK_T:
                SwitchTool(ToolId::MoveCam);
                lblTool->CreateSurface("Pan");
                break;
            case SDLK_MINUS:
                SwitchTool(ToolId::Slope);
                lblTool->CreateSurface("Slope");
                break;
            case SDLK_G: {
                if (mLevel.gravityMode == GravityMode::Platformer)
                    mLevel.gravityMode = GravityMode::WallRun;
                else if (mLevel.gravityMode == GravityMode::WallRun)
                    mLevel.gravityMode = GravityMode::OpenWorld;
                else
                    mLevel.gravityMode = GravityMode::Platformer;
                std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun) ? "Wall Run"
                                   : (mLevel.gravityMode == GravityMode::OpenWorld)
                                       ? "Open World"
                                       : "Platform";
                std::string gStatus = (mLevel.gravityMode == GravityMode::WallRun)
                                          ? "Mode: Wall Run"
                                      : (mLevel.gravityMode == GravityMode::OpenWorld)
                                          ? "Mode: Open World (top-down)"
                                          : "Mode: Platformer";
                mToolbar.SetGravityLabel(gLbl);
                SetStatus(gStatus);
                break;
            }

            case SDLK_I:
                mImportInputActive = true;
                mImportInputText.clear();
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                SetStatus(mPalette.ActiveTab() == EditorPalette::Tab::Backgrounds
                              ? "Import bg path or folder (Enter=go, Esc=cancel):"
                              : "Import tile path or folder (Enter=go, Esc=cancel):");
                break;
            case SDLK_S:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    fs::create_directories("levels");
                    std::string path = "levels/" + mLevelName + ".json";
                    mLevel.name      = mLevelName;
                    SaveLevel(mLevel, path);
                    SetStatus("Saved: " + path);
                }
                break;
            case SDLK_Z:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    if (!mLevel.tiles.empty()) {
                        mLevel.tiles.pop_back();
                        SetStatus("Undo tile");
                    } else if (!mLevel.coins.empty()) {
                        mLevel.coins.pop_back();
                        SetStatus("Undo coin");
                    } else if (!mLevel.enemies.empty()) {
                        mLevel.enemies.pop_back();
                        SetStatus("Undo enemy");
                    }
                }
                break;
            case SDLK_DELETE:
            case SDLK_BACKSPACE:
                // Delegate to SelectTool if active
                if (mTool) {
                    auto ctx = MakeToolCtx();
                    mTool->OnKeyDown(ctx, e.key.key, (SDL_Keymod)e.key.mod);
                }
                break;
            case SDLK_TAB:
                mPalette.ToggleCollapsed();
                SetStatus(mPalette.IsCollapsed() ? "Palette hidden (Tab to show)"
                                                 : "Palette visible (Tab to hide)");
                break;
            case SDLK_ESCAPE:
                if (mActionAnimPickerTile >= 0) {
                    CloseAnimPicker();
                    break;
                }
                if (mActiveToolId == ToolId::Select && mTool) {
                    auto ctx = MakeToolCtx();
                    if (mTool->OnKeyDown(ctx, e.key.key, (SDL_Keymod)e.key.mod) ==
                        ToolResult::Consumed)
                        break; // don't fall through to the tile-browser Esc handler
                }
                // Navigate back up in tile browser
                if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles &&
                    mPalette.CurrentDir() != TILE_ROOT) {
                    fs::path    parent = fs::path(mPalette.CurrentDir()).parent_path();
                    std::string up     = parent.string();
                    if (up.rfind(TILE_ROOT, 0) != 0)
                        up = TILE_ROOT;
                    LoadTileView(up);
                }
                break;
            default:
                break;
        }
    }

    // ── Mouse button down ─────────────────────────────────────────────────────
    // ── Right-click: group cycling for action tiles ──────────────────────────
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mActiveToolId == ToolId::Action && my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0 && mLevel.tiles[ti].action) {
                // Right-click cycles through available death animations:
                // None -> anim1 -> anim2 -> ... -> last -> None -> ...
                // The thumbnail badge updates immediately on each click.
                {
                    auto  manifests = ScanAnimatedTiles();
                    auto& cur       = mLevel.tiles[ti].actionDestroyAnim;
                    if (manifests.empty()) {
                        cur.clear();
                        SetStatus("No death anims in animated_tiles/");
                    } else {
                        int idx = -1; // -1 = currently None
                        for (int m = 0; m < (int)manifests.size(); m++)
                            if (manifests[m].string() == cur) {
                                idx = m;
                                break;
                            }
                        idx++; // advance: None->0, 0->1, ..., last->None
                        if (idx >= (int)manifests.size()) {
                            cur.clear();
                            SetStatus("Tile " + std::to_string(ti) + ": death anim -> None");
                        } else {
                            cur = manifests[idx].string();
                            GetDestroyAnimThumb(cur); // preload thumbnail
                            SetStatus("Tile " + std::to_string(ti) + ": death anim -> " +
                                      manifests[idx].stem().string());
                        }
                    }
                }
                return true;
            }
        }
    }

    // Right-click on canvas: cycle tile rotation / action group / moving-plat params
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mActiveToolId == ToolId::MovingPlat && my >= TOOLBAR_H && mx < CanvasW()) {
            // Preset cycle:
            //  H  96  60  (default horiz, medium)
            //  H  48  40  (horiz, short)
            //  H 192  80  (horiz, long)
            //  V  96  60  (vert, medium)
            //  V 192  50  (vert, long)
            //  V  48  40  (vert, short)
            //  → back to H 96 60
            if (mMovPlatHoriz && mMovPlatRange == 96 && mMovPlatSpeed == 60 &&
                !mMovPlatLoop) {
                mMovPlatHoriz = true;
                mMovPlatRange = 48;
                mMovPlatSpeed = 40;
                mMovPlatLoop  = false;
            } else if (mMovPlatHoriz && mMovPlatRange == 48 && !mMovPlatLoop) {
                mMovPlatHoriz = true;
                mMovPlatRange = 192;
                mMovPlatSpeed = 80;
                mMovPlatLoop  = false;
            } else if (mMovPlatHoriz && mMovPlatRange == 192 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 96;
                mMovPlatSpeed = 60;
                mMovPlatLoop  = false;
            } else if (!mMovPlatHoriz && mMovPlatRange == 96 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 192;
                mMovPlatSpeed = 50;
                mMovPlatLoop  = false;
            } else if (!mMovPlatHoriz && mMovPlatRange == 192 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 48;
                mMovPlatSpeed = 40;
                mMovPlatLoop  = false;
            } else if (!mMovPlatLoop) {
                mMovPlatHoriz   = true;
                mMovPlatRange   = 1800;
                mMovPlatSpeed   = 150;
                mMovPlatLoop    = true;
                mMovPlatTrigger = true;
            } else {
                mMovPlatHoriz   = true;
                mMovPlatRange   = 96;
                mMovPlatSpeed   = 60;
                mMovPlatLoop    = false;
                mMovPlatTrigger = false;
            }
            // Update all tiles already in the group
            for (int idx : mMovPlatIndices) {
                mLevel.tiles[idx].moveHoriz   = mMovPlatHoriz;
                mLevel.tiles[idx].moveRange   = mMovPlatRange;
                mLevel.tiles[idx].moveSpeed   = mMovPlatSpeed;
                mLevel.tiles[idx].moveLoop    = mMovPlatLoop;
                mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
            }
            SetStatus(std::string(mMovPlatHoriz ? "H" : "V") +
                      "  range=" + std::to_string((int)mMovPlatRange) + "  spd=" +
                      std::to_string((int)mMovPlatSpeed) + "  (RClick cycles presets)");
            return true;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                if (mActiveToolId == ToolId::Action && mLevel.tiles[ti].action) {
                    int& grp = mLevel.tiles[ti].actionGroup;
                    grp      = (grp + 1) % 10;
                    SetStatus("Tile " + std::to_string(ti) + " group -> " +
                              (grp == 0 ? "standalone" : std::to_string(grp)));
                } else {
                    int& rot = mLevel.tiles[ti].rotation;
                    rot      = (rot + 90) % 360;
                    SetStatus("Tile " + std::to_string(ti) + " rotated to " +
                              std::to_string(rot) + "deg");
                }
                return true;
            } else if (mActiveToolId == ToolId::Tile && mTool) {
                // Delegate right-click to TileTool (cycles ghost rotation)
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, mx, my, SDL_BUTTON_RIGHT, SDL_GetModState());
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;

        // ── Destroy-anim picker — handle clicks inside the popup ──────────────────
        if (mActionAnimPickerTile >= 0 && !mAnimPickerEntries.empty()) {
            if (HitTest(mActionAnimPickerRect, mx, my)) {
                // Re-derive cell geometry (must match Render exactly)
                const int THUMB    = 48;
                const int ROW_H    = THUMB + 10;
                const int PAD      = 8;
                const int COL_W    = THUMB + PAD * 2;
                const int COLS     = 4;
                const int TITLE_H  = 28;
                int       px       = mActionAnimPickerRect.x;
                int       py       = mActionAnimPickerRect.y;
                int       ey       = py + TITLE_H;
                int       nEntries = (int)mAnimPickerEntries.size();
                for (int i = 0; i < nEntries; i++) {
                    int      col  = i % COLS;
                    int      row  = i / COLS;
                    int      ex   = px + PAD + col * COL_W;
                    int      ey2  = ey + PAD + row * (ROW_H + PAD);
                    SDL_Rect cell = {ex, ey2, COL_W - PAD, ROW_H};
                    if (HitTest(cell, mx, my)) {
                        const auto& entry = mAnimPickerEntries[i];
                        if (mActionAnimPickerTile < (int)mLevel.tiles.size()) {
                            mLevel.tiles[mActionAnimPickerTile].actionDestroyAnim =
                                entry.path;
                            if (!entry.path.empty())
                                GetDestroyAnimThumb(entry.path);
                            SetStatus("Tile " + std::to_string(mActionAnimPickerTile) +
                                      ": death anim → " +
                                      (entry.path.empty() ? "None" : entry.name));
                        }
                        CloseAnimPicker();
                        return true;
                    }
                }
                return true; // click inside popup but not on a cell — absorb
            } else {
                // Click outside popup — close without changing anything
                CloseAnimPicker();
                // Fall through so the click still acts on the canvas
            }
        }

        // Palette collapse toggle tab — a narrow strip at the left edge of the panel
        {
            int      cw          = CanvasW();
            SDL_Rect collapseBtn = {cw, TOOLBAR_H, PALETTE_TAB_W, 28};
            if (HitTest(collapseBtn, mx, my)) {
                mPalette.ToggleCollapsed();
                return true;
            }
        }

        // Tab bar (only when expanded)
        if (!mPalette.IsCollapsed() && mx >= CanvasW() + PALETTE_TAB_W && my >= TOOLBAR_H &&
            my < TOOLBAR_H + TAB_H) {
            int hw = (PALETTE_W - PALETTE_TAB_W) / 2;
            mPalette.SetActiveTab((mx < CanvasW() + PALETTE_TAB_W + hw)
                                      ? EditorPalette::Tab::Tiles
                                      : EditorPalette::Tab::Backgrounds);
            return true;
        }

        // Toolbar group collapse pills (strip lives below the buttons)
        for (int gi = 0; gi < static_cast<int>(TBGrp::COUNT); ++gi) {
            auto grp = static_cast<TBGrp>(gi);
            if (HitTest(mToolbar.PillRect(grp), mx, my)) {
                mToolbar.ToggleGroup(grp);
                static const char* kGrpNames[] = {"Place", "Modifier", "Actions"};
                SetStatus(std::string(kGrpNames[gi]) + (mToolbar.IsCollapsed(grp)
                                                            ? " group collapsed"
                                                            : " group expanded"));
                return true;
            }
        }

        // Toolbar
        // Any toolbar click closes the anim picker
        if (mActionAnimPickerTile >= 0 && my < TOOLBAR_H)
            CloseAnimPicker();

        // Toolbar button dispatch via EditorToolbar::HandleClick
        if (my < TOOLBAR_H) {
            auto click = mToolbar.HandleClick(mx, my);
            if (click.kind == EditorToolbar::ClickResult::Kind::Button) {
                CloseAnimPicker();
                switch (click.button) {
                    case TBBtn::Coin:
                        SwitchTool(ToolId::Coin);
                        lblTool->CreateSurface("Coin");
                        return true;
                    case TBBtn::Enemy:
                        SwitchTool(ToolId::Enemy);
                        lblTool->CreateSurface("Enemy");
                        return true;
                    case TBBtn::Tile:
                        SwitchTool(ToolId::Tile);
                        lblTool->CreateSurface("Tile");
                        return true;
                    case TBBtn::Erase:
                        SwitchTool(ToolId::Erase);
                        lblTool->CreateSurface("Erase");
                        return true;
                    case TBBtn::PlayerStart:
                        SwitchTool(ToolId::PlayerStart);
                        lblTool->CreateSurface("Player");
                        return true;
                    case TBBtn::Select:
                        SwitchTool(ToolId::Select);
                        lblTool->CreateSurface("Select");
                        return true;
                    case TBBtn::MoveCam:
                        SwitchTool(ToolId::MoveCam);
                        lblTool->CreateSurface("Pan");
                        return true;
                    case TBBtn::Prop:
                        SwitchTool(ToolId::Prop);
                        lblTool->CreateSurface("Prop");
                        return true;
                    case TBBtn::Ladder:
                        SwitchTool(ToolId::Ladder);
                        lblTool->CreateSurface("Ladder");
                        return true;
                    case TBBtn::Action:
                        SwitchTool(ToolId::Action);
                        lblTool->CreateSurface("Action");
                        return true;
                    case TBBtn::Slope:
                        SwitchTool(ToolId::Slope);
                        lblTool->CreateSurface("Slope");
                        return true;
                    case TBBtn::Resize:
                        SwitchTool(ToolId::Resize);
                        lblTool->CreateSurface("Resize");
                        return true;
                    case TBBtn::Hitbox:
                        SwitchTool(ToolId::Hitbox);
                        lblTool->CreateSurface("Hitbox");
                        return true;
                    case TBBtn::Hazard:
                        SwitchTool(ToolId::Hazard);
                        lblTool->CreateSurface("Hazard");
                        return true;
                    case TBBtn::AntiGrav:
                        SwitchTool(ToolId::AntiGrav);
                        lblTool->CreateSurface("Float");
                        return true;
                    case TBBtn::PowerUp:
                        SwitchTool(ToolId::PowerUp);
                        mPowerUpPopupOpen = true;
                        mPowerUpTileIdx   = -1;
                        lblTool->CreateSurface("PowerUp");
                        SetStatus("PowerUp: click a tile to assign a power-up pickup");
                        return true;
                    case TBBtn::MovingPlat: {
                        SwitchTool(ToolId::MovingPlat);
                        lblTool->CreateSurface("MovingPlat");
                        int maxUsed = 0;
                        for (const auto& ts : mLevel.tiles)
                            if (ts.moveGroupId > maxUsed)
                                maxUsed = ts.moveGroupId;
                        mMovPlatNextGroupId = maxUsed + 1;
                        mMovPlatCurGroupId  = mMovPlatNextGroupId++;
                        mMovPlatIndices.clear();
                        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                            if (!mLevel.tiles[i].moving)
                                continue;
                            const auto& first = mLevel.tiles[i];
                            mMovPlatHoriz     = first.moveHoriz;
                            mMovPlatRange     = first.moveRange;
                            mMovPlatSpeed     = first.moveSpeed;
                            mMovPlatLoop      = first.moveLoop;
                            mMovPlatTrigger   = first.moveTrigger;
                            break;
                        }
                        mMovPlatPopupOpen  = true;
                        mMovPlatSpeedInput = false;
                        mMovPlatSpeedStr   = std::to_string((int)mMovPlatSpeed);
                        SetStatus(
                            "MovingPlat: click tiles to add. RClick=axis/range. New group "
                            "ID=" +
                            std::to_string(mMovPlatCurGroupId));
                        return true;
                    }
                    case TBBtn::Save: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        SetStatus("Saved: " + path);
                        return true;
                    }
                    case TBBtn::Load: {
                        std::string path = "levels/" + mLevelName + ".json";
                        if (LoadLevel(path, mLevel)) {
                            SetStatus("Loaded: " + path);
                            if (!mLevel.background.empty())
                                background = std::make_unique<Image>(
                                    mLevel.background, FitModeFromString(mLevel.bgFitMode));
                            LoadBgPalette();
                            mCamera.SetPosition(0.0f, 0.0f);
                        } else
                            SetStatus("No file: " + path);
                        return true;
                    }
                    case TBBtn::Clear:
                        mLevel.coins.clear();
                        mLevel.enemies.clear();
                        mLevel.tiles.clear();
                        SetStatus("Cleared");
                        return true;
                    case TBBtn::Play: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        mLaunchGame = true;
                        return true;
                    }
                    case TBBtn::Back: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        mGoBack = true;
                        return true;
                    }
                    case TBBtn::Gravity: {
                        if (mLevel.gravityMode == GravityMode::Platformer)
                            mLevel.gravityMode = GravityMode::WallRun;
                        else if (mLevel.gravityMode == GravityMode::WallRun)
                            mLevel.gravityMode = GravityMode::OpenWorld;
                        else
                            mLevel.gravityMode = GravityMode::Platformer;
                        std::string gLbl =
                            (mLevel.gravityMode == GravityMode::WallRun)     ? "Wall Run"
                            : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                             : "Platform";
                        std::string gStatus =
                            (mLevel.gravityMode == GravityMode::WallRun) ? "Mode: Wall Run"
                            : (mLevel.gravityMode == GravityMode::OpenWorld)
                                ? "Mode: Open World (top-down)"
                                : "Mode: Platformer";
                        mToolbar.SetGravityLabel(gLbl);
                        SetStatus(gStatus);
                        return true;
                    }
                    default:
                        break;
                } // switch
            } // if Button
        } // if toolbar

        // ── Palette panel ──────────────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles) {
                // Resolve which palette entry was clicked (same grid as render)
                constexpr int PAD = 4, LBL_H = 14;
                const int     cellW = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
                const int     cellH = cellW + LBL_H;
                const int     itemH = cellH + PAD;
                int           relX  = mx - CanvasW() - PAD;
                int           relY  = my - TOOLBAR_H - TAB_H - PAD;

                // header strip (44px)
                if (relY < 44)
                    return true;
                relY -= 44;

                int col = relX / (cellW + PAD);
                int row = relY / itemH;
                if (col < 0 || col >= PAL_COLS)
                    return true;

                int idx = (mPalette.TileScroll() + row) * PAL_COLS + col;
                if (idx < 0 || idx >= (int)mPalette.Items().size())
                    return true;

                const auto& item = mPalette.Items()[idx];

                // Delete button hit? open confirm popup instead of deleting immediately
                if (item.delBtn.x >= 0 && HitTest(item.delBtn, mx, my)) {
                    mDelConfirmActive = true;
                    mDelConfirmPath   = item.path;
                    mDelConfirmIsDir  = item.isFolder;
                    mDelConfirmName   = fs::path(item.path).filename().string();
                    return true;
                }

                if (item.isFolder) {
                    // Copy the path BEFORE calling LoadTileView — LoadTileView clears
                    // and rebuilds mPaletteItems, which invalidates the 'item' reference.
                    std::string folderPath = item.path;
                    std::string folderName = fs::path(folderPath).filename().string();
                    LoadTileView(folderPath);
                    SetStatus("Opened: " + folderName);
                } else {
                    // ── Double-click detection ────────────────────────────────
                    bool isDouble = mPalette.CheckDoubleClick(idx);

                    mPalette.SetSelectedTile(idx);
                    mGhostRotation = 0; // reset rotation when a new tile is picked
                    SwitchTool(ToolId::Tile);
                    lblTool->CreateSurface("Tool: Tile");
                    SetStatus("Selected: " + item.label + (isDouble ? " (double)" : ""));
                }
            } else {
                // Backgrounds — single column
                constexpr int PAD = 4, LBL_H = 16;
                const int     thumbW = PALETTE_W - PAD * 2;
                const int     thumbH = thumbW / 2;
                const int     itemH  = thumbH + LBL_H + PAD;

                // Fit-mode cycle button in the header strip (same geometry as Render)
                {
                    int bw = 54, bh = 16;
                    int bx  = CanvasW() + PALETTE_W - bw - 4;
                    int by2 = TOOLBAR_H + TAB_H + (24 - bh) / 2;
                    if (mx >= bx && mx < bx + bw && my >= by2 && my < by2 + bh) {
                        // Cycle: cover -> contain -> stretch -> tile -> scroll -> cover
                        auto& fm = mLevel.bgFitMode;
                        if (fm == "cover")
                            fm = "contain";
                        else if (fm == "contain")
                            fm = "stretch";
                        else if (fm == "stretch")
                            fm = "tile";
                        else if (fm == "tile")
                            fm = "scroll";
                        else
                            fm = "cover";
                        // Rebuild background image with new fit mode
                        if (!mLevel.background.empty())
                            background = std::make_unique<Image>(mLevel.background,
                                                                 FitModeFromString(fm));
                        // Force badge cache rebuild for the new label
                        lblBgHeader.reset();
                        SetStatus("Background fit: " + fm);
                        return true;
                    }
                }

                int relY = my - TOOLBAR_H - TAB_H - 24 - PAD; // 24 = bg header strip
                int row  = relY / itemH;
                int idx  = mPalette.BgScroll() + row;
                if (idx >= 0 && idx < (int)mPalette.BgItems().size()) {
                    // Delete button?
                    if (mPalette.BgItems()[idx].delBtn.x >= 0 &&
                        HitTest(mPalette.BgItems()[idx].delBtn, mx, my)) {
                        mDelConfirmActive = true;
                        mDelConfirmPath   = mPalette.BgItems()[idx].path;
                        mDelConfirmIsDir  = false;
                        mDelConfirmName =
                            fs::path(mPalette.BgItems()[idx].path).filename().string();
                    } else {
                        ApplyBackground(idx);
                    }
                }
            }
            return true;
        }

        // ── Canvas ─────────────────────────────────────────────────────────────
        // Moving-platform config popup click handling
        if (mMovPlatPopupOpen && HitTest(mMovPlatPopupRect, mx, my)) {
            const int PW      = 280;
            const int PAD     = 8;
            const int ROW_H   = 26;
            const int TITLE_H = 30;
            const int px      = mMovPlatPopupRect.x;
            const int py      = mMovPlatPopupRect.y;
            int       ry      = py + TITLE_H;

            // Row 0: speed field + close button
            SDL_Rect speedField = {
                px + PAD + 90, ry + (ROW_H - 20) / 2, PW - PAD * 2 - 90 - 44, 20};
            SDL_Rect closeBtnR = {px + PW - PAD - 36, ry + (ROW_H - 20) / 2, 36, 20};
            if (HitTest(speedField, mx, my)) {
                mMovPlatSpeedInput = true;
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
            if (HitTest(closeBtnR, mx, my)) {
                if (!mMovPlatSpeedStr.empty()) {
                    int v         = std::clamp(std::stoi(mMovPlatSpeedStr), 10, 2000);
                    mMovPlatSpeed = (float)v;
                    // Apply to current session tiles
                    for (int idx : mMovPlatIndices)
                        mLevel.tiles[idx].moveSpeed = mMovPlatSpeed;
                    // Also apply to ALL moving tiles in the current group so editing
                    // an already-placed platform actually takes effect.
                    for (auto& t : mLevel.tiles) {
                        if (!t.moving)
                            continue;
                        bool inGroup =
                            (mMovPlatCurGroupId != 0 &&
                             t.moveGroupId == mMovPlatCurGroupId) ||
                            std::any_of(mMovPlatIndices.begin(),
                                        mMovPlatIndices.end(),
                                        [&](int i) { return &t == &mLevel.tiles[i]; });
                        if (inGroup)
                            t.moveSpeed = mMovPlatSpeed;
                    }
                }
                mMovPlatPopupOpen  = false;
                mMovPlatSpeedInput = false;
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
            ry += ROW_H + PAD;

            // Row 1: H/V direction toggle
            SDL_Rect btnH = {px + PAD + 90, ry, 48, ROW_H - 4};
            SDL_Rect btnV = {px + PAD + 90 + 54, ry, 48, ROW_H - 4};
            if (HitTest(btnH, mx, my)) {
                mMovPlatHoriz = true;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveHoriz = true;
                return true;
            }
            if (HitTest(btnV, mx, my)) {
                mMovPlatHoriz = false;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveHoriz = false;
                return true;
            }
            ry += ROW_H + PAD;

            // Row 2: Loop (ping-pong) checkbox row
            SDL_Rect loopRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
            if (HitTest(loopRow, mx, my)) {
                mMovPlatLoop = !mMovPlatLoop;
                if (!mMovPlatLoop)
                    mMovPlatTrigger = false;
                for (int idx : mMovPlatIndices) {
                    mLevel.tiles[idx].moveLoop    = mMovPlatLoop;
                    mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
                }
                return true;
            }
            ry += ROW_H + PAD;

            // Row 3: Move on Touch checkbox row
            SDL_Rect trigRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
            if (HitTest(trigRow, mx, my)) {
                mMovPlatTrigger = !mMovPlatTrigger;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
                return true;
            }

            return true; // absorb all other clicks inside popup
        }

        if (my < TOOLBAR_H || mx >= CanvasW())
            return true;
        auto [sx, sy] = SnapToGrid(mx, my);

        // Dispatch to extracted tools first
        if (mTool) {
            // Populate TileTool's placement info if active
            if (mActiveToolId == ToolId::Tile) {
                if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
                    const auto* selItem = mPalette.SelectedItem();
                    tt->placementInfo   = selItem ? TilePlacementInfo{true,
                                                                      selItem->isFolder,
                                                                      selItem->path,
                                                                      selItem->label}
                                                  : TilePlacementInfo{};
                }
            }
            auto ctx = MakeToolCtx();
            auto res = mTool->OnMouseDown(ctx, mx, my, SDL_BUTTON_LEFT, SDL_GetModState());
            if (res == ToolResult::Consumed)
                return true;
        }

        switch (mActiveToolId) {
            // Coin, Enemy, Tile, Erase, PlayerStart, MoveCam handled by mTool above
            default:
                break;
            case ToolId::Action: {
                // If the picker is open and the click landed outside it, close it
                // and fall through so the click can act on whatever tile is below.
                if (mActionAnimPickerTile >= 0) {
                    if (HitTest(mActionAnimPickerRect, mx, my))
                        return true; // handled by the picker block above
                    CloseAnimPicker();
                    // fall through — open picker for the newly clicked tile if applicable
                }
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    if (mLevel.tiles[ti].action) {
                        // Tile is already an action tile — open the anim picker
                        OpenAnimPicker(ti);
                        SetStatus("Tile " + std::to_string(ti) + ": choose death animation");
                    } else {
                        // Not an action tile yet — make it one
                        mLevel.tiles[ti].action = true;
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                        SetStatus("Tile " + std::to_string(ti) +
                                  " → action  (click again to assign death anim)");
                    }
                } else {
                    CloseAnimPicker();
                }
                return true;
            }
            case ToolId::PowerUp: {
                // Handle PowerUp popup click: close popup if open and click is in it
                if (mPowerUpPopupOpen && mPowerUpTileIdx >= 0 &&
                    HitTest(mPowerUpPopupRect, mx, my)) {
                    // Re-derive cell geometry matching Render
                    const auto& reg = GetPowerUpRegistry();
                    const int   PAD = 8, ROW_H = 28, TITLE_H = 32;
                    int         py = mPowerUpPopupRect.y + TITLE_H;
                    for (int i = 0; i < (int)reg.size(); i++) {
                        SDL_Rect row = {mPowerUpPopupRect.x + PAD,
                                        py + i * (ROW_H + 2),
                                        mPowerUpPopupRect.w - PAD * 2,
                                        ROW_H};
                        if (HitTest(row, mx, my)) {
                            // Assign this power-up to the tile
                            auto& t           = mLevel.tiles[mPowerUpTileIdx];
                            t.powerUp         = true;
                            t.powerUpType     = reg[i].id;
                            t.powerUpDuration = reg[i].defaultDuration;
                            SetStatus("Tile " + std::to_string(mPowerUpTileIdx) +
                                      " -> PowerUp: " + reg[i].label);
                            mPowerUpPopupOpen = false;
                            mPowerUpTileIdx   = -1;
                            return true;
                        }
                    }
                    // 'None' row (clear)
                    SDL_Rect noneRow = {mPowerUpPopupRect.x + PAD,
                                        py + (int)reg.size() * (ROW_H + 2),
                                        mPowerUpPopupRect.w - PAD * 2,
                                        ROW_H};
                    if (HitTest(noneRow, mx, my)) {
                        mLevel.tiles[mPowerUpTileIdx].powerUp     = false;
                        mLevel.tiles[mPowerUpTileIdx].powerUpType = "";
                        SetStatus("Tile " + std::to_string(mPowerUpTileIdx) +
                                  " -> PowerUp removed");
                        mPowerUpPopupOpen = false;
                        mPowerUpTileIdx   = -1;
                        return true;
                    }
                    return true; // absorb
                }
                // Click outside popup or no popup open: open/reopen on tile
                mPowerUpPopupOpen = false;
                mPowerUpTileIdx   = -1;
                int ti            = HitTile(mx, my);
                if (ti >= 0) {
                    mPowerUpTileIdx   = ti;
                    mPowerUpPopupOpen = true;
                    // Position popup near the tile
                    auto [wsx, wsy] = WorldToScreen(mLevel.tiles[ti].x, mLevel.tiles[ti].y);
                    const auto& reg = GetPowerUpRegistry();
                    int         ph  = 32 + (int)(reg.size() + 1) * 30 + 8;
                    int         pw  = 200;
                    int px2 = std::clamp(wsx, 0, (mWindow ? mWindow->GetWidth() : 800) - pw);
                    int py2 = std::clamp(wsy + mLevel.tiles[ti].h,
                                         TOOLBAR_H,
                                         (mWindow ? mWindow->GetHeight() : 600) - ph);
                    mPowerUpPopupRect = {px2, py2, pw, ph};
                    SetStatus("Tile " + std::to_string(ti) + ": choose power-up type");
                } else {
                    SetStatus("PowerUp: click a tile to assign a power-up pickup");
                }
                return true;
            }
            case ToolId::MovingPlat: {
                // Left-click: toggle tile into/out of the current moving group
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    auto& t = mLevel.tiles[ti];
                    // If the tile is already moving but from a *different* session group,
                    // adopt its group so the speed popup edits the right group.
                    if (t.moving &&
                        std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti) ==
                            mMovPlatIndices.end()) {
                        // Clicking an existing platform from a previous session: adopt its
                        // group
                        mMovPlatCurGroupId =
                            (t.moveGroupId != 0) ? t.moveGroupId : mMovPlatNextGroupId++;
                        mMovPlatHoriz    = t.moveHoriz;
                        mMovPlatRange    = t.moveRange;
                        mMovPlatSpeed    = t.moveSpeed;
                        mMovPlatLoop     = t.moveLoop;
                        mMovPlatTrigger  = t.moveTrigger;
                        mMovPlatSpeedStr = std::to_string((int)mMovPlatSpeed);
                        // Collect all tiles in that group into mMovPlatIndices
                        mMovPlatIndices.clear();
                        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                            if (!mLevel.tiles[i].moving)
                                continue;
                            bool inGrp = (t.moveGroupId != 0)
                                             ? (mLevel.tiles[i].moveGroupId == t.moveGroupId)
                                             : (i == ti);
                            if (inGrp)
                                mMovPlatIndices.push_back(i);
                        }
                        SetStatus("Adopted platform group " +
                                  std::to_string(mMovPlatCurGroupId) +
                                  "  spd=" + std::to_string((int)mMovPlatSpeed) +
                                  "  tiles=" + std::to_string(mMovPlatIndices.size()));
                        return true;
                    }
                    // If already in this group, remove it
                    auto it = std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti);
                    if (it != mMovPlatIndices.end()) {
                        mMovPlatIndices.erase(it);
                        t.moving      = false;
                        t.moveGroupId = 0;
                        SetStatus("Tile " + std::to_string(ti) +
                                  " removed from platform group " +
                                  std::to_string(mMovPlatCurGroupId));
                    } else {
                        // Add to current group
                        mMovPlatIndices.push_back(ti);
                        t.moving      = true;
                        t.moveHoriz   = mMovPlatHoriz;
                        t.moveRange   = mMovPlatRange;
                        t.moveSpeed   = mMovPlatSpeed;
                        t.moveLoop    = mMovPlatLoop;
                        t.moveTrigger = mMovPlatTrigger;
                        t.movePhase   = 0.0f; // set per-tile via Ctrl+scroll in editor
                        t.moveLoopDir = 1;    // set per-tile via Shift+scroll in editor
                        t.moveGroupId =
                            (mMovPlatIndices.size() > 1) ? mMovPlatCurGroupId : 0;
                        // Re-apply group id to all tiles in group
                        if (mMovPlatIndices.size() > 1) {
                            for (int idx : mMovPlatIndices)
                                mLevel.tiles[idx].moveGroupId = mMovPlatCurGroupId;
                        }
                        SetStatus("Tile " + std::to_string(ti) +
                                  " added to platform group " +
                                  std::to_string(mMovPlatCurGroupId) + "  " +
                                  (mMovPlatHoriz ? "H" : "V") +
                                  "  range=" + std::to_string((int)mMovPlatRange) +
                                  "  spd=" + std::to_string((int)mMovPlatSpeed));
                    }
                }
                return true;
            }
        }

        // Only start drag for inline tools that don't have their own entity-drag.
        // Extracted tools (Prop, Ladder, Slope, Hazard, AntiGrav, Select, Resize,
        // Hitbox) handle drag via mTool->OnMouseDown()/OnMouseMove()/OnMouseUp().
        if (mActiveToolId != ToolId::Erase && mActiveToolId != ToolId::Coin &&
            mActiveToolId != ToolId::Enemy && mActiveToolId != ToolId::Tile &&
            mActiveToolId != ToolId::MoveCam && mActiveToolId != ToolId::PlayerStart &&
            mActiveToolId != ToolId::Prop && mActiveToolId != ToolId::Ladder &&
            mActiveToolId != ToolId::Slope && mActiveToolId != ToolId::Hazard &&
            mActiveToolId != ToolId::AntiGrav && mActiveToolId != ToolId::Select &&
            mActiveToolId != ToolId::Resize && mActiveToolId != ToolId::Hitbox) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                mIsDragging = true;
                mDragIndex  = ti;
                mDragIsTile = true;
                mDragIsCoin = false;
                return true;
            }
            int ci = HitCoin(mx, my);
            if (ci >= 0) {
                mIsDragging = true;
                mDragIndex  = ci;
                mDragIsCoin = true;
                mDragIsTile = false;
                return true;
            }
            int ei = HitEnemy(mx, my);
            if (ei >= 0) {
                mIsDragging = true;
                mDragIndex  = ei;
                mDragIsCoin = false;
                mDragIsTile = false;
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        // Dispatch to active tool first (handles entity drag stop for modifier tools)
        if (mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseUp(
                ctx, (int)e.button.x, (int)e.button.y, e.button.button, SDL_GetModState());
        }
        mIsDragging = false;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;

        // ── Pan: derive delta from absolute position vs. recorded start ─────────
        // SDL coalesces MOUSE_MOTION on macOS so xrel/yrel can skip frames.
        // Using absolute position minus the recorded start position means we
        // always land exactly where the mouse is right now, regardless of how
        // many motion events were coalesced into this one.
        if (mCamera.IsPanning()) {
            mCamera.UpdatePan(mx, my);
            return true;
        }

        // Track which action tile is under the cursor during an active drag-drop
        // so Render can show the "drop here" highlight.
        if (mDropActive && mActiveToolId == ToolId::Action) {
            int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            mActionAnimDropHover = (ti >= 0 && mLevel.tiles[ti].action) ? ti : -1;
        } else {
            mActionAnimDropHover = -1;
        }

        // Delegate motion to active tool (select, resize, hitbox, entity drag)
        if (mTool) {
            auto ctx = MakeToolCtx();
            if (mTool->OnMouseMove(ctx, mx, my) == ToolResult::Consumed)
                return true;
        }

        // ── Entity drag ───────────────────────────────────────────────────────
        if (mIsDragging && mDragIndex >= 0 && my >= TOOLBAR_H && mx < CanvasW()) {
            auto [sx, sy] = SnapToGrid(mx, my);
            if (mDragIsTile && mDragIndex < (int)mLevel.tiles.size()) {
                mLevel.tiles[mDragIndex].x = (float)sx;
                mLevel.tiles[mDragIndex].y = (float)sy;
            } else if (mDragIsCoin && mDragIndex < (int)mLevel.coins.size()) {
                mLevel.coins[mDragIndex].x = (float)sx;
                mLevel.coins[mDragIndex].y = (float)sy;
            } else if (!mDragIsCoin && !mDragIsTile &&
                       mDragIndex < (int)mLevel.enemies.size()) {
                mLevel.enemies[mDragIndex].x = (float)sx;
                mLevel.enemies[mDragIndex].y = (float)sy;
            }
        }
    }

    return true;
}

// --- Update ----------------------------------------------------------------
void LevelEditorScene::Update(float /*dt*/) {
    // Pan is driven entirely by SDL_EVENT_MOUSE_MOTION in HandleEvent using
    // absolute position delta from the recorded start point. SDL_CaptureMouse
    // ensures motion events are delivered reliably, so no polling catch-up is
    // needed here. Having two writers to mCamX/Y caused jitter, especially
    // under thermal throttling where frame timing is inconsistent.
}

// --- Render ----------------------------------------------------------------
void LevelEditorScene::Render(Window& window) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();

    int          W = mWindow->GetWidth(), H = mWindow->GetHeight();
    SDL_Surface* screen = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (!screen) {
        window.Update();
        return;
    }
    SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(
        screen,
        nullptr,
        SDL_MapRGBA(SDL_GetPixelFormatDetails(screen->format), nullptr, 0, 0, 0, 0));
    int cw = CanvasW();

    // ── Canvas pass ─────────────────────────────────────────────────────────
    EditorCanvasRenderer::MovPlatState mp;
    mp.indices    = &mMovPlatIndices;
    mp.curGroupId = mMovPlatCurGroupId;
    mp.horiz      = mMovPlatHoriz;
    mp.range      = mMovPlatRange;
    mp.speed      = mMovPlatSpeed;
    mp.loop       = mMovPlatLoop;
    mp.trigger    = mMovPlatTrigger;
    mp.popupOpen  = mMovPlatPopupOpen;
    mp.speedInput = mMovPlatSpeedInput;
    mp.speedStr   = mMovPlatSpeedStr;
    mp.popupRect  = mMovPlatPopupRect;

    mCanvasRenderer.Render(window,
                           screen,
                           cw,
                           TOOLBAR_H,
                           GRID,
                           mLevel,
                           mCamera,
                           mSurfaceCache,
                           mPalette,
                           background.get(),
                           coinSheet.get(),
                           enemySheet.get(),
                           mActiveToolId,
                           mTool.get(),
                           MakeToolCtx(),
                           mActionAnimDropHover,
                           mp);

    // Sync back the popup rect (canvas renderer computes it when popup is open)
    mMovPlatPopupRect = mCanvasRenderer.MovPlatPopupRect();

    // ── UI pass ─────────────────────────────────────────────────────────────
    // Bridge AnimPickerEntry types (LevelEditorScene::AnimPickerEntry ->
    // EditorUIRenderer::AnimPickerEntry — same fields, different type).
    std::vector<EditorUIRenderer::AnimPickerEntry> uiPickerEntries;
    uiPickerEntries.reserve(mAnimPickerEntries.size());
    for (const auto& e : mAnimPickerEntries)
        uiPickerEntries.push_back({e.path, e.name, e.thumb});

    // Bridge PowerUpEntry types
    const auto&                                 reg = GetPowerUpRegistry();
    std::vector<EditorUIRenderer::PowerUpEntry> uiPowerUpReg;
    uiPowerUpReg.reserve(reg.size());
    for (const auto& r : reg)
        uiPowerUpReg.push_back({r.id, r.label, r.defaultDuration});

    EditorUIRenderer::PowerUpPopupState puState;
    puState.open     = mPowerUpPopupOpen;
    puState.tileIdx  = mPowerUpTileIdx;
    puState.rect     = mPowerUpPopupRect;
    puState.registry = &uiPowerUpReg;

    EditorUIRenderer::DelConfirmState dcState;
    dcState.active  = mDelConfirmActive;
    dcState.isDir   = mDelConfirmIsDir;
    dcState.name    = mDelConfirmName;
    dcState.yesRect = mDelConfirmYes;
    dcState.noRect  = mDelConfirmNo;

    EditorUIRenderer::ImportInputState impState;
    impState.active = mImportInputActive;
    impState.text   = mImportInputText;

    EditorUIRenderer::MovPlatPopupState mpuState;
    mpuState.open       = mMovPlatPopupOpen;
    mpuState.speedInput = mMovPlatSpeedInput;
    mpuState.speedStr   = mMovPlatSpeedStr;
    mpuState.speed      = mMovPlatSpeed;
    mpuState.horiz      = mMovPlatHoriz;
    mpuState.loop       = mMovPlatLoop;
    mpuState.trigger    = mMovPlatTrigger;
    mpuState.curGroupId = mMovPlatCurGroupId;
    mpuState.rect       = mMovPlatPopupRect;

    mUIRenderer.Render(window,
                       screen,
                       cw,
                       TOOLBAR_H,
                       GRID,
                       mActiveToolId,
                       mCamera,
                       mLevel,
                       mSurfaceCache,
                       mToolbar,
                       mPalette,
                       lblStatus.get(),
                       lblTool.get(),
                       lblToolPrefix.get(),
                       mActionAnimPickerTile,
                       uiPickerEntries,
                       mActionAnimPickerRect,
                       puState,
                       dcState,
                       impState,
                       mpuState,
                       mDropActive,
                       lblPalHeader,
                       lblPalHint1,
                       lblPalHint2,
                       lblBgHeader,
                       lblStatusBar,
                       lblCamPos,
                       lblBottomHint,
                       mLastTileCount,
                       mLastCoinCount,
                       mLastEnemyCount,
                       mLastCamX,
                       mLastCamY,
                       mLastTileSizeW,
                       mLastPalHeaderPath,
                       GetTileW());

    // Sync back rects that HandleEvent needs
    mDelConfirmYes        = mUIRenderer.DelConfirmYesRect();
    mDelConfirmNo         = mUIRenderer.DelConfirmNoRect();
    mActionAnimPickerRect = mUIRenderer.AnimPickerRect();

    // Upload completed surface to GPU and present
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, screen);
    SDL_DestroySurface(screen);
    if (tex) {
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// --- NextScene -------------------------------------------------------------
std::unique_ptr<Scene> LevelEditorScene::NextScene() {
    if (mLaunchGame) {
        mLaunchGame = false;
        return std::make_unique<GameScene>(
            "levels/" + mLevelName + ".json", true, mProfilePath);
    }
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}
// --- ImportPath ------------------------------------------------------------
bool LevelEditorScene::ImportPath(const std::string& srcPath) {
    fs::path src(srcPath);

    if (fs::is_directory(src)) {
        if (mPalette.ActiveTab() == EditorPalette::Tab::Backgrounds) {
            int count = 0;
            for (const auto& entry : fs::recursive_directory_iterator(src)) {
                if (entry.path().extension() == ".png")
                    count += ImportPath(entry.path().string()) ? 1 : 0;
            }
            SetStatus("Imported " + std::to_string(count) + " backgrounds from " +
                      src.filename().string());
            return count > 0;
        }

        fs::path        baseDestDir = fs::path(mPalette.CurrentDir()) / src.filename();
        std::error_code ec;
        int             count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(src)) {
            if (entry.is_directory()) {
                fs::path rel  = fs::relative(entry.path(), src);
                fs::path dest = baseDestDir / rel;
                fs::create_directories(dest, ec);
                continue;
            }
            if (entry.path().extension() != ".png")
                continue;
            fs::path rel  = fs::relative(entry.path(), src);
            fs::path dest = baseDestDir / rel;
            fs::create_directories(dest.parent_path(), ec);
            if (!fs::exists(dest)) {
                fs::copy_file(entry.path(), dest, ec);
                if (ec)
                    continue;
            }
            count++;
        }
        if (count == 0) {
            SetStatus("No PNGs found in " + src.filename().string());
            return false;
        }
        LoadTileView(baseDestDir.string());
        SetStatus("Imported \"" + src.filename().string() + "\" into " +
                  fs::path(mPalette.CurrentDir()).filename().string() + " (" +
                  std::to_string(count) + " files)");
        return true;
    }

    std::string ext = src.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") {
        SetStatus("Import failed: only .png supported (got " + ext + ")");
        return false;
    }

    bool        isBg       = (mPalette.ActiveTab() == EditorPalette::Tab::Backgrounds);
    std::string destDirStr = isBg ? BG_ROOT : TILE_ROOT;
    if (!isBg && mPalette.CurrentDir() != TILE_ROOT)
        destDirStr = mPalette.CurrentDir();

    fs::path        destDir(destDirStr);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        SetStatus("Import failed: can't create " + destDirStr);
        return false;
    }

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, ec);
        if (ec) {
            SetStatus("Import failed: " + ec.message());
            return false;
        }
    }

    if (isBg) {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) {
            SetStatus("Import failed: can't load " + dest.string());
            return false;
        }
        const int    tw = PALETTE_W - 8, th = tw / 2;
        SDL_Surface* thumb = MakeThumb(full, tw, th);
        SDL_DestroySurface(full);
        mPalette.BgItems().push_back({dest.string(), dest.stem().string(), thumb});
        mPalette.SetBgScroll(std::max(0, (int)mPalette.BgItems().size() - 1));
        ApplyBackground((int)mPalette.BgItems().size() - 1);
        SetStatus("Imported & applied: " + dest.filename().string());
    } else {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) {
            SetStatus("Import failed: can't load " + dest.string());
            return false;
        }
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        SDL_Surface* thumb = MakeThumb(full, PAL_ICON, PAL_ICON);
        LoadTileView(mPalette.CurrentDir());
        auto& items = mPalette.Items();
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i].path == dest.string()) {
                mPalette.SetSelectedTile(i);
                int row = i / PAL_COLS;
                mPalette.SetTileScroll(std::max(0, row));
                break;
            }
        }
        if (thumb)
            SDL_DestroySurface(thumb);
        SDL_DestroySurface(full);
        SwitchTool(ToolId::Tile);
        if (lblTool)
            lblTool->CreateSurface("Tile");
        SetStatus("Imported: " + dest.filename().string() + " -> auto-selected");
    }
    return true;
}

// --- Tile tool accessors ---------------------------------------------------
int LevelEditorScene::GetTileW() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->tileW;
    return mTileW;
}
int LevelEditorScene::GetTileH() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->tileH;
    return mTileH;
}
int LevelEditorScene::GetGhostRotation() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->ghostRotation;
    return mGhostRotation;
}
