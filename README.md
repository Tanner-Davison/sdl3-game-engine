# Forge2D

Author: Tanner Davison

A 2D platformer engine built from scratch with SDL3, EnTT ECS, and C++23. Forge2D includes a full-featured level editor, a character creator with per-animation sprite and hitbox configuration, a tile animation creator, and a runtime game engine with physics, combat, and scene management. All levels, characters, and tile animations are created and saved through the editor tools — no hardcoded content.

---

## Features

- Full level editor with tile placement, resizing, hitbox editing, and multi-tool workflow
- Character creator with 8 animation slots (Idle, Walk, Crouch, Jump, Fall, Slash, Hurt, Death), per-slot FPS, and per-slot hitbox configuration
- Tile animation creator for building animated tile sequences from PNG folders
- Scene system with title screen, game, editor, pause menu, and creator scenes
- ECS architecture using EnTT with cleanly separated systems
- Platformer, wall-run, and open-world (top-down) gravity modes per level
- Collision system with AABB, slopes, step-up, and custom hitbox offsets
- Ladder climbing system with top-landing, mid-grab, and descent
- Moving platforms with horizontal/vertical travel, ping-pong, sine oscillation, grouped movement, and player-trigger activation
- Floating/anti-gravity objects with bob, drift, spin, and physics-driven player interaction
- Action tiles (slash-to-destroy with hit counts, grouped triggering, and animated destruction sequences)
- Hazard tiles with continuous damage and visual flash
- Power-up system (anti-gravity, extensible for future types)
- Enemy AI with patrol, stomp-to-kill, slash-to-kill, and gravity for grounded enemies
- Coin collection and level completion tracking
- Pause menu with resume, back-to-editor, and back-to-title options
- GPU-accelerated rendering via SDL3 Renderer with texture caching across respawns
- Camera system with deadzone, smooth lerp follow, and level bounds clamping
- Debug hitbox overlay (F1) showing player, tile, hazard, ladder, and enemy colliders
- Custom character profiles saved as JSON with per-slot sprite folders and hitbox data
- Level serialization to JSON with full round-trip fidelity

---

## Architecture

### ECS (Entity Component System)

Components are plain data structs defined in `Components.hpp`. Systems are free functions in individual headers under `include/systems/`, aggregated by `Systems.hpp`.

**Core Components:** Transform, Velocity, Collider, ColliderOffset, Renderable, RenderOffset, AnimationState, AnimationSet, Health, InvincibilityTimer, GravityState, ClimbState, HazardState, AttackState, PlayerBaseCollider, ActivePowerUps, FloatState, MovingPlatformState, HitFlash, DestroyAnimTag, SlopeCollider

**Tag Components:** PlayerTag, EnemyTag, CoinTag, DeadTag, TileTag, LadderTag, PropTag, HazardTag, FloatTag, MovingPlatformTag, TileAnimTag, OpenWorldTag, ActionTag, PowerUpTag, DestructibleTag

**Systems:**

- `InputSystem` — keyboard input for WASD/arrows, space, ctrl, F (slash), W/S on ladders
- `MovementSystem` — velocity integration, friction, gravity force, jump hold boost, enemy patrol with tile-edge reversal
- `PlayerStateSystem` — animation priority state machine (attack > hurt > anti-gravity float > airborne > crouch > walk > idle) with per-character collider enforcement and slot capability gating
- `AnimationSystem` — delta-time frame advancement with looping and non-looping support
- `CollisionSystem` — multi-pass AABB (slope pass, gravity-axis snap, lateral push-out with step-up), enemy stomp/damage, coin collection, hazard overlap, and sword hitbox vs enemy/action-tile detection
- `BoundsSystem` — level-edge clamping, wall-run gravity flip, open-world bounds, punishment timer tick
- `MovingPlatformSystem` — platform tick (sine/ping-pong/trigger modes, grouped sync) and player carry
- `FloatingSystem` — bob oscillation, drift/spin decay, player body push, sword slash push, floating-object-to-object collisions, tile bounce, and enemy gravity for non-floating enemies
- `LadderSystem` — ladder column detection, climb/descend/top-lock states, jump-off, and side walk-off
- `RenderSystem` — GPU texture rendering with camera offset, flip, rotation, invincibility flash, hazard flash, hit flash overlay, and pre-sorted tile render list
- `HUDSystem` — health bar, gravity direction indicator, coin count, stomp count

### Scene System

Scenes inherit from `Scene` (load, unload, handle event, update, render, next scene). `SceneManager` owns the active scene and handles transitions.

- `TitleScene` — title screen with Play and Editor buttons, character profile selector
- `GameScene` — main gameplay, owns the ECS registry and all runtime assets
- `LevelEditorScene` — full level editor (modular: EditorFileOps, EditorPalette, EditorToolbar, EditorPopups, EditorCamera, EditorCanvasRenderer, EditorUIRenderer, EditorSurfaceCache)
- `PlayerCreatorScene` — character creator with drag-and-drop sprite folders, hitbox editor, and save/load
- `TileAnimCreatorScene` — animated tile builder from PNG sequences
- `PauseMenuScene` — in-game pause overlay with resume and back navigation

### Level Editor Tools (`include/tools/`)

The editor uses a tool abstraction (`EditorTool` base) with a shared `EditorToolContext`. Available tools:

- `PlacementTools` — single tile and line/rect fill placement
- `SelectTool` — click or box-select tiles, move selection, delete, copy
- `ResizeTool` — drag tile edges/corners to resize
- `HitboxTool` — drag to define custom collider sub-rects per tile
- `ModifierTools` — toggle tile properties (prop, hazard, ladder, slope, action, moving platform, power-up, anti-gravity, etc.)

### Supporting Classes

- **Window** — RAII SDL3 window wrapper with GPU renderer
- **Image** — loads and renders images with fit modes (Cover, Contain, Stretch, SrcSize, Scroll, ScrollWide) and optional tiling/repeat
- **SpriteSheet** — texture atlas parser (text, XML, or explicit path list) with named animation lookup and GPU texture upload
- **Text** — SDL3_ttf text rendering with centering helpers
- **UI / Button / Rectangle** — interactive UI primitives with hover detection
- **LevelSerializer** — JSON save/load for levels (tiles, enemies, coins, player spawn, background, gravity mode, and all per-tile properties)
- **PlayerProfile** — JSON save/load for character profiles (name, sprite dimensions, per-slot folder paths, hitbox overrides, FPS overrides)
- **AnimatedTile** — JSON manifest loader for tile animation sequences
- **SurfaceUtils** — surface rotation and scaling helpers
- **GameConfig** — compile-time gameplay constants (player stats, physics, enemy stats, camera)
- **GameEvents** — lightweight result structs returned by systems (CollisionResult, FloatingResult)

---

## Building

### Prerequisites

Dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg): SDL3, SDL3_image (with PNG), SDL3_ttf, EnTT, nlohmann_json, libpng, and zlib.

**Install vcpkg** (if not already installed):
```bash
git clone https://github.com/microsoft/vcpkg ~/tools/vcpkg
cd ~/tools/vcpkg
./bootstrap-vcpkg.sh
```

**Install dependencies:**

Linux:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json
```

macOS Apple Silicon:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json --triplet arm64-osx
```

macOS Intel:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json --triplet x64-osx
```

### Building with Presets

```bash
# Linux
cmake --preset linux && cmake --build --preset linux

# macOS Apple Silicon
cmake --preset mac-arm && cmake --build --preset mac-arm

# macOS Intel
cmake --preset mac-intel && cmake --build --preset mac-intel

# Run (always from the project root so asset paths resolve correctly)
./build/forge2d
```

### Manual CMake (no presets)

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=~/tools/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build
```

---

## Project Structure

```
forge2d/
├── src/                         # Implementation files (.cpp)
│   ├── main.cpp                 # Entry point, scene loop
│   ├── GameScene.cpp            # Main gameplay scene
│   ├── LevelEditorScene.cpp     # Level editor core
│   ├── Editor*.cpp              # Editor subsystem implementations
│   ├── PlayerCreatorScene.cpp   # Character creator
│   ├── TileAnimCreatorScene.cpp # Tile animation creator
│   ├── TitleScene.cpp           # Title screen
│   └── *.cpp                    # Window, Image, Text, SpriteSheet, UI, etc.
├── include/
│   ├── systems/                 # Individual ECS system headers
│   ├── tools/                   # Level editor tool classes
│   ├── engine/                  # Scene, SceneManager base classes
│   ├── game/                    # GameConfig, GameEvents
│   ├── Components.hpp           # All ECS component definitions
│   ├── Systems.hpp              # Aggregator for all systems
│   ├── GameScene.hpp            # Game scene header
│   ├── LevelEditorScene.hpp     # Editor scene header
│   ├── Level.hpp                # Level data structures
│   ├── LevelSerializer.hpp      # Level JSON I/O
│   ├── PlayerProfile.hpp        # Character profile data and JSON I/O
│   ├── AnimatedTile.hpp         # Animated tile manifest loader
│   ├── PauseMenuScene.hpp       # Pause overlay
│   └── *.hpp                    # Window, Image, Text, SpriteSheet, UI, etc.
├── game_assets/                 # Sprites, backgrounds, tilesets, character frames
├── levels/                      # Saved level JSON files
├── players/                     # Saved character profile JSON files
├── fonts/                       # TTF font files
├── CMakeLists.txt               # Build configuration (C++23, vcpkg)
└── CMakePresets.json            # Platform build presets (Linux, macOS ARM/Intel)
```

---

## Controls

### Gameplay

| Key | Action |
|-----|--------|
| A / D (or arrows) | Move left / right |
| W / S on ladder | Climb up / down |
| Space | Jump |
| Left Ctrl | Crouch |
| F | Slash (sword attack) |
| ESC | Pause menu |
| F1 | Toggle debug hitbox overlay |
| F11 | Toggle fullscreen |
| R | Retry after game over |

### Level Editor

| Key | Action |
|-----|--------|
| Arrow keys / Middle-click drag | Pan camera |
| Scroll wheel | Zoom in / out |
| 1-9 | Switch tools |
| G | Toggle gravity mode |
| Delete | Delete selected tiles |
| Ctrl+S | Save level |
| Ctrl+Z | Undo |
| Play button | Test level in-game |

---

## Workflow

1. Launch the game — the title screen offers Play (start a level) or Editor (open the level editor)
2. In the editor, place tiles from the palette, set tile properties (solid, prop, hazard, ladder, slope, action, moving, power-up), place enemies and coins, set the player spawn point, and configure the background and gravity mode
3. Save levels to the `levels/` directory as JSON files
4. Create custom characters in the Player Creator — assign sprite folders per animation slot, configure hitboxes and FPS, and save to `players/`
5. Create animated tiles in the Tile Animation Creator — select a folder of PNG frames, preview the animation, and save a manifest JSON
6. Press Play in the editor to test the level with your selected character, then ESC to return to the editor

---

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
