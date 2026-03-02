#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameConfig.hpp
//
// All game-design tuning values live here.  Nothing in this file is engine
// infrastructure — these are the knobs a designer would turn.
//
// Components.hpp, Systems.hpp and engine/ headers must NOT include this file.
// Only game-layer code (Scenes, game-specific systems) should include it.
// ─────────────────────────────────────────────────────────────────────────────

// ── Player stats ─────────────────────────────────────────────────────────────
inline constexpr float PLAYER_HIT_DAMAGE    = 15.0f;
inline constexpr float PLAYER_INVINCIBILITY = 1.5f;
inline constexpr float PLAYER_MAX_HEALTH    = 100.0f;

// ── Player sprite / frame dimensions (pixels) ────────────────────────────────
inline constexpr int PLAYER_SPRITE_WIDTH  = 72;
inline constexpr int PLAYER_SPRITE_HEIGHT = 97;

// ── Player collider dimensions ────────────────────────────────────────────────
inline constexpr int PLAYER_STAND_WIDTH  = 32;
inline constexpr int PLAYER_STAND_HEIGHT = 60;
inline constexpr int PLAYER_DUCK_WIDTH   = 55;
inline constexpr int PLAYER_DUCK_HEIGHT  = 32;

// ── Player render offsets ────────────────────────────────────────────────────
inline constexpr int PLAYER_STAND_ROFF_X = -24;
inline constexpr int PLAYER_DUCK_ROFF_X  =   5;

// ── Enemy sprite dimensions ───────────────────────────────────────────────────
inline constexpr int SLIME_SPRITE_WIDTH  = 36;
inline constexpr int SLIME_SPRITE_HEIGHT = 26;

// ── Physics ───────────────────────────────────────────────────────────────────
inline constexpr float GRAVITY_DURATION = 5.0f;    // seconds before gravity re-activates
inline constexpr float GRAVITY_FORCE    = 800.0f;  // pixels / sec²
inline constexpr float JUMP_FORCE       = 450.0f;  // pixels / sec  (upward impulse)
inline constexpr float MAX_FALL_SPEED   = 1000.0f; // terminal velocity
inline constexpr float PLAYER_SPEED     = 300.0f;  // lateral pixels / sec

// ── World / spawn counts ──────────────────────────────────────────────────────
inline constexpr int GRAVITYSLUGSCOUNT = 20;
inline constexpr int COIN_COUNT        = 8;
inline constexpr int COIN_SIZE         = 40;
