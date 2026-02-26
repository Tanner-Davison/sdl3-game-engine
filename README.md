# SDL Sandbox

Author: Tanner Davison

A 2D game engine foundation built on SDL3 and EnTT. This project is a personal sandbox for building and testing the core systems needed for a platformer engine — windowing, sprite animation, sprite sheets, an Entity Component System (ECS), collision detection, scene management, and a delta-time game loop.

---

## Architecture Overview

### ECS (Entity Component System)
The core game simulation runs through EnTT. Components are plain data structs defined in `Components.hpp`, and systems are free functions split into individual headers under `include/systems/`, aggregated by `Systems.hpp`.

**Components:**
- `Transform` — world position (x, y)
- `Velocity` — movement direction, speed, and friction
- `AnimationState` — current frame, timer, fps, looping flag, and current animation ID
- `AnimationSet` — full set of frame rect vectors per animation (idle, walk, jump, hurt, duck, front)
- `Renderable` — sprite sheet surface, active frame rects, and horizontal flip state
- `FlipCache` — lazily-built per-frame cache of pre-flipped surfaces, invalidated on animation change
- `Health` — current and max health values
- `Collider` — AABB width and height, updated when crouching
- `InvincibilityTimer` — post-hit grace period with remaining time and active flag
- `GravityState` — gravity direction, velocity along gravity axis, grounded state, crouch state, and punishment timer
- `PlayerTag` — empty tag marking the player entity
- `EnemyTag` — empty tag marking enemy entities

**Systems (`include/systems/`):**
- `InputSystem` — WASD/space/ctrl input, handles gravity and free mode separately
- `MovementSystem` — velocity, friction, gravity force, jump boost, crouch deceleration
- `PlayerStateSystem` — animation priority state machine (hurt > crouch > jump > walk > idle), manages collider and position on crouch transitions
- `AnimationSystem` — advances animation frames using delta time, respects looping flag
- `RenderSystem` — extracts frames from original sheet, applies flip cache, gravity rotation, wall-flush position offset, and invincibility flash. Includes debug hitbox rendering
- `CollisionSystem` — AABB player vs enemy detection, applies damage, triggers zero gravity punishment
- `BoundsSystem` — clamps player to screen edges, activates gravity on wall contact, ticks punishment timer
- `CenterPullSystem` — pulls player toward screen center during zero gravity punishment
- `HUDSystem` — renders health bar and zero gravity countdown timer

### Scene System
Game states are organized as scenes that inherit from a `Scene` base class. `SceneManager` owns the active scene and handles transitions automatically when `NextScene()` returns a non-null value.

- `TitleScene` — opening screen with Play button, transitions to `GameScene`
- `GameScene` — main gameplay, owns the ECS registry and all assets

To add a new level, create a class that inherits from `Scene` and return it from `GameScene::NextScene()` when the win condition is met.

### Supporting Classes
- **Window** — RAII wrapper around `SDL_Window`. Uses surface-based rendering. Note: `SDL_GetWindowSurface` and `SDL_CreateRenderer` are mutually exclusive — do not create a renderer on this window.
- **Image** — Loads and blits a single image surface with configurable `FitMode`:
  - `CONTAIN` — letterboxed to fit within bounds
  - `COVER` — cropped to fill bounds, preserving aspect ratio
  - `STRETCH` — fills bounds ignoring aspect ratio
  - `SRCSIZE` — renders at original pixel dimensions
  - `PRESCALED` — bakes a scaled surface once at first render, rebakes on window resize, then blits 1:1 every frame. Use this for backgrounds to avoid per-frame software scaling.
- **SpriteSheet** — Parses a texture atlas (text or XML format) and exposes named frame rects and animation sequences.
- **Text** — SDL3_ttf text rendering with optional background color. `CreateSurface` safely ignores empty strings.
- **UI / Button / Rectangle** — Basic interactive UI with hover detection and click callbacks.
- **ErrorHandling** — Lightweight SDL error checking helper.

---

## Building

### Prerequisites

All dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg).

**Install vcpkg** (if not already installed):
```bash
git clone https://github.com/microsoft/vcpkg ~/tools/vcpkg
cd ~/tools/vcpkg
./bootstrap-vcpkg.sh
```

**Install dependencies:**

Linux:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt
```

macOS Apple Silicon:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt --triplet arm64-osx
```

macOS Intel:
```bash
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt --triplet x64-osx
```

> **Note:** JPEG support is not included by default on macOS. Use PNG assets, or convert JPEGs with `sips -s format png input.jpg --out output.png`.

---

### Building with Presets

**Linux:**
```bash
cmake --preset linux
cmake --build --preset linux
./build/sdl-sandbox
```

**macOS Apple Silicon (M1/M2/M3):**
```bash
cmake --preset mac-arm
cmake --build --preset mac-arm
./build/sdl-sandbox
```

**macOS Intel:**
```bash
cmake --preset mac-intel
cmake --build --preset mac-intel
./build/sdl-sandbox
```

> **Important:** Always run the executable from the project root so asset paths resolve correctly.

---

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
sdl-sandbox/
├── src/                    # Implementation files (.cpp)
├── include/                # Header files
│   ├── systems/            # Individual ECS system headers
│   ├── Components.hpp      # All ECS component definitions and game constants
│   ├── Systems.hpp         # Aggregator — includes all system headers
│   ├── Scene.hpp           # Abstract scene base class
│   ├── SceneManager.hpp    # Owns and transitions between scenes
│   ├── GameScene.hpp       # Main gameplay scene
│   ├── TitleScene.hpp      # Opening title screen
│   ├── Image.hpp           # Image loading and rendering
│   ├── SpriteSheet.hpp     # Texture atlas parser
│   ├── Text.hpp            # TTF text rendering
│   ├── Window.hpp          # SDL window wrapper
│   └── SurfaceUtils.hpp    # Surface rotation helpers
├── game_assets/            # Sprites, backgrounds, tilesets
├── fonts/                  # TTF font files
└── CMakePresets.json       # Platform build presets
```

---

## Controls

| Key | Action |
|-----|--------|
| A / D | Move left / right (or up/down on side walls) |
| Space | Jump (gravity mode only) |
| Left Ctrl | Crouch — reduces hitbox, decelerates with friction |
| R | Retry after game over |

---

## Current Demo

The game opens on a title screen. Pressing ENTER or clicking Play drops into the gameplay scene where a player and 15 randomly-positioned slime enemies spawn.

**Free mode:** WASD to move with friction-based deceleration.

**Gravity mode:** Triggered by touching any screen wall. Gravity pulls the player toward that wall. The sprite rotates to appear upright relative to the gravity wall. Walking perpendicular to the wall, jumping, and crouching all work on any wall.

**Zero gravity punishment:** Taking a hit from an enemy releases the player into free-float mode for 15 seconds. A countdown HUD element displays the remaining time. `CenterPullSystem` gently pulls the player toward the screen center during this period. Gravity resumes automatically when the timer expires.

**Combat:** Each slime hit deals 15 damage with 1.5 seconds of invincibility. At 0 health a game over screen appears with a Retry button (click or press R) that respawns everything fresh.

---

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
