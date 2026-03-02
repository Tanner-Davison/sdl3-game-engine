#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameEvents.hpp
//
// Lightweight value types that systems return or emit instead of mutating
// out-parameters owned by a Scene.  Keeps systems self-contained and testable.
// ─────────────────────────────────────────────────────────────────────────────

// Returned by CollisionSystem every frame.
// The calling Scene accumulates these into its own state.
struct CollisionResult {
    bool playerDied      = false;
    int  coinsCollected  = 0;
    int  enemiesStomped  = 0;
};
