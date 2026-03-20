#pragma once
// EditorFileOps.hpp
//
// Handles all file I/O for the level editor:
//   - ImportPath()     copy a PNG or folder into game_assets, refresh palette
//
// EditorFileOps is a pure-static helper — no member state. It operates on
// data passed in via the Ctx struct, which holds non-owning references to the
// LevelEditorScene state it needs. This keeps the class simple and avoids any
// ownership or lifetime complexity.
//
// SDL drop-event handling (SDL_EVENT_DROP_FILE etc.) remains in
// LevelEditorScene::HandleEvent() because it is tightly coupled to the active
// tool (Action tool needs HitTile, GetDestroyAnimThumb) and to mDropActive /
// mActionAnimDropHover state that is used by Render. The drop handler calls
// ImportPath() for the actual file copying, so the I/O logic is still centralised.

#include "EditorPalette.hpp"
#include "EditorSurfaceCache.hpp"
#include "LevelData.hpp"
#include <functional>
#include <string>

class EditorFileOps {
  public:
    // ── Context: non-owning references to editor state ────────────────────────
    struct Ctx {
        EditorPalette&      palette;
        EditorSurfaceCache& cache;
        Level&              level;

        // Callbacks into the orchestrator for side effects that EditorFileOps
        // cannot perform directly (they need LevelEditorScene members).
        std::function<void(const std::string&)> setStatus;
        std::function<void()>                   refreshTileView;   // reload current dir
        std::function<void()>                   refreshBgPalette;
        std::function<void(int)>                applyBackground;   // ApplyBackground(idx)
        std::function<void()>                   switchToTileTool;

        // Layout constants (forwarded from LevelEditorScene statics)
        int         palIcon  = 40;   // PAL_ICON
        int         palCols  = 5;    // PAL_COLS
        int         palW     = 220;  // PALETTE_W
        const char* tileRoot = "game_assets/tiles";
        const char* bgRoot   = "game_assets/backgrounds";
    };

    // Import a file or folder into game_assets.
    //   - Folder: recursively copies all .png files into a matching subfolder under
    //             the current palette directory (or bg root if bg tab is active).
    //   - PNG:    copies into the current palette dir (or bg root), creates a
    //             thumbnail, inserts into the palette, and auto-selects the tile.
    //   Returns true on success.
    static bool ImportPath(const std::string& srcPath, Ctx& ctx);
};
