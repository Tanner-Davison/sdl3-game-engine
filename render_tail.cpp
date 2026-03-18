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

    int          W      = mWindow->GetWidth(), H = mWindow->GetHeight();
    SDL_Surface* screen = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (!screen) {
        window.Update();
        return;
    }
    SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(
        screen, nullptr,
        SDL_MapRGBA(SDL_GetPixelFormatDetails(screen->format), nullptr, 0, 0, 0, 0));
    int cw = CanvasW();

    // -- Canvas pass ----------------------------------------------------------
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

    auto toolCtx = MakeToolCtx();
    mCanvasRenderer.Render(
        window, screen, cw, TOOLBAR_H, GRID,
        mLevel, mCamera, mSurfaceCache, mPalette,
        background.get(), coinSheet.get(), enemySheet.get(),
        mActiveToolId, mTool.get(), toolCtx,
        mActionAnimDropHover, mp);

    // Sync back the popup rect (canvas renderer computes it when popup is open)
    mMovPlatPopupRect = mCanvasRenderer.MovPlatPopupRect();

    // -- UI pass --------------------------------------------------------------
    // Bridge AnimPickerEntry (LevelEditorScene -> EditorUIRenderer)
    std::vector<EditorUIRenderer::AnimPickerEntry> uiPickerEntries;
    uiPickerEntries.reserve(mAnimPickerEntries.size());
    for (const auto& e : mAnimPickerEntries)
        uiPickerEntries.push_back({e.path, e.name, e.thumb});

    // Bridge PowerUpEntry
    const auto& reg = GetPowerUpRegistry();
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

    mUIRenderer.Render(
        window, screen, cw, TOOLBAR_H, GRID,
        mActiveToolId, mCamera, mLevel,
        mSurfaceCache, mToolbar, mPalette,
        lblStatus.get(), lblTool.get(), lblToolPrefix.get(),
        mActionAnimPickerTile, uiPickerEntries, mActionAnimPickerRect,
        puState, dcState, impState, mpuState,
        mDropActive,
        lblPalHeader, lblPalHint1, lblPalHint2, lblBgHeader,
        lblStatusBar, lblCamPos, lblBottomHint,
        mLastTileCount, mLastCoinCount, mLastEnemyCount,
        mLastCamX, mLastCamY, mLastTileSizeW,
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
        int count = 0;
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

// --- Tile tool accessors (delegate to TileTool when active) ------------------
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
