# SDL Sandbox

A 2D game engine foundation built on SDL2. This project is a personal sandbox for building and testing the core systems needed for a platformer engine — windowing, sprite animation, sprite sheets, UI components, and a delta-time game loop.

## Architecture Overview

The engine is organized around a small set of focused classes:

- **Window** — RAII wrapper around `SDL_Window`. Handles surface management, color mapping, and screenshotting.
- **Image** — Loads and blits a single image surface with configurable `FitMode` (CONTAIN, COVER, STRETCH, SRCSIZE). Supports horizontal flipping.
- **SpriteSheet** — Parses a texture atlas (text or XML format) and exposes individual frame rects and animation sequences by name.
- **Sprite** — Animated 2D entity. Supports frame-based animation from image strips or sprite sheets, keyboard-driven movement, scaling, state management, and delta-time updates.
- **UI / Button / Rectangle / SettingsMenu** — Basic interactive UI layer. Buttons respond to click events and can toggle settings panels.
- **ErrorHandling** — Lightweight SDL error checking macro and helper function.

## Building

```bash
cmake -B build
cmake --build build
./build/sdl-sandbox
```

Requires SDL2, SDL2_image, and SDL2_ttf.

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
├── game_assets/   # Sprites, backgrounds, fonts
├── fonts/         # TTF font files
├── docs/          # Generated Doxygen output
└── MyDoxyFile     # Doxygen configuration
```

## Current Demo

The main loop spawns 15 randomly-positioned slime enemies wandering a castle background, with a player character controlled by keyboard input. This serves as a stress test for the sprite and animation systems.

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
