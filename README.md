# SDL Sandbox

A 2D game engine foundation built on SDL3 and EnTT. This project is a personal sandbox for building and testing the core systems needed for a platformer engine — windowing, sprite animation, sprite sheets, an Entity Component System, collision detection, and a delta-time game loop.

## Architecture Overview

### ECS (Entity Component System)
The core game simulation runs through EnTT. Components are plain data structs defined in `Components.hpp`, and systems are free functions in `Systems.hpp`.

**Components:**
- `Transform` — world position (x, y)
- `Velocity` — movement direction, speed, and friction
- `AnimationState` — current frame, timer, fps, looping
- `Renderable` — sprite sheet surface, frame rects, flip state
- `Health` — current and max health
- `Collider` — AABB width and height
- `InvincibilityTimer` — post-hit grace period
- `PlayerTag` — empty tag that marks the player entity

**Systems:**
- `InputSystem` — WASD movement and flip direction for the player
- `MovementSystem` — applies velocity to position, friction on key release
- `AnimationSystem` — advances animation frames using delta time
- `RenderSystem` — blits the current frame to the window surface
- `CollisionSystem` — AABB player vs enemy detection, applies damage and invincibility

### Supporting Classes
- **Window** — RAII wrapper around `SDL_Window`. Handles surface management and renderer creation.
- **Image** — Loads and blits a single image surface with configurable `FitMode` (CONTAIN, COVER, STRETCH, SRCSIZE). Supports horizontal flipping.
- **SpriteSheet** — Parses a texture atlas (text or XML format) and exposes individual frame rects and animation sequences by name.
- **UI / Button / Rectangle / SettingsMenu** — Basic interactive UI layer. Buttons respond to click events and can toggle settings panels.
- **Text / ScaledText** — SDL3_ttf text rendering with optional background color and auto-scaling to fit a target width.
- **ErrorHandling** — Lightweight SDL error checking helper.

## Building

Requires SDL3, SDL3_image, SDL3_ttf, and EnTT (managed via vcpkg).

```bash
cmake -B build
cmake --build build
./build/sdl-sandbox
```

## Generating Documentation

```bash
./generate_docs.sh
./view_documentation.sh
```

Or open `docs/html/index.html` directly in a browser.

## Project Structure

```
sdl-sandbox/
├── src/           # Implementation files
├── include/       # Header files
│   ├── Components.hpp   # ECS component definitions
│   └── Systems.hpp      # ECS system functions
├── game_assets/   # Sprites, backgrounds, tilesets
├── fonts/         # TTF font files
└── MyDoxyFile     # Doxygen configuration
```

## Current Demo

The main loop spawns a player and 15 randomly-positioned slime enemies on a castle background. The player is controlled with WASD and takes 15 damage per hit with 1.5 seconds of invincibility after each hit. At 0 health a game over screen appears with a clickable retry button and R key shortcut that resets all entities and respawns everything fresh.

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
