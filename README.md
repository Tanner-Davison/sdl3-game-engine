# SDL Sandbox

A 2D game engine foundation built on SDL3 and EnTT. This project is a personal sandbox for building and testing the core systems needed for a platformer engine — windowing, sprite animation, sprite sheets, an Entity Component System, collision detection, scene management, and a delta-time game loop.

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
- `GravityState` — gravity direction, velocity, grounded state, and 60s timer
- `PlayerTag` — empty tag that marks the player entity

**Systems:**
- `InputSystem` — WASD/space input, handles free mode and gravity mode separately
- `MovementSystem` — velocity, friction, gravity force, jump boost, and gravity timer
- `CenterPullSystem` — pulls player back to screen center after gravity expires
- `BoundsSystem` — clamps player to screen edges, triggers gravity mode on wall contact
- `AnimationSystem` — advances animation frames using delta time
- `RenderSystem` — blits current frame to screen, handles flip and gravity-based sprite rotation
- `CollisionSystem` — AABB player vs enemy detection, applies damage and invincibility

### Scene System
Game states are organized as scenes that inherit from a `Scene` base class. `SceneManager` owns the active scene and handles transitions automatically when `NextScene()` returns a non-null value.

- `TitleScene` — opening screen with Play button, transitions to `GameScene`
- `GameScene` — main gameplay, owns the ECS registry and all assets

### Supporting Classes
- **Window** — RAII wrapper around `SDL_Window`. Handles surface management and renderer creation.
- **Image** — Loads and blits a single image surface with configurable `FitMode` (CONTAIN, COVER, STRETCH, SRCSIZE).
- **SpriteSheet** — Parses a texture atlas (text or XML format) and exposes frame rects and animation sequences by name.
- **UI / Button / Rectangle / SettingsMenu** — Basic interactive UI layer with hover detection and click callbacks.
- **Text / ScaledText** — SDL3_ttf text rendering with optional background color and auto-scaling to a target width.
- **ErrorHandling** — Lightweight SDL error checking helper.

## Building

Requires SDL3, SDL3_image, SDL3_ttf, and EnTT (managed via vcpkg).

```bash
cmake -B build
cmake --build build
./build/sdl-sandbox
```

## Project Structure

```
sdl-sandbox/
├── src/                 # Implementation files
│   └── TitleScene.cpp   # NextScene() defined here to avoid circular includes
├── include/             # Header files
│   ├── Components.hpp   # ECS component definitions and constants
│   ├── Systems.hpp      # ECS system functions
│   ├── Scene.hpp        # Abstract scene base class
│   ├── SceneManager.hpp # Owns and transitions between scenes
│   ├── TitleScene.hpp   # Opening title screen
│   └── GameScene.hpp    # Main gameplay scene
├── game_assets/         # Sprites, backgrounds, tilesets
├── fonts/               # TTF font files
└── MyDoxyFile           # Doxygen configuration
```

## Current Demo

The game opens on a title screen. Pressing ENTER or clicking Play drops into the gameplay scene where a player and 15 randomly-positioned slime enemies spawn on a castle background.

**Free mode:** WASD to move with floaty friction-based deceleration.

**Gravity mode:** Triggered by touching any screen wall. Gravity pulls the player toward whichever wall they hit. The player sprite rotates to always appear upright relative to their gravity wall. Perpendicular walking (along the wall) still works. Space bar jumps away from the wall with variable height based on how long space is held. Hitting a new wall mid-air switches gravity direction instantly. The gravity mode timer runs for 60 seconds from first wall contact and is never reset by subsequent wall hits. When it expires, the player is pulled back to screen center and free mode resumes.

**Combat:** Each slime hit deals 15 damage with 1.5 seconds of invincibility after each hit. At 0 health a game over screen appears with a clickable Retry button and R key shortcut that clears all entities and respawns everything fresh.

## Documentation

See `DOCUMENTATION.md` for a full breakdown of every component, system, and architectural decision.

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
